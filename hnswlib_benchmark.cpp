#include "ann_benchmark_common.h"

#include <hnswlib/hnswlib.h>
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
    std::vector<int> ef_search_values;
    int repeats = 5;
    int warmup_queries = 1000;
    int query_count = 0;
    int m = 16;
    int ef_construction = 200;
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
        } else if (key == "--ef-search") {
            options.ef_search_values = ann_bench::parse_list<int>(value);
        } else if (key == "--repeats") {
            options.repeats = std::stoi(value);
        } else if (key == "--warmup-queries") {
            options.warmup_queries = std::stoi(value);
        } else if (key == "--query-count") {
            options.query_count = std::stoi(value);
        } else if (key == "--m") {
            options.m = std::stoi(value);
        } else if (key == "--ef-construction") {
            options.ef_construction = std::stoi(value);
        } else {
            throw std::invalid_argument("unknown option: " + key);
        }
    }

    if (options.dataset.empty() || options.output.empty() || options.top_k_values.empty() ||
        options.thread_values.empty() || options.ef_search_values.empty()) {
        throw std::invalid_argument("dataset, output, top-k, threads and ef-search are required");
    }
    const auto all_positive = [](const std::vector<int>& values) {
        return std::all_of(values.begin(), values.end(), [](int value) { return value > 0; });
    };
    if (!all_positive(options.top_k_values) || !all_positive(options.thread_values) ||
        !all_positive(options.ef_search_values) || options.repeats <= 0 ||
        options.warmup_queries < 0 || options.query_count < 0 || options.m <= 0 ||
        options.ef_construction <= 0) {
        throw std::invalid_argument("numeric options are invalid");
    }
    return options;
}

void search_batch(
        const hnswlib::HierarchicalNSW<float>& index,
        const ann_bench::Vecs<float>& queries,
        size_t query_count,
        int top_k,
        int threads,
        std::vector<size_t>& labels) {
    const size_t expected_size = query_count * static_cast<size_t>(top_k);
    if (labels.size() != expected_size) {
        throw std::invalid_argument("label buffer has an unexpected size");
    }

    // hnswlib 的单查询接口是线程安全的；这里在查询维度做 OpenMP 并行，
    // 因而 threads 表示真正的并发查询线程数，而不是索引内部的隐藏线程数。
#pragma omp parallel for num_threads(threads) schedule(static)
    for (int64_t query = 0; query < static_cast<int64_t>(query_count); ++query) {
        size_t* output = labels.data() +
                static_cast<size_t>(query) * static_cast<size_t>(top_k);
        std::fill_n(output, static_cast<size_t>(top_k), std::numeric_limits<size_t>::max());
        const float* vector =
                queries.values.data() + static_cast<size_t>(query) * queries.columns;
        const auto result = index.searchKnnCloserFirst(vector, static_cast<size_t>(top_k));
        const size_t count = std::min(result.size(), static_cast<size_t>(top_k));
        for (size_t rank = 0; rank < count; ++rank) {
            output[rank] = result[rank].second;
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
        hnswlib::L2Space space(base.columns);
        hnswlib::HierarchicalNSW<float> index(
                &space,
                base.rows,
                static_cast<size_t>(options.m),
                static_cast<size_t>(options.ef_construction));

        // 索引构建不计入 QPS。addPoint 官方支持并发插入，使用测试线程数中的
        // 最大值缩短一次性建库时间；buildThreads 会写入 CSV，便于复现实验。
#pragma omp parallel for num_threads(build_threads) schedule(dynamic, 64)
        for (int64_t row = 0; row < static_cast<int64_t>(base.rows); ++row) {
            index.addPoint(
                    base.values.data() + static_cast<size_t>(row) * base.columns,
                    static_cast<hnswlib::labeltype>(row));
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
                "M=" + std::to_string(options.m) +
                ";efConstruction=" + std::to_string(options.ef_construction) +
                ";buildThreads=" + std::to_string(build_threads);

        for (const int threads : options.thread_values) {
            for (const int top_k : options.top_k_values) {
                for (const int ef_search : options.ef_search_values) {
                    if (ef_search < top_k) {
                        continue;
                    }
                    index.setEf(static_cast<size_t>(ef_search));
                    std::vector<size_t> labels(
                            warmup_count * static_cast<size_t>(top_k));
                    if (warmup_count > 0) {
                        search_batch(
                                index,
                                query,
                                warmup_count,
                                top_k,
                                threads,
                                labels);
                    }

                    // 在开始计时前分配结果数组，避免把内存首次分配混进 QPS。
                    labels.resize(query_count * static_cast<size_t>(top_k));
                    std::vector<double> qps_values;
                    qps_values.reserve(static_cast<size_t>(options.repeats));
                    for (int repeat = 0; repeat < options.repeats; ++repeat) {
                        const auto start = std::chrono::steady_clock::now();
                        search_batch(
                                index,
                                query,
                                query_count,
                                top_k,
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
                    output << "hnswlib-hnsw,hnswlib," << top_k << ',' << threads
                           << ",efSearch," << ef_search << ',' << recall << ',' << qps << ','
                           << options.repeats << ',' << warmup_count << ',' << base.rows << ','
                           << query_count << ',' << query.columns << ',' << index_parameters
                           << ",ok,\n";
                    output.flush();
                    std::cout << "hnswlib-hnsw k=" << top_k << " threads=" << threads
                              << " efSearch=" << ef_search << " recall=" << recall
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
