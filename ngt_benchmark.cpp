#include "ann_benchmark_common.h"

#include <NGT/Index.h>
#include <omp.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Options {
    std::filesystem::path dataset;
    std::filesystem::path output;
    std::vector<int> top_k_values;
    std::vector<int> thread_values;
    std::vector<double> epsilon_values;
    int repeats = 5;
    int warmup_queries = 1000;
    int query_count = 0;
    int edge_size_for_creation = 40;
    int edge_size_for_search = 0;
};

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) {
            throw std::invalid_argument("missing value for " + std::string(argv[i]));
        }
        const std::string key = argv[i];
        const std::string value = argv[i + 1];
        if (key == "--dataset") {
            options.dataset = value;
        } else if (key == "--output") {
            options.output = value;
        } else if (key == "--top-k") {
            options.top_k_values = ann_bench::parse_list<int>(value);
        } else if (key == "--threads") {
            options.thread_values = ann_bench::parse_list<int>(value);
        } else if (key == "--epsilon") {
            options.epsilon_values = ann_bench::parse_list<double>(value);
        } else if (key == "--repeats") {
            options.repeats = std::stoi(value);
        } else if (key == "--warmup-queries") {
            options.warmup_queries = std::stoi(value);
        } else if (key == "--query-count") {
            options.query_count = std::stoi(value);
        } else if (key == "--edge-size-for-creation") {
            options.edge_size_for_creation = std::stoi(value);
        } else if (key == "--edge-size-for-search") {
            options.edge_size_for_search = std::stoi(value);
        } else {
            throw std::invalid_argument("unknown option: " + key);
        }
    }

    if (options.dataset.empty() || options.output.empty() || options.top_k_values.empty() ||
        options.thread_values.empty() || options.epsilon_values.empty()) {
        throw std::invalid_argument("dataset, output, top-k, threads and epsilon are required");
    }
    const auto all_positive = [](const std::vector<int>& values) {
        return std::all_of(values.begin(), values.end(), [](int value) { return value > 0; });
    };
    if (!all_positive(options.top_k_values) || !all_positive(options.thread_values) ||
        options.repeats <= 0 || options.warmup_queries < 0 || options.query_count < 0 ||
        options.edge_size_for_creation <= 0 || options.edge_size_for_search < 0) {
        throw std::invalid_argument("numeric options are invalid");
    }
    return options;
}

