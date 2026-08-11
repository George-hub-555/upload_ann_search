#include <faiss/IndexFlat.h>
#include <faiss/IndexHNSW.h>
#include <faiss/IndexIVFPQ.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace {

struct Options {
    std::filesystem::path dataset = "dataset";
    std::filesystem::path output = "results/raw/faiss.csv";
    std::vector<int> top_k_values{10, 100};
    std::vector<int> thread_values{1, 16};
    std::vector<int> ef_search_values{10, 15, 20, 30, 40, 50, 75, 100, 150, 200, 300, 400, 800};
    std::vector<int> nprobe_values{1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024};
    int repeats = 5;
    int warmup_queries = 1000;
    int query_count = 0;
    int hnsw_m = 16;
    int hnsw_ef_construction = 100;
    int ivfpq_nlist = 1024;
    int ivfpq_m = 16;
    int ivfpq_nbits = 8;
};

template <typename T>
struct Vecs {
    std::vector<T> values;
    size_t rows = 0;
    size_t columns = 0;
};

std::vector<int> parse_int_list(const std::string& text) {
    std::vector<int> result;
    std::stringstream stream(text);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty()) {
            result.push_back(std::stoi(item));
        }
    }
    if (result.empty()) {
        throw std::invalid_argument("integer list must not be empty");
    }
    return result;
}

void print_help(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --dataset PATH             default: dataset\n"
        << "  --output PATH              default: results/raw/faiss.csv\n"
        << "  --top-k LIST               default: 10,100\n"
        << "  --threads LIST             default: 1,16\n"
        << "  --ef-search LIST           HNSW sweep\n"
        << "  --nprobe LIST              IVFPQ sweep\n"
        << "  --repeats N                default: 5\n"
        << "  --warmup-queries N         default: 1000\n"
        << "  --query-count N            0 means all queries\n"
        << "  --hnsw-m N                 default: 16\n"
        << "  --hnsw-ef-construction N   default: 100\n"
        << "  --ivfpq-nlist N            default: 1024\n"
        << "  --ivfpq-m N                default: 16\n"
        << "  --ivfpq-nbits N            default: 8\n";
}

Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string key = argv[i];
        if (key == "--help" || key == "-h") {
            print_help(argv[0]);
            std::exit(0);
        }
        if (i + 1 >= argc) {
            throw std::invalid_argument("missing value after " + key);
        }
        const std::string value = argv[++i];
        if (key == "--dataset") {
            options.dataset = value;
        } else if (key == "--output") {
            options.output = value;
        } else if (key == "--top-k") {
            options.top_k_values = parse_int_list(value);
        } else if (key == "--threads") {
            options.thread_values = parse_int_list(value);
        } else if (key == "--ef-search") {
            options.ef_search_values = parse_int_list(value);
        } else if (key == "--nprobe") {
            options.nprobe_values = parse_int_list(value);
        } else if (key == "--repeats") {
            options.repeats = std::stoi(value);
        } else if (key == "--warmup-queries") {
            options.warmup_queries = std::stoi(value);
        } else if (key == "--query-count") {
            options.query_count = std::stoi(value);
        } else if (key == "--hnsw-m") {
            options.hnsw_m = std::stoi(value);
        } else if (key == "--hnsw-ef-construction") {
            options.hnsw_ef_construction = std::stoi(value);
        } else if (key == "--ivfpq-nlist") {
            options.ivfpq_nlist = std::stoi(value);
        } else if (key == "--ivfpq-m") {
            options.ivfpq_m = std::stoi(value);
        } else if (key == "--ivfpq-nbits") {
            options.ivfpq_nbits = std::stoi(value);
        } else {
            throw std::invalid_argument("unknown option: " + key);
        }
    }

    const auto positive = [](const std::vector<int>& values) {
        return std::all_of(values.begin(), values.end(), [](int v) { return v > 0; });
    };
    if (!positive(options.top_k_values) || !positive(options.thread_values) ||
        !positive(options.ef_search_values) || !positive(options.nprobe_values) ||
        options.repeats <= 0 || options.warmup_queries < 0 || options.query_count < 0 ||
        options.hnsw_m <= 0 || options.hnsw_ef_construction <= 0 ||
        options.ivfpq_nlist <= 0 || options.ivfpq_m <= 0 || options.ivfpq_nbits <= 0) {
        throw std::invalid_argument("all numeric options must be positive (query-count and warmup may be zero)");
    }
    return options;
}

