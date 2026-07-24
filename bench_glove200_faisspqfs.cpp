/*
 * bench_glove200_faisspqfs.cpp - FAISS GloVe-200d QPS / Recall benchmark for ARM
 *
 * Compile from the Faiss source root on ARM Linux:
 *   g++ -O3 -march=armv8-a -std=c++17 -I. \
 *       -o bench_glove200_faisspqfs bench_glove200_faisspqfs.cpp \
 *       -Lbuild/faiss -lfaiss -lopenblas -fopenmp -lpthread \
 *       -Wl,-rpath,build/faiss
 *
 * Run:
 *   ./bench_glove200_faisspqfs
 *   ./bench_glove200_faisspqfs -dataset dataset/glove.twitter.27B.200d.txt -k 10
 *   ./bench_glove200_faisspqfs -indexes ivfflat,ivfpq,ivfpqfs -maxtrn 2000
 *   ./bench_glove200_faisspqfs -nlist 128,256,512,1024,2048,4096 \
 *       -nprobe 1,2,4,8,16,32,64,128,256 -m 100 -k_reorder 0
 *
 * The input must be a GloVe text file with one word followed by
 * exactly 200 floating-point values per line.
 */

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cmath>
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
#include <faiss/IndexIVFPQFastScan.h>
#include <faiss/IndexRefine.h>

using idx_t = faiss::idx_t;

namespace {

struct FloatVectors {
    size_t dimension = 0;
    size_t count = 0;
    std::vector<float> values;
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

std::vector<int> parse_int_list_arg(
        int argc,
        char** argv,
        const char* flag,
        const char* default_value) {
    const std::string text =
            arg_get_str(argc, argv, flag, default_value);
    std::vector<int> values;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t comma = text.find(',', start);
        const std::string token = text.substr(
                start,
                comma == std::string::npos ? std::string::npos
                                           : comma - start);
        if (token.empty() || token[0] == '-') {
            throw std::runtime_error(
                    std::string("invalid value for ") + flag + ": " + text);
        }

        errno = 0;
        char* end = nullptr;
        const unsigned long long value =
                std::strtoull(token.c_str(), &end, 10);
        if (errno == ERANGE || !end || *end != '\0' || value == 0 ||
            value >
                    static_cast<unsigned long long>(
                            std::numeric_limits<int>::max())) {
            throw std::runtime_error(
                    std::string("invalid value for ") + flag + ": " + text);
        }
        values.push_back(static_cast<int>(value));

        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return values;
}

std::string join_int_list(const std::vector<int>& values) {
    std::string text;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            text += ',';
        }
        text += std::to_string(values[i]);
    }
    return text;
}

