/*
 * bench_sift_custom.cpp - FAISS SIFT1M QPS / Recall benchmark for ARM
 *
 * Compile from the Faiss source root on ARM Linux:
 *   g++ -O3 -march=armv8-a -std=c++17 -I. \
 *       -o bench_sift_custom bench_sift_custom.cpp \
 *       -Lbuild/faiss -lfaiss -lopenblas -fopenmp -lpthread \
 *       -Wl,-rpath,build/faiss
 *
 * Run:
 *   ./bench_sift_custom
 *   ./bench_sift_custom -dataset dataset -k 10
 *   ./bench_sift_custom -indexes ivfflat,ivfpq -maxtrn 2000
 *   ./bench_sift_custom -maxtrn 10000 -reportfreq 1000
 *
 * The dataset directory must contain:
 *   sift_learn.fvecs
 *   sift_base.fvecs
 *   sift_query.fvecs
 *   sift_groundtruth.ivecs
 */

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <faiss/IndexFlat.h>
#include <faiss/IndexHNSW.h>
#include <faiss/IndexIVFFlat.h>
#include <faiss/IndexIVFPQ.h>

using idx_t = faiss::idx_t;

namespace {

struct FloatVectors {
    size_t dimension = 0;
    size_t count = 0;
    std::vector<float> values;
};

struct IntVectors {
    size_t dimension = 0;
    size_t count = 0;
    std::vector<int32_t> values;
};

struct BenchResult {
    std::string index_name;
    std::string params;
    double recall;
    double qps;
    double build_time;
};

double elapsed_seconds() {
    using Clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(
                   Clock::now().time_since_epoch())
            .count();
}

const char* arg_get_str(
        int argc,
        char** argv,
        const char* flag,
        const char* default_value) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], flag) == 0) {
            return argv[i + 1];
        }
    }
    return default_value;
}

bool arg_has(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], flag) == 0) {
            return true;
        }
    }
    return false;
}

size_t parse_size_arg(
        int argc,
        char** argv,
        const char* flag,
        size_t default_value) {
    const char* text = arg_get_str(argc, argv, flag, nullptr);
    if (!text) {
        return default_value;
    }

    if (text[0] == '-') {
        throw std::runtime_error(
                std::string("invalid value for ") + flag + ": " + text);
    }
    errno = 0;
    char* end = nullptr;
    unsigned long long value = std::strtoull(text, &end, 10);
    if (text[0] == '\0' || errno == ERANGE || (end && *end != '\0') ||
        value > std::numeric_limits<size_t>::max()) {
        throw std::runtime_error(
                std::string("invalid value for ") + flag + ": " + text);
    }
    return static_cast<size_t>(value);
}

int parse_int_arg(
        int argc,
        char** argv,
        const char* flag,
        int default_value) {
    size_t value =
            parse_size_arg(argc, argv, flag, static_cast<size_t>(default_value));
    if (value > static_cast<size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error(
                std::string("value is too large for ") + flag);
    }
    return static_cast<int>(value);
}

std::string join_path(const std::string& directory, const char* filename) {
    if (directory.empty()) {
        return filename;
    }
    const char last = directory.back();
    if (last == '/' || last == '\\') {
        return directory + filename;
    }
    return directory + "/" + filename;
}

uint64_t file_size(std::ifstream& input, const std::string& path) {
    input.seekg(0, std::ios::end);
    const std::streampos end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("cannot determine file size: " + path);
    }
    input.seekg(0, std::ios::beg);
    return static_cast<uint64_t>(end);
}

template <typename T>
void checked_read(
        std::ifstream& input,
        T* destination,
        size_t item_count,
        const std::string& path) {
    const size_t bytes = item_count * sizeof(T);
    input.read(
            reinterpret_cast<char*>(destination),
            static_cast<std::streamsize>(bytes));
    if (!input || static_cast<size_t>(input.gcount()) != bytes) {
        throw std::runtime_error("short or failed read from: " + path);
    }
}