template <typename T>
Vecs<T> read_vecs(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open " + path.string());
    }

    Vecs<T> result;
    int32_t columns = 0;
    while (input.read(reinterpret_cast<char*>(&columns), sizeof(columns))) {
        if (columns <= 0) {
            throw std::runtime_error("invalid vector dimension in " + path.string());
        }
        if (result.columns == 0) {
            result.columns = static_cast<size_t>(columns);
        } else if (result.columns != static_cast<size_t>(columns)) {
            throw std::runtime_error("inconsistent vector dimension in " + path.string());
        }
        const size_t old_size = result.values.size();
        result.values.resize(old_size + result.columns);
        input.read(
            reinterpret_cast<char*>(result.values.data() + old_size),
            static_cast<std::streamsize>(result.columns * sizeof(T)));
        if (!input) {
            throw std::runtime_error("truncated vector file " + path.string());
        }
        ++result.rows;
    }
    if (result.rows == 0) {
        throw std::runtime_error("empty vector file " + path.string());
    }
    return result;
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const size_t middle = values.size() / 2;
    if (values.size() % 2 != 0) {
        return values[middle];
    }
    return (values[middle - 1] + values[middle]) / 2.0;
}

double recall_at_k(
        const std::vector<faiss::idx_t>& labels,
        const Vecs<int32_t>& ground_truth,
        size_t query_count,
        int top_k) {
    size_t matches = 0;
    for (size_t query = 0; query < query_count; ++query) {
        std::unordered_set<faiss::idx_t> returned;
        returned.reserve(static_cast<size_t>(top_k) * 2);
        for (int rank = 0; rank < top_k; ++rank) {
            returned.insert(labels[query * top_k + rank]);
        }
        for (int rank = 0; rank < top_k; ++rank) {
            if (returned.count(ground_truth.values[query * ground_truth.columns + rank]) != 0) {
                ++matches;
            }
        }
    }
    return static_cast<double>(matches) /
           static_cast<double>(query_count * static_cast<size_t>(top_k));
}

void write_header(std::ofstream& output) {
    output
        << "algorithm,implementation,top_k,threads,search_parameter,search_value,"
        << "recall_at_k,qps,repeat_count,warmup_queries,base_count,query_count,dimension,"
        << "index_parameters,status,note\n";
}