FloatVectors read_glove200(
        const std::string& path,
        size_t max_vectors) {
    constexpr size_t kExpectedDimension = 200;
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open: " + path);
    }

    FloatVectors result;
    result.dimension = kExpectedDimension;
    if (max_vectors > 0) {
        result.values.reserve(max_vectors * kExpectedDimension);
    }

    std::string line;
    size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (max_vectors > 0 && result.count >= max_vectors) {
            break;
        }
        if (line.empty()) {
            continue;
        }

        const char* cursor = line.c_str();
        while (*cursor &&
               !std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (!*cursor) {
            throw std::runtime_error(
                    "missing vector values at line " +
                    std::to_string(line_number));
        }

        for (size_t j = 0; j < kExpectedDimension; ++j) {
            while (*cursor &&
                   std::isspace(static_cast<unsigned char>(*cursor))) {
                ++cursor;
            }
            errno = 0;
            char* end = nullptr;
            const float value = std::strtof(cursor, &end);
            if (end == cursor || errno == ERANGE) {
                throw std::runtime_error(
                        "invalid float at line " +
                        std::to_string(line_number) + ", dimension " +
                        std::to_string(j));
            }
            result.values.push_back(value);
            cursor = end;
        }

        while (*cursor &&
               std::isspace(static_cast<unsigned char>(*cursor))) {
            ++cursor;
        }
        if (*cursor) {
            throw std::runtime_error(
                    "more than 200 values at line " +
                    std::to_string(line_number));
        }

        ++result.count;
        if (result.count % 100000 == 0) {
            std::printf(
                    "  loaded %zu GloVe vectors...\n", result.count);
        }
    }

    if (result.count < 100) {
        throw std::runtime_error(
                "GloVe input must contain at least 100 vectors");
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
        if (name == "faisspqfs" || name == "pqfs") {
            name = "ivfpqfs";
        }
        if (name == "all") {
            indexes.insert("flat");
            indexes.insert("ivfflat");
            indexes.insert("ivfpq");
            indexes.insert("ivfpqfs");
            indexes.insert("hnsw");
        } else if (
                name == "flat" || name == "ivfflat" || name == "ivfpq" ||
                name == "ivfpqfs" || name == "hnsw") {
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
        const idx_t* ground_truth,
        size_t query_count,
        size_t result_k,
        size_t ground_truth_k) {
    size_t matches = 0;
    for (size_t query = 0; query < query_count; ++query) {
        const idx_t* result_row = results + query * result_k;
        const idx_t* gt_row = ground_truth + query * ground_truth_k;
        for (size_t i = 0; i < result_k; ++i) {
            for (size_t j = 0; j < result_k; ++j) {
                if (result_row[i] == gt_row[j]) {
                    ++matches;
                    break;
                }
            }
        }
    }
    return static_cast<double>(matches) /
            static_cast<double>(query_count * result_k);
}

void print_help(const char* program) {
    std::printf("Usage: %s [flags]\n", program);
    std::printf(
            "  -dataset     <path>  GloVe-200d text file (default: "
            "dataset/glove.twitter.27B.200d.txt)\n");
    std::printf(
            "  -indexes     <list>  all or comma-separated flat,ivfflat,"
            "ivfpq,ivfpqfs,hnsw (default: all)\n");
    std::printf(
            "  -k           <int>   search/recall Top-K (default: 10)\n");
    std::printf(
            "  -maxtrn      <int>   maximum query rounds (default: 10000)\n");
    std::printf(
            "  -maxvectors  <int>   cap loaded GloVe vectors (default: all)\n");
    std::printf(
            "  -reportfreq  <int>   print local stats every N rounds "
            "(default: 0 = off)\n");
    std::printf(
            "  -nlist       <list>  IVF cluster counts (default: "
            "128,256,512,1024,2048,4096)\n");
    std::printf(
            "  -nprobe      <list>  IVF probes (default: "
            "1,2,4,8,16,32,64,128,256)\n");
    std::printf(
            "  -m           <int>   PQ subquantizer count (default: 100)\n");
    std::printf(
            "  -k_reorder   <int>   exact rerank factor; 0 disables "
            "(default: 0)\n");
    std::printf(
            "  -bbs         <int>   FastScan block size (default: 32)\n");
    std::printf(
            "  -hnsw_m      <int>   HNSW links per node (default: 32)\n");
    std::printf(
            "  -output      <path>  final result CSV (default: "
            "bench_glove200_faisspqfs_results.csv)\n");
}

int benchmark_main(int argc, char** argv) {
    if (arg_has(argc, argv, "-h") || arg_has(argc, argv, "--help")) {
        print_help(argv[0]);
        return 0;
    }

    const std::string dataset_path = arg_get_str(
            argc,
            argv,
            "-dataset",
            "dataset/glove.twitter.27B.200d.txt");
    const std::string output_path = arg_get_str(
            argc,
            argv,
            "-output",
            "bench_glove200_faisspqfs_results.csv");
    const std::string index_text =
            arg_get_str(argc, argv, "-indexes", "all");
    const auto selected_indexes = parse_indexes(index_text);

    const std::string metric_text = "l2";
    const faiss::MetricType metric = faiss::METRIC_L2;

    const int k = parse_int_arg(argc, argv, "-k", 10);
    const size_t max_trn =
            parse_size_arg(argc, argv, "-maxtrn", 10000);
    const size_t maxturn_compat =
            parse_size_arg(argc, argv, "-maxturn", 0);
    const size_t max_vectors =
            parse_size_arg(argc, argv, "-maxvectors", maxturn_compat);
    const int report_freq =
            parse_int_arg(argc, argv, "-reportfreq", 0);
    const std::vector<int> nlists = parse_int_list_arg(
            argc,
            argv,
            "-nlist",
            "128,256,512,1024,2048,4096");
    const std::vector<int> nprobes = parse_int_list_arg(
            argc,
            argv,
            "-nprobe",
            "1,2,4,8,16,32,64,128,256");
    const int pq_m_compat = parse_int_arg(argc, argv, "-pq_m", 100);
    const int pq_m = parse_int_arg(argc, argv, "-m", pq_m_compat);
    const int k_reorder =
            parse_int_arg(argc, argv, "-k_reorder", 0);
    const int bbs = parse_int_arg(argc, argv, "-bbs", 32);
    const int hnsw_m = parse_int_arg(argc, argv, "-hnsw_m", 32);

    if (k <= 0 || pq_m <= 0 || bbs <= 0 || hnsw_m <= 0) {
        throw std::runtime_error(
                "-k, -m, -bbs and -hnsw_m must be positive");
    }
    if (200 % pq_m != 0) {
        throw std::runtime_error("-m must divide GloVe dimension 200");
    }
    if (bbs % 32 != 0) {
        throw std::runtime_error("-bbs must be a multiple of 32");
    }

    const double program_start = elapsed_seconds();
    const std::string nlist_text = join_int_list(nlists);
    const std::string nprobe_text = join_int_list(nprobes);
    std::printf("============================================================\n");
    std::printf("FAISS GloVe-200d IVFPQ FastScan Benchmark\n");
    std::printf(
            "  dataset=%s  metric=%s  indexes=%s\n",
            dataset_path.c_str(),
            metric_text.c_str(),
            index_text.c_str());
    std::printf(
            "  topk=%d  maxtrn=%zu  maxvectors=%zu  reportfreq=%d\n",
            k,
            max_trn,
            max_vectors,
            report_freq);
    std::printf(
            "  nlist=%s  nprobe=%s  m=%d  k_reorder=%d  bbs=%d\n",
            nlist_text.c_str(),
            nprobe_text.c_str(),
            pq_m,
            k_reorder,
            bbs);
    std::printf("============================================================\n");

    std::printf(
            "[%.3f s] Loading GloVe-200d: %s\n",
            elapsed_seconds() - program_start,
            dataset_path.c_str());
    FloatVectors all_data = read_glove200(dataset_path, max_vectors);
    const size_t dimension = all_data.dimension;
    const size_t total_count = all_data.count;
    const size_t query_limit = total_count / 10;
    const size_t requested_queries =
            max_trn == 0 ? query_limit : max_trn;
    const size_t query_count =
            std::min(requested_queries, query_limit);
    if (query_count == 0 || query_count >= total_count) {
        throw std::runtime_error(
                "not enough vectors to form database and query sets");
    }
    const size_t base_count = total_count - query_count;
    const size_t gt_k = static_cast<size_t>(k);
    const size_t train_count = std::min<size_t>(500000, base_count);
    float* xb = all_data.values.data();
    float* xt = xb;
    float* xq = xb + base_count * dimension;

    if (static_cast<size_t>(k) > base_count) {
        throw std::runtime_error("-k exceeds the base vector count");
    }
    if (k_reorder > 0 &&
        static_cast<size_t>(k) >
                base_count / static_cast<size_t>(k_reorder)) {
        throw std::runtime_error(
                "k * k_reorder must not exceed the base vector count");
    }

    std::printf(
            "[%.3f s] Split: train=%zu  base=%zu  query=%zu  dim=%zu\n",
            elapsed_seconds() - program_start,
            train_count,
            base_count,
            query_count,
            dimension);

    std::vector<idx_t> ground_truth(
            query_count * static_cast<size_t>(k));
    std::vector<float> ground_truth_distances(
            query_count * static_cast<size_t>(k));
    double flat_build_time = 0.0;
    double flat_qps = 0.0;
    {
        std::printf(
                "[%.3f s] Building exact Flat ground truth...\n",
                elapsed_seconds() - program_start);
        const double build_start = elapsed_seconds();
        faiss::IndexFlat flat(dimension, metric);
        flat.add(static_cast<idx_t>(base_count), xb);
        flat_build_time = elapsed_seconds() - build_start;
        const double search_start = elapsed_seconds();
        flat.search(
                static_cast<idx_t>(query_count),
                xq,
                static_cast<idx_t>(k),
                ground_truth_distances.data(),
                ground_truth.data());
        const double search_time = elapsed_seconds() - search_start;
        flat_qps = static_cast<double>(query_count) / search_time;
        std::printf(
                "[%.3f s] Ground truth ready: %.3fs, QPS=%.1f\n",
                elapsed_seconds() - program_start,
                search_time,
                flat_qps);
    }

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
                    xq + offset * dimension,
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
                        ground_truth.data() + batch_begin * gt_k,
                        current,
                        static_cast<size_t>(k),
                        gt_k);
                const double local_qps =
                        static_cast<double>(current) / batch_time;
                const double cumulative_recall = compute_recall_at_k(
                        ids.data(),
                        ground_truth.data(),
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
            }
        }

        BenchResult result;
        result.index_name = label;
        result.params = params;
        result.recall = compute_recall_at_k(
                ids.data(),
                ground_truth.data(),
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
                "[%.3f s] %-9s %-60s recall@%d=%.4f  QPS=%10.1f  "
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
        save_result({
                "Flat",
                "metric=l2",
                1.0,
                flat_qps,
                flat_build_time});
    }

    if (selected_indexes.count("ivfflat")) {
        for (int nlist : nlists) {
            if (static_cast<size_t>(nlist) > train_count ||
                static_cast<size_t>(nlist) > base_count) {
                std::printf(
                        "[IVFFlat] skip nlist=%d: not enough vectors\n",
                        nlist);
                continue;
            }
            std::printf(
                    "[%.3f s] Training/building IVFFlat(nlist=%d) ...\n",
                    elapsed_seconds() - program_start,
                    nlist);
            const double start = elapsed_seconds();
            faiss::IndexFlat quantizer(dimension, metric);
            faiss::IndexIVFFlat index(
                    &quantizer,
                    dimension,
                    static_cast<size_t>(nlist),
                    metric);
            index.train(static_cast<idx_t>(train_count), xt);
            index.add(static_cast<idx_t>(base_count), xb);
            const double build_time = elapsed_seconds() - start;

            for (int nprobe : nprobes) {
                if (nprobe > nlist) {
                    std::printf(
                            "  [IVFFlat] skip nlist=%d nprobe=%d "
                            "(nprobe > nlist)\n",
                            nlist,
                            nprobe);
                    continue;
                }
                index.nprobe = static_cast<size_t>(nprobe);
                const std::string params =
                        "metric=l2 nlist=" + std::to_string(nlist) +
                        " nprobe=" + std::to_string(nprobe);
                BenchResult result =
                        run_search("IVFFlat", params, index);
                result.build_time = build_time;
                save_result(result);
            }
        }
    }

    if (selected_indexes.count("ivfpq")) {
        for (int nlist : nlists) {
            if (static_cast<size_t>(nlist) > train_count ||
                static_cast<size_t>(nlist) > base_count) {
                std::printf(
                        "[IVFPQ] skip nlist=%d: not enough vectors\n",
                        nlist);
                continue;
            }
            std::printf(
                    "[%.3f s] Training/building IVFPQ(nlist=%d, "
                    "m=%d, nbits=8) ...\n",
                    elapsed_seconds() - program_start,
                    nlist,
                    pq_m);
            const double start = elapsed_seconds();
            faiss::IndexFlat quantizer(dimension, metric);
            faiss::IndexIVFPQ index(
                    &quantizer,
                    dimension,
                    static_cast<size_t>(nlist),
                    static_cast<size_t>(pq_m),
                    8,
                    metric);
            index.train(static_cast<idx_t>(train_count), xt);
            index.add(static_cast<idx_t>(base_count), xb);
            const double build_time = elapsed_seconds() - start;

            for (int nprobe : nprobes) {
                if (nprobe > nlist) {
                    std::printf(
                            "  [IVFPQ] skip nlist=%d nprobe=%d "
                            "(nprobe > nlist)\n",
                            nlist,
                            nprobe);
                    continue;
                }
                index.nprobe = static_cast<size_t>(nprobe);
                const std::string params =
                        "metric=l2 nlist=" + std::to_string(nlist) +
                        " nprobe=" + std::to_string(nprobe) +
                        " m=" + std::to_string(pq_m) + " nbits=8";
                BenchResult result = run_search("IVFPQ", params, index);
                result.build_time = build_time;
                save_result(result);
            }
        }
    }

    if (selected_indexes.count("ivfpqfs")) {
        for (int nlist : nlists) {
            if (static_cast<size_t>(nlist) > train_count ||
                static_cast<size_t>(nlist) > base_count) {
                std::printf(
                        "[IVFPQfs] skip nlist=%d: not enough vectors\n",
                        nlist);
                continue;
            }
            std::printf(
                    "[%.3f s] Training/building IVFPQFastScan("
                    "nlist=%d, m=%d, nbits=4, bbs=%d) ...\n",
                    elapsed_seconds() - program_start,
                    nlist,
                    pq_m,
                    bbs);
            const double start = elapsed_seconds();
            faiss::IndexFlat quantizer(dimension, metric);
            faiss::IndexIVFPQFastScan index(
                    &quantizer,
                    dimension,
                    static_cast<size_t>(nlist),
                    static_cast<size_t>(pq_m),
                    4,
                    metric,
                    bbs);
            index.train(static_cast<idx_t>(train_count), xt);
            index.add(static_cast<idx_t>(base_count), xb);

            auto make_params = [&](int nprobe) {
                return "metric=l2 nlist=" + std::to_string(nlist) +
                        " nprobe=" + std::to_string(nprobe) +
                        " m=" + std::to_string(pq_m) +
                        " nbits=4 bbs=" + std::to_string(bbs) +
                        " k_reorder=" + std::to_string(k_reorder);
            };

            if (k_reorder == 0) {
                const double build_time = elapsed_seconds() - start;
                for (int nprobe : nprobes) {
                    if (nprobe > nlist) {
                        std::printf(
                                "  [IVFPQfs] skip nlist=%d nprobe=%d "
                                "(nprobe > nlist)\n",
                                nlist,
                                nprobe);
                        continue;
                    }
                    index.nprobe = static_cast<size_t>(nprobe);
                    BenchResult result = run_search(
                            "IVFPQfs", make_params(nprobe), index);
                    result.build_time = build_time;
                    save_result(result);
                }
            } else {
                std::printf(
                        "[%.3f s] Building exact reorder layer "
                        "(candidate_k=%zu) ...\n",
                        elapsed_seconds() - program_start,
                        static_cast<size_t>(k) *
                                static_cast<size_t>(k_reorder));
                faiss::IndexRefineFlat refine(&index, xb);
                refine.k_factor = static_cast<float>(k_reorder);
                const double build_time = elapsed_seconds() - start;
                for (int nprobe : nprobes) {
                    if (nprobe > nlist) {
                        std::printf(
                                "  [IVFPQfs] skip nlist=%d nprobe=%d "
                                "(nprobe > nlist)\n",
                                nlist,
                                nprobe);
                        continue;
                    }
                    index.nprobe = static_cast<size_t>(nprobe);
                    BenchResult result = run_search(
                            "IVFPQfs", make_params(nprobe), refine);
                    result.build_time = build_time;
                    save_result(result);
                }
            }
        }
    }

    if (selected_indexes.count("hnsw")) {
        std::printf(
                "[%.3f s] Building HNSWFlat(M=%d, metric=L2) ...\n",
                elapsed_seconds() - program_start,
                hnsw_m);
        const double start = elapsed_seconds();
        faiss::IndexHNSWFlat index(dimension, hnsw_m, metric);
        index.add(static_cast<idx_t>(base_count), xb);
        const double build_time = elapsed_seconds() - start;

        for (int ef_search : {16, 32, 64, 128, 256, 512}) {
            index.hnsw.efSearch = ef_search;
            const std::string params =
                    "metric=l2 M=" + std::to_string(hnsw_m) +
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
            "%-9s %-60s %12s %12s %12s\n",
            "Index",
            "Params",
            "Recall",
            "QPS",
            "Build(s)");
    std::printf(
            "--------------------------------------------------------------------\n");
    for (const BenchResult& result : results) {
        std::printf(
                "%-9s %-60s %12.4f %12.1f %12.3f\n",
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