FloatVectors read_fvecs(const std::string& path, size_t max_vectors = 0) {
    static_assert(sizeof(float) == 4, "fvecs requires 32-bit float");
    static_assert(sizeof(int32_t) == 4, "fvecs requires 32-bit int");

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open: " + path);
    }
    const uint64_t bytes = file_size(input, path);

    int32_t dimension = 0;
    checked_read(input, &dimension, 1, path);
    if (dimension <= 0 || dimension > 1000000) {
        throw std::runtime_error("invalid vector dimension in: " + path);
    }

    const uint64_t row_bytes =
            (static_cast<uint64_t>(dimension) + 1) * sizeof(float);
    if (bytes == 0 || bytes % row_bytes != 0) {
        throw std::runtime_error("invalid fvecs file size: " + path);
    }

    size_t count = static_cast<size_t>(bytes / row_bytes);
    if (max_vectors > 0) {
        count = std::min(count, max_vectors);
    }
    input.seekg(0, std::ios::beg);

    FloatVectors result;
    result.dimension = static_cast<size_t>(dimension);
    result.count = count;
    result.values.resize(count * result.dimension);

    // Read in chunks to avoid one fread call per vector and avoid retaining the
    // 4-byte dimension header for every vector.
    constexpr size_t kRowsPerChunk = 4096;
    const size_t row_floats = result.dimension + 1;
    std::vector<float> chunk(kRowsPerChunk * row_floats);

    size_t rows_done = 0;
    while (rows_done < count) {
        const size_t rows = std::min(kRowsPerChunk, count - rows_done);
        checked_read(input, chunk.data(), rows * row_floats, path);

        for (size_t row = 0; row < rows; ++row) {
            int32_t row_dimension = 0;
            std::memcpy(
                    &row_dimension,
                    chunk.data() + row * row_floats,
                    sizeof(row_dimension));
            if (row_dimension != dimension) {
                throw std::runtime_error(
                        "inconsistent vector dimension in: " + path);
            }
            std::memcpy(
                    result.values.data() +
                            (rows_done + row) * result.dimension,
                    chunk.data() + row * row_floats + 1,
                    result.dimension * sizeof(float));
        }
        rows_done += rows;
    }
    return result;
}

IntVectors read_ivecs(const std::string& path, size_t max_vectors = 0) {
    static_assert(sizeof(int32_t) == 4, "ivecs requires 32-bit int");

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open: " + path);
    }
    const uint64_t bytes = file_size(input, path);

    int32_t dimension = 0;
    checked_read(input, &dimension, 1, path);
    if (dimension <= 0 || dimension > 1000000) {
        throw std::runtime_error("invalid vector dimension in: " + path);
    }

    const uint64_t row_bytes =
            (static_cast<uint64_t>(dimension) + 1) * sizeof(int32_t);
    if (bytes == 0 || bytes % row_bytes != 0) {
        throw std::runtime_error("invalid ivecs file size: " + path);
    }

    size_t count = static_cast<size_t>(bytes / row_bytes);
    if (max_vectors > 0) {
        count = std::min(count, max_vectors);
    }
    input.seekg(0, std::ios::beg);

    IntVectors result;
    result.dimension = static_cast<size_t>(dimension);
    result.count = count;
    result.values.resize(count * result.dimension);

    constexpr size_t kRowsPerChunk = 4096;
    const size_t row_ints = result.dimension + 1;
    std::vector<int32_t> chunk(kRowsPerChunk * row_ints);

    size_t rows_done = 0;
    while (rows_done < count) {
        const size_t rows = std::min(kRowsPerChunk, count - rows_done);
        checked_read(input, chunk.data(), rows * row_ints, path);

        for (size_t row = 0; row < rows; ++row) {
            if (chunk[row * row_ints] != dimension) {
                throw std::runtime_error(
                        "inconsistent vector dimension in: " + path);
            }
            std::memcpy(
                    result.values.data() +
                            (rows_done + row) * result.dimension,
                    chunk.data() + row * row_ints + 1,
                    result.dimension * sizeof(int32_t));
        }
        rows_done += rows;
    }
    return result;
}