template <typename Configure>
void benchmark_sweep(
        faiss::Index& index,
        const std::string& algorithm,
        const std::string& parameter_name,
        const std::vector<int>& parameter_values,
        const std::string& index_parameters,
        const Options& options,
        const Vecs<float>& queries,
        const Vecs<int32_t>& ground_truth,
        size_t base_count,
        std::ofstream& output,
        Configure configure) {
    const size_t query_count = options.query_count == 0
        ? queries.rows
        : std::min(queries.rows, static_cast<size_t>(options.query_count));
    const size_t warmup_count = std::min(
        query_count, static_cast<size_t>(options.warmup_queries));

    for (const int threads : options.thread_values) {
#ifdef _OPENMP
        omp_set_num_threads(threads);
#endif
        for (const int top_k : options.top_k_values) {
            if (static_cast<size_t>(top_k) > ground_truth.columns) {
                throw std::runtime_error("top-k exceeds ground-truth width");
            }
            for (const int parameter : parameter_values) {
                if (algorithm == "faiss-hnsw" && parameter < top_k) {
                    continue;
                }
                configure(parameter);

                if (warmup_count > 0) {
                    std::vector<float> warmup_distances(warmup_count * top_k);
                    std::vector<faiss::idx_t> warmup_labels(warmup_count * top_k);
                    index.search(
                        static_cast<faiss::idx_t>(warmup_count),
                        queries.values.data(),
                        top_k,
                        warmup_distances.data(),
                        warmup_labels.data());
                }

                std::vector<double> qps_values;
                std::vector<faiss::idx_t> labels(query_count * top_k);
                std::vector<float> distances(query_count * top_k);
                for (int repeat = 0; repeat < options.repeats; ++repeat) {
                    const auto start = std::chrono::steady_clock::now();
                    index.search(
                        static_cast<faiss::idx_t>(query_count),
                        queries.values.data(),
                        top_k,
                        distances.data(),
                        labels.data());
                    const auto stop = std::chrono::steady_clock::now();
                    const double seconds = std::chrono::duration<double>(stop - start).count();
                    qps_values.push_back(static_cast<double>(query_count) / seconds);
                }

                const double recall = recall_at_k(labels, ground_truth, query_count, top_k);
                const double qps = median(qps_values);
                output << algorithm << ",Faiss," << top_k << ',' << threads << ','
                       << parameter_name << ',' << parameter << ',' << recall << ',' << qps << ','
                       << options.repeats << ',' << warmup_count << ',' << base_count << ','
                       << query_count << ',' << queries.columns << ',' << index_parameters
                       << ",ok,\n";
                output.flush();
                std::cout << algorithm << " k=" << top_k << " threads=" << threads << ' '
                          << parameter_name << '=' << parameter << " recall=" << recall
                          << " qps=" << qps << std::endl;
            }
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_options(argc, argv);
        const Vecs<float> base = read_vecs<float>(options.dataset / "sift_base.fvecs");
        const Vecs<float> query = read_vecs<float>(options.dataset / "sift_query.fvecs");
        const Vecs<float> learn = read_vecs<float>(options.dataset / "sift_learn.fvecs");
        const Vecs<int32_t> ground_truth =
            read_vecs<int32_t>(options.dataset / "sift_groundtruth.ivecs");

        if (base.columns != query.columns || base.columns != learn.columns) {
            throw std::runtime_error("base/query/learn dimensions differ");
        }
        if (ground_truth.rows < query.rows) {
            throw std::runtime_error("ground truth contains fewer rows than queries");
        }
        if (base.columns % static_cast<size_t>(options.ivfpq_m) != 0) {
            throw std::runtime_error("IVFPQ M must divide the vector dimension");
        }

        if (!options.output.parent_path().empty()) {
            std::filesystem::create_directories(options.output.parent_path());
        }
        std::ofstream output(options.output, std::ios::trunc);
        if (!output) {
            throw std::runtime_error("cannot create " + options.output.string());
        }
        write_header(output);

#ifdef _OPENMP
        omp_set_num_threads(*std::max_element(options.thread_values.begin(), options.thread_values.end()));
#endif
        {
            faiss::IndexHNSWFlat index(static_cast<faiss::idx_t>(base.columns), options.hnsw_m);
            index.hnsw.efConstruction = options.hnsw_ef_construction;
            index.add(static_cast<faiss::idx_t>(base.rows), base.values.data());
            const std::string parameters =
                "M=" + std::to_string(options.hnsw_m) +
                ";efConstruction=" + std::to_string(options.hnsw_ef_construction);
            benchmark_sweep(
                index,
                "faiss-hnsw",
                "efSearch",
                options.ef_search_values,
                parameters,
                options,
                query,
                ground_truth,
                base.rows,
                output,
                [&](int value) { index.hnsw.efSearch = value; });
        }

        {
            faiss::IndexFlatL2 quantizer(static_cast<faiss::idx_t>(base.columns));
            faiss::IndexIVFPQ index(
                &quantizer,
                static_cast<faiss::idx_t>(base.columns),
                options.ivfpq_nlist,
                options.ivfpq_m,
                options.ivfpq_nbits);
            index.train(static_cast<faiss::idx_t>(learn.rows), learn.values.data());
            index.add(static_cast<faiss::idx_t>(base.rows), base.values.data());
            const std::string parameters =
                "nlist=" + std::to_string(options.ivfpq_nlist) +
                ";M=" + std::to_string(options.ivfpq_m) +
                ";nbits=" + std::to_string(options.ivfpq_nbits) +
                ";train=" + std::to_string(learn.rows);
            benchmark_sweep(
                index,
                "faiss-ivfpq",
                "nprobe",
                options.nprobe_values,
                parameters,
                options,
                query,
                ground_truth,
                base.rows,
                output,
                [&](int value) { index.nprobe = static_cast<size_t>(value); });
        }

        std::cout << "Faiss CSV written to " << options.output << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << std::endl;
        return 1;
    }
}