void search_batch(
        NGT::Index& index,
        const std::vector<std::vector<float>>& queries,
        size_t query_count,
        int top_k,
        double epsilon,
        int edge_size_for_search,
        int threads,
        std::vector<size_t>& labels) {
    const size_t expected_size = query_count * static_cast<size_t>(top_k);
    if (labels.size() != expected_size) {
        throw std::invalid_argument("label buffer has an unexpected size");
    }

    // NGT 的接口一次搜索一条向量，因此和 hnswlib 一样在查询维度并行。
#pragma omp parallel for num_threads(threads) schedule(static)
    for (int64_t query = 0; query < static_cast<int64_t>(query_count); ++query) {
        size_t* output = labels.data() +
                static_cast<size_t>(query) * static_cast<size_t>(top_k);
        std::fill_n(output, static_cast<size_t>(top_k), std::numeric_limits<size_t>::max());
        NGT::SearchQuery search_query(queries[static_cast<size_t>(query)]);
        NGT::ObjectDistances objects;
        search_query.setResults(&objects);
        search_query.setSize(static_cast<size_t>(top_k));
        search_query.setEpsilon(static_cast<float>(epsilon));
        search_query.setEdgeSize(edge_size_for_search);
        index.search(search_query);

        const size_t count = std::min(objects.size(), static_cast<size_t>(top_k));
        for (size_t rank = 0; rank < count; ++rank) {
            if (objects[rank].id > 0) {
                // NGT 保留 0 作为空 ID，首条向量 ID 为 1；SIFT ground truth
                // 从 0 编号，所以统一结果时必须减 1。
                output[rank] = static_cast<size_t>(objects[rank].id - 1);
            }
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        const auto base =
                ann_bench::read_vecs<float>(options.dataset / "sift_base.fvecs");
        const auto query =
                ann_bench::read_vecs<float>(options.dataset / "sift_query.fvecs");
        const auto ground_truth = ann_bench::read_vecs<int32_t>(
                options.dataset / "sift_groundtruth.ivecs");
        const size_t query_count = options.query_count == 0
                ? query.rows
                : std::min(query.rows, static_cast<size_t>(options.query_count));
        ann_bench::validate_dataset(
                base, query, ground_truth, options.top_k_values, query_count);

        const int build_threads =
                *std::max_element(options.thread_values.begin(), options.thread_values.end());
        NGT::Property property;
        property.dimension = static_cast<int>(base.columns);
        property.objectType = NGT::ObjectSpace::ObjectType::Float;
        property.distanceType = NGT::Index::Property::DistanceType::DistanceTypeL2;
        property.indexType = NGT::Index::Property::GraphAndTree;
        property.threadPoolSize = build_threads;
        property.edgeSizeForCreation = options.edge_size_for_creation;
        property.edgeSizeForSearch = options.edge_size_for_search;

        // 使用只存在于当前进程内的索引，避免产生绝对路径或在 A/B 两机之间
        // 搬运 NGT 索引文件。建库不在下面的查询计时区间内。
        NGT::Index index(property);
        std::vector<float> object(base.columns);
        for (size_t row = 0; row < base.rows; ++row) {
            std::copy_n(
                    base.values.data() + row * base.columns,
                    base.columns,
                    object.begin());
            const size_t object_id = index.append(object);
            if (object_id != row + 1) {
                throw std::runtime_error("NGT object IDs are not contiguous and one-based");
            }
        }
        index.createIndex(static_cast<size_t>(build_threads));

        std::vector<std::vector<float>> query_vectors(
                query_count, std::vector<float>(query.columns));
        for (size_t row = 0; row < query_count; ++row) {
            std::copy_n(
                    query.values.data() + row * query.columns,
                    query.columns,
                    query_vectors[row].begin());
        }

        if (!options.output.parent_path().empty()) {
            std::filesystem::create_directories(options.output.parent_path());
        }
        std::ofstream output(options.output, std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot create " + options.output.string());
        }
        ann_bench::write_header(output);

        const size_t warmup_count = std::min(
                query_count, static_cast<size_t>(options.warmup_queries));
        const std::string index_parameters =
                "indexType=GraphAndTree;edgeSizeForCreation=" +
                std::to_string(options.edge_size_for_creation) +
                ";edgeSizeForSearch=" + std::to_string(options.edge_size_for_search) +
                ";buildThreads=" + std::to_string(build_threads);

        for (const int threads : options.thread_values) {
            for (const int top_k : options.top_k_values) {
                for (const double epsilon : options.epsilon_values) {
                    std::vector<size_t> labels(
                            warmup_count * static_cast<size_t>(top_k));
                    if (warmup_count > 0) {
                        search_batch(
                                index,
                                query_vectors,
                                warmup_count,
                                top_k,
                                epsilon,
                                options.edge_size_for_search,
                                threads,
                                labels);
                    }

                    // 结果数组先分配再开始计时，QPS 只覆盖真实查询调用。
                    labels.resize(query_count * static_cast<size_t>(top_k));
                    std::vector<double> qps_values;
                    qps_values.reserve(static_cast<size_t>(options.repeats));
                    for (int repeat = 0; repeat < options.repeats; ++repeat) {
                        const auto start = std::chrono::steady_clock::now();
                        search_batch(
                                index,
                                query_vectors,
                                query_count,
                                top_k,
                                epsilon,
                                options.edge_size_for_search,
                                threads,
                                labels);
                        const auto stop = std::chrono::steady_clock::now();
                        const double seconds =
                                std::chrono::duration<double>(stop - start).count();
                        qps_values.push_back(static_cast<double>(query_count) / seconds);
                    }

                    const double recall = ann_bench::recall_at_k(
                            labels, ground_truth, query_count, top_k);
                    const double qps = ann_bench::median(qps_values);
                    output << "ngt,NGT," << top_k << ',' << threads << ",epsilon,"
                           << epsilon << ',' << recall << ',' << qps << ',' << options.repeats
                           << ',' << warmup_count << ',' << base.rows << ',' << query_count << ','
                           << query.columns << ',' << index_parameters << ",ok,\n";
                    output.flush();
                    std::cout << "ngt k=" << top_k << " threads=" << threads
                              << " epsilon=" << epsilon << " recall=" << recall
                              << " qps=" << qps << std::endl;
                }
            }
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << std::endl;
        return 1;
    }
}