std::unordered_set<std::string> parse_indexes(const std::string& text) {
    std::unordered_set<std::string> indexes;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t comma = text.find(',', start);
        std::string name = text.substr(
                start,
                comma == std::string::npos ? std::string::npos
                                           : comma - start);
        std::transform(
                name.begin(), name.end(), name.begin(), [](unsigned char c) {
                    return static_cast<char>(std::tolower(c));
                });
        if (name == "all") {
            indexes.insert("flat");
            indexes.insert("ivfflat");
            indexes.insert("ivfpq");
            indexes.insert("hnsw");
        } else if (
                name == "flat" || name == "ivfflat" || name == "ivfpq" ||
                name == "hnsw") {
            indexes.insert(name);
        } else if (!name.empty()) {
            throw std::runtime_error("unknown index name: " + name);
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    if (indexes.empty()) {
        throw std::runtime_error("-indexes must select at least one index");
    }
    return indexes;
}

// Set-overlap Recall@k. Result order does not affect the score.
double compute_recall_at_k(
        const idx_t* results,
        const int32_t* ground_truth,
        size_t query_count,
        size_t result_k,
        size_t ground_truth_k) {
    size_t matches = 0;
    for (size_t query = 0; query < query_count; ++query) {
        const idx_t* result_row = results + query * result_k;
        const int32_t* gt_row = ground_truth + query * ground_truth_k;
        for (size_t i = 0; i < result_k; ++i) {
            for (size_t j = 0; j < result_k; ++j) {
                if (result_row[i] == static_cast<idx_t>(gt_row[j])) {
                    ++matches;
                    break;
                }
            }
        }
    }
    return static_cast<double>(matches) /
            static_cast<double>(query_count * result_k);
}

std::string safe_filename_tag(const std::string& text) {
    std::string tag = text;
    for (char& c : tag) {
        const bool valid =
                (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '-' || c == '_';
        if (!valid) {
            c = '_';
        }
    }
    return tag;
}

void print_help(const char* program) {
    std::printf("Usage: %s [flags]\n", program);
    std::printf(
            "  -dataset     <dir>   SIFT data directory (default: dataset)\n");
    std::printf(
            "  -indexes     <list>  all or comma-separated flat,ivfflat,"
            "ivfpq,hnsw (default: all)\n");
    std::printf(
            "  -k           <int>   search/recall k (default: 10)\n");
    std::printf(
            "  -maxtrn      <int>   maximum query rounds (default: all)\n");
    std::printf(
            "  -reportfreq  <int>   print local statistics every N rounds "
            "(default: 0 = off)\n");
    std::printf(
            "  -nlist       <int>   IVF cluster count (default: 4096)\n");
    std::printf(
            "  -pq_m        <int>   IVFPQ subquantizer count (default: 16)\n");
    std::printf(
            "  -hnsw_m      <int>   HNSW links per node (default: 32)\n");
    std::printf(
            "  -output      <path>  result CSV (default: "
            "bench_sift_results.csv)\n");
}

int benchmark_main(int argc, char** argv) {
    if (arg_has(argc, argv, "-h") || arg_has(argc, argv, "--help")) {
        print_help(argv[0]);
        return 0;
    }

    const std::string dataset_dir =
            arg_get_str(argc, argv, "-dataset", "dataset");
    const std::string output_path =
            arg_get_str(argc, argv, "-output", "bench_sift_results.csv");
    const std::string index_text =
            arg_get_str(argc, argv, "-indexes", "all");
    const auto selected_indexes = parse_indexes(index_text);
    const int k = parse_int_arg(argc, argv, "-k", 10);
    // -maxquery is retained as a compatibility alias. -maxtrn takes
    // precedence when both are supplied.
    const size_t max_query_compat =
            parse_size_arg(argc, argv, "-maxquery", 0);
    const size_t max_trn =
            parse_size_arg(argc, argv, "-maxtrn", max_query_compat);
    const int report_freq =
            parse_int_arg(argc, argv, "-reportfreq", 0);
    const int nlist = parse_int_arg(argc, argv, "-nlist", 4096);
    const int pq_m = parse_int_arg(argc, argv, "-pq_m", 16);
    const int hnsw_m = parse_int_arg(argc, argv, "-hnsw_m", 32);

    if (k <= 0 || nlist <= 0 || pq_m <= 0 || hnsw_m <= 0) {
        throw std::runtime_error(
                "-k, -nlist, -pq_m and -hnsw_m must be positive");
    }

    const double program_start = elapsed_seconds();
    std::printf("============================================================\n");
    std::printf("FAISS SIFT QPS / Recall Benchmark\n");
    std::printf(
            "  dataset=%s  indexes=%s  topk=%d  maxtrn=%zu\n",
            dataset_dir.c_str(),
            index_text.c_str(),
            k,
            max_trn);
    std::printf(
            "  nlist=%d  pq_m=%d  hnsw_m=%d  reportfreq=%d\n",
            nlist,
            pq_m,
            hnsw_m,
            report_freq);
    std::printf("============================================================\n");

    const std::string train_path =
            join_path(dataset_dir, "sift_learn.fvecs");
    const std::string base_path =
            join_path(dataset_dir, "sift_base.fvecs");
    const std::string query_path =
            join_path(dataset_dir, "sift_query.fvecs");
    const std::string gt_path =
            join_path(dataset_dir, "sift_groundtruth.ivecs");

    std::printf("[%.3f s] Loading training vectors: %s\n",
                elapsed_seconds() - program_start,
                train_path.c_str());
    FloatVectors train = read_fvecs(train_path);

    std::printf("[%.3f s] Loading base vectors: %s\n",
                elapsed_seconds() - program_start,
                base_path.c_str());
    FloatVectors base = read_fvecs(base_path);

    std::printf("[%.3f s] Loading query vectors: %s\n",
                elapsed_seconds() - program_start,
                query_path.c_str());
    FloatVectors query = read_fvecs(query_path, max_trn);

    std::printf("[%.3f s] Loading ground truth: %s\n",
                elapsed_seconds() - program_start,
                gt_path.c_str());
    IntVectors ground_truth = read_ivecs(gt_path, max_trn);

    if (train.dimension != base.dimension ||
        query.dimension != base.dimension) {
        throw std::runtime_error(
                "learn/base/query vector dimensions do not match");
    }
    if (query.count != ground_truth.count) {
        throw std::runtime_error(
                "query and ground-truth vector counts do not match");
    }
    if (static_cast<size_t>(k) > ground_truth.dimension) {
        throw std::runtime_error(
                "-k exceeds the number of neighbors in ground truth (" +
                std::to_string(ground_truth.dimension) + ")");
    }
    if (base.count < static_cast<size_t>(nlist)) {
        throw std::runtime_error("-nlist exceeds base vector count");
    }
    if (base.dimension % static_cast<size_t>(pq_m) != 0) {
        throw std::runtime_error(
                "-pq_m must divide the SIFT dimension (" +
                std::to_string(base.dimension) + ")");
    }
    for (int32_t id : ground_truth.values) {
        if (id < 0 || static_cast<size_t>(id) >= base.count) {
            throw std::runtime_error(
                    "ground truth contains an ID outside the base set");
        }
    }

    const size_t dimension = base.dimension;
    const size_t query_count = query.count;
    const size_t gt_k = ground_truth.dimension;
    std::printf(
            "[%.3f s] Loaded: train=%zu  base=%zu  query=%zu  dim=%zu  "
            "ground_truth_k=%zu\n",
            elapsed_seconds() - program_start,
            train.count,
            base.count,
            query_count,
            dimension,
            gt_k);

    std::unique_ptr<FILE, decltype(&std::fclose)> csv(
            std::fopen(output_path.c_str(), "w"), &std::fclose);
    if (!csv) {
        throw std::runtime_error("cannot create result CSV: " + output_path);
    }
    std::fprintf(
            csv.get(),
            "index,params,recall@%d,qps,build_time_s,queries\n",
            k);
    std::fflush(csv.get());

    std::vector<BenchResult> results;
    std::vector<idx_t> ids(query_count * static_cast<size_t>(k));
    std::vector<float> distances(query_count * static_cast<size_t>(k));

    auto run_search = [&](const std::string& label,
                          const std::string& params,
                          faiss::Index& index) -> BenchResult {
        size_t batch_size = query_count;
        if (report_freq > 0) {
            batch_size = std::min(
                    query_count, static_cast<size_t>(report_freq));
        }

        std::unique_ptr<FILE, decltype(&std::fclose)> progress(
                nullptr, &std::fclose);
        if (report_freq > 0) {
            const std::string progress_path =
                    "progress_sift_" +
                    safe_filename_tag(label + "_" + params) + ".csv";
            progress.reset(std::fopen(progress_path.c_str(), "w"));
            if (progress) {
                std::fprintf(
                        progress.get(),
                        "batch,round_begin,round_end,local_time_s,"
                        "local_recall@%d,local_qps,cumulative_recall@%d,"
                        "cumulative_qps\n",
                        k,
                        k);
            }
            std::printf(
                    "  [%s] progress -> %s\n",
                    label.c_str(),
                    progress_path.c_str());
        }

        double search_time = 0.0;
        size_t offset = 0;
        size_t batch_number = 0;
        while (offset < query_count) {
            const size_t batch_begin = offset;
            const size_t current =
                    std::min(batch_size, query_count - offset);
            const double start = elapsed_seconds();
            index.search(
                    static_cast<idx_t>(current),
                    query.values.data() + offset * dimension,
                    static_cast<idx_t>(k),
                    distances.data() + offset * static_cast<size_t>(k),
                    ids.data() + offset * static_cast<size_t>(k));
            const double batch_time = elapsed_seconds() - start;
            search_time += batch_time;
            offset += current;
            ++batch_number;

            if (report_freq > 0) {
                const double local_recall = compute_recall_at_k(
                        ids.data() +
                                batch_begin * static_cast<size_t>(k),
                        ground_truth.values.data() + batch_begin * gt_k,
                        current,
                        static_cast<size_t>(k),
                        gt_k);
                const double local_qps =
                        static_cast<double>(current) / batch_time;
                const double cumulative_recall = compute_recall_at_k(
                        ids.data(),
                        ground_truth.values.data(),
                        offset,
                        static_cast<size_t>(k),
                        gt_k);
                const double cumulative_qps =
                        static_cast<double>(offset) / search_time;
                std::printf(
                        "  [%s] batch=%zu  rounds=%zu-%zu/%zu  "
                        "local: time=%.3fs recall@%d=%.4f QPS=%.1f  "
                        "cumulative: recall@%d=%.4f QPS=%.1f\n",
                        label.c_str(),
                        batch_number,
                        batch_begin + 1,
                        offset,
                        query_count,
                        batch_time,
                        k,
                        local_recall,
                        local_qps,
                        k,
                        cumulative_recall,
                        cumulative_qps);
                if (progress) {
                    std::fprintf(
                            progress.get(),
                            "%zu,%zu,%zu,%.6f,%.6f,%.2f,%.6f,%.2f\n",
                            batch_number,
                            batch_begin + 1,
                            offset,
                            batch_time,
                            local_recall,
                            local_qps,
                            cumulative_recall,
                            cumulative_qps);
                    std::fflush(progress.get());
                }
            }
        }

        BenchResult result;
        result.index_name = label;
        result.params = params;
        result.recall = compute_recall_at_k(
                ids.data(),
                ground_truth.values.data(),
                query_count,
                static_cast<size_t>(k),
                gt_k);
        result.qps = static_cast<double>(query_count) / search_time;
        return result;
    };

    auto save_result = [&](const BenchResult& result) {
        results.push_back(result);
        std::fprintf(
                csv.get(),
                "%s,\"%s\",%.6f,%.2f,%.3f,%zu\n",
                result.index_name.c_str(),
                result.params.c_str(),
                result.recall,
                result.qps,
                result.build_time,
                query_count);
        std::fflush(csv.get());
        std::printf(
                "[%.3f s] %-8s %-31s recall@%d=%.4f  QPS=%10.1f  "
                "build=%.3fs\n",
                elapsed_seconds() - program_start,
                result.index_name.c_str(),
                result.params.c_str(),
                k,
                result.recall,
                result.qps,
                result.build_time);
    };

    if (selected_indexes.count("flat")) {
        std::printf(
                "[%.3f s] Building Flat index ...\n",
                elapsed_seconds() - program_start);
        const double start = elapsed_seconds();
        faiss::IndexFlatL2 index(dimension);
        index.add(static_cast<idx_t>(base.count), base.values.data());
        const double build_time = elapsed_seconds() - start;

        BenchResult result = run_search("Flat", "", index);
        result.build_time = build_time;
        save_result(result);
    }

    if (selected_indexes.count("ivfflat")) {
        std::printf(
                "[%.3f s] Training/building IVFFlat(nlist=%d) ...\n",
                elapsed_seconds() - program_start,
                nlist);
        const double start = elapsed_seconds();
        faiss::IndexFlatL2 quantizer(dimension);
        faiss::IndexIVFFlat index(
                &quantizer, dimension, static_cast<idx_t>(nlist));
        index.train(
                static_cast<idx_t>(train.count), train.values.data());
        index.add(static_cast<idx_t>(base.count), base.values.data());
        const double build_time = elapsed_seconds() - start;

        for (int nprobe : {1, 2, 4, 8, 16, 32, 64}) {
            if (nprobe > nlist) {
                continue;
            }
            index.nprobe = static_cast<size_t>(nprobe);
            const std::string params =
                    "nlist=" + std::to_string(nlist) +
                    " nprobe=" + std::to_string(nprobe);
            BenchResult result =
                    run_search("IVFFlat", params, index);
            result.build_time = build_time;
            save_result(result);
        }
    }

    if (selected_indexes.count("ivfpq")) {
        std::printf(
                "[%.3f s] Training/building IVFPQ(nlist=%d, m=%d) ...\n",
                elapsed_seconds() - program_start,
                nlist,
                pq_m);
        const double start = elapsed_seconds();
        faiss::IndexFlatL2 quantizer(dimension);
        faiss::IndexIVFPQ index(
                &quantizer,
                dimension,
                static_cast<idx_t>(nlist),
                static_cast<size_t>(pq_m),
                8);
        index.train(
                static_cast<idx_t>(train.count), train.values.data());
        index.add(static_cast<idx_t>(base.count), base.values.data());
        const double build_time = elapsed_seconds() - start;

        for (int nprobe : {1, 2, 4, 8, 16, 32, 64}) {
            if (nprobe > nlist) {
                continue;
            }
            index.nprobe = static_cast<size_t>(nprobe);
            const std::string params =
                    "nlist=" + std::to_string(nlist) +
                    " nprobe=" + std::to_string(nprobe) +
                    " m=" + std::to_string(pq_m);
            BenchResult result = run_search("IVFPQ", params, index);
            result.build_time = build_time;
            save_result(result);
        }
    }

    if (selected_indexes.count("hnsw")) {
        std::printf(
                "[%.3f s] Building HNSWFlat(M=%d) ...\n",
                elapsed_seconds() - program_start,
                hnsw_m);
        const double start = elapsed_seconds();
        faiss::IndexHNSWFlat index(dimension, hnsw_m);
        index.add(static_cast<idx_t>(base.count), base.values.data());
        const double build_time = elapsed_seconds() - start;

        for (int ef_search : {16, 32, 64, 128, 256, 512}) {
            index.hnsw.efSearch = ef_search;
            const std::string params =
                    "M=" + std::to_string(hnsw_m) +
                    " efSearch=" + std::to_string(ef_search);
            BenchResult result = run_search("HNSW", params, index);
            result.build_time = build_time;
            save_result(result);
        }
    }

    std::printf("\n");
    std::printf("====================================================================\n");
    std::printf("RESULTS (saved to %s)\n", output_path.c_str());
    std::printf("====================================================================\n");
    std::printf(
            "%-9s %-31s %12s %12s %12s\n",
            "Index",
            "Params",
            "Recall",
            "QPS",
            "Build(s)");
    std::printf(
            "--------------------------------------------------------------------\n");
    for (const BenchResult& result : results) {
        std::printf(
                "%-9s %-31s %12.4f %12.1f %12.3f\n",
                result.index_name.c_str(),
                result.params.c_str(),
                result.recall,
                result.qps,
                result.build_time);
    }
    std::printf("====================================================================\n");
    std::printf(
            "Total time: %.3f s\n", elapsed_seconds() - program_start);
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    try {
        return benchmark_main(argc, argv);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Error: %s\n", error.what());
        return 1;
    }
}
