/*
 * bench_glove_custom.cpp — FAISS QPS / Recall benchmark with progress reporting
 *
 * Extends bench_glove.cpp with two runtime flags:
 *   -maxturn N     Limit total vectors used across db+train+queries (default: all)
 *   -reportfreq N  Print intermediate stats every N queries during search (default: 0 = off)
 *
 * Compile (from faiss-main/):
 *   g++ -O3 -march=armv8-a -std=c++17 -I. -o bench_glove_custom bench_glove_custom.cpp \
 *       -Lbuild/faiss -lfaiss -lopenblas -lpthread -Wl,-rpath,build/faiss
 *
 * Run:
 *   ./bench_glove_custom -dataset dataset/glove.twitter.27B.25d.txt -k 10 -maxturn 200000 -reportfreq 1000
 *
 * All flags are optional:
 *   -dataset  <path>    GloVe text file (default: dataset/glove.twitter.27B.25d.txt)
 *   -k        <int>     top-k for recall (default: 10)
 *   -maxturn  <int>     max total vectors to use from dataset (default: all)
 *   -reportfreq <int>   print a progress line every N queries (default: 0 = no progress)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>

#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFFlat.h>
#include <faiss/IndexIVFPQ.h>
#include <faiss/IndexHNSW.h>

using idx_t = faiss::idx_t;

/* =============================================================
 *  Command-line argument parser (no external dependencies)
 * ============================================================= */
static const char* arg_get_str(int argc, char** argv, const char* flag,
                                const char* def) {
    for (int i = 1; i < argc - 1; i++) {
        if (strcmp(argv[i], flag) == 0)
            return argv[i + 1];
    }
    return def;
}

static int arg_get_int(int argc, char** argv, const char* flag, int def) {
    const char* v = arg_get_str(argc, argv, flag, nullptr);
    return v ? atoi(v) : def;
}

static bool arg_has(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], flag) == 0)
            return true;
    }
    return false;
}

/* =============================================================
 *  Timer
 * ============================================================= */
double elapsed() {
    using namespace std::chrono;
    return duration_cast<microseconds>(
               steady_clock::now().time_since_epoch())
               .count() /
           1e6;
}

/* =============================================================
 *  GloVe reader — loads at most max_vectors
 * ============================================================= */
float* read_glove(const std::string& filename, size_t& d_out,
                   size_t& n_out, size_t max_vectors) {
    std::ifstream fin(filename);
    if (!fin.is_open()) {
        fprintf(stderr, "Error: cannot open %s\n", filename.c_str());
        exit(1);
    }

    std::vector<float> all_data;
    std::string line;
    size_t d = 0;
    size_t n = 0;

    while (std::getline(fin, line)) {
        if (max_vectors > 0 && n >= max_vectors)
            break;
        if (line.empty())
            continue;
        std::istringstream iss(line);
        std::string word;
        iss >> word; // skip word token
        float val;
        size_t cnt = 0;
        while (iss >> val) {
            all_data.push_back(val);
            cnt++;
        }
        if (n == 0) {
            d = cnt;
        }
        n++;
    }
    fin.close();

    d_out = d;
    n_out = n;
    printf("  read %zu vectors, dim %zu", n, d);
    if (max_vectors > 0)
        printf(" (capped by -maxturn=%zu)", max_vectors);
    printf("\n");

    float* x = new float[n * d];
    memcpy(x, all_data.data(), n * d * sizeof(float));
    return x;
}

/* =============================================================
 *  Pick the best m for PQ: largest divisor of d that is <= 64
 *  (prefers values close to 8)
 * ============================================================= */
int best_pq_m(size_t d) {
    int best = 1;
    for (int m = 1; m <= (int)std::min(d, size_t(64)); m++) {
        if (d % m == 0) {
            if (best == 1 || std::abs(m - 8) < std::abs(best - 8))
                best = m;
        }
    }
    return best;
}

/* =============================================================
 *  Recall@k
 * ============================================================= */
float compute_recall(const idx_t* I, const idx_t* gt, size_t nq, size_t k) {
    size_t correct = 0;
    for (size_t i = 0; i < nq; i++) {
        for (size_t j = 0; j < k; j++) {
            if (I[i * k + j] == gt[i * k + j]) {
                correct++;
            }
        }
    }
    return float(correct) / float(nq * k);
}

/* =============================================================
 *  Result row
 * ============================================================= */
struct BenchResult {
    std::string index_name;
    std::string params;
    float recall;
    double qps;
    double build_time;
};

/* =============================================================
 *  Main
 * ============================================================= */
int main(int argc, char** argv) {
    double t_start = elapsed();

    /* ---- Parse flags ---- */
    std::string dataset_path =
        arg_get_str(argc, argv, "-dataset", "dataset/glove.twitter.27B.25d.txt");
    int k              = arg_get_int(argc, argv, "-k", 10);
    size_t maxturn     = (size_t)arg_get_int(argc, argv, "-maxturn", 0);
    int reportfreq     = arg_get_int(argc, argv, "-reportfreq", 0);

    if (arg_has(argc, argv, "-h") || arg_has(argc, argv, "--help")) {
        printf("Usage: %s [flags]\n", argv[0]);
        printf("  -dataset  <path>    GloVe dataset  (default: dataset/glove.twitter.27B.25d.txt)\n");
        printf("  -k        <int>     top-k recall   (default: 10)\n");
        printf("  -maxturn  <int>     max vectors to use  (default: all)\n");
        printf("  -reportfreq <int>   progress every N queries  (default: 0 = off)\n");
        return 0;
    }

    printf("============================================================\n");
    printf("FAISS QPS / Recall Benchmark  (custom edition)\n");
    printf("============================================================\n");

    /* ---- Load ---- */
    printf("[%.3f s] Loading dataset: %s\n", elapsed() - t_start,
           dataset_path.c_str());

    size_t d, n_total;
    float* all_data = read_glove(dataset_path, d, n_total, maxturn);

    /* ---- Split ---- */
    size_t nq = 10000;
    if (nq > n_total / 3)
        nq = n_total / 10;
    if (nq < 100)
        nq = 100;

    size_t nb = n_total - nq;   // database = everything except queries
    size_t nt = nb / 2;         // training = half of database
    if (nt > 200000)
        nt = 200000;

    float* xb = all_data;
    float* xt = xb;
    float* xq = all_data + nb * d;

    printf("[%.3f s] Split: db=%zu  train=%zu  queries=%zu  dim=%zu  k=%d\n",
           elapsed() - t_start, nb, nt, nq, d, k);

    /* ---- Ground truth ---- */
    printf("[%.3f s] Computing ground truth (IndexFlatL2) ...\n",
           elapsed() - t_start);

    faiss::IndexFlatL2 flat_index(d);
    flat_index.add(nb, xb);

    idx_t* gt_ids = new idx_t[k * nq];
    float* gt_dist = new float[k * nq];

    double t_gt0 = elapsed();
    flat_index.search(nq, xq, k, gt_dist, gt_ids);
    double t_gt = elapsed() - t_gt0;
    double t_gt_qps = nq / t_gt;

    printf("[%.3f s] Ground truth done  (%.3f s,  %.0f QPS)\n",
           elapsed() - t_start, t_gt, t_gt_qps);

    /* ---- Open results CSV ---- */
    FILE* csv = fopen("bench_results.csv", "w");
    if (!csv) {
        perror("fopen bench_results.csv");
        return 1;
    }
    fprintf(csv, "index,params,recall@%d,qps,build_time_s\n", k);

    std::vector<BenchResult> all_results;
    idx_t* I = new idx_t[k * nq];
    float* D = new float[k * nq];

    /* =============================================================
     *  0) Flat (baseline)
     * ============================================================= */
    {
        BenchResult r{"Flat", "", 1.0f, t_gt_qps, 0.0};
        all_results.push_back(r);
        fprintf(csv, "Flat,,%.6f,%.1f,%.3f\n", r.recall, r.qps, r.build_time);
        printf("[%.3f s] Flat:                recall=1.0000  QPS=%9.1f\n",
               elapsed() - t_start, r.qps);
    }

    /* =============================================================
     *  Helper to run a single search (with optional progress reporting)
     *  Returns (recall, qps) for the full run, and prints intermediate
     *  lines if reportfreq > 0.
     * ============================================================= */
    auto run_search = [&](const char* label, faiss::Index& index,
                          int nq_batch, double& out_recall,
                          double& out_qps) {
        // If reportfreq <= 0 or nq_batch <= reportfreq, just run once
        if (reportfreq <= 0 || nq_batch <= reportfreq) {
            double t0 = elapsed();
            index.search(nq_batch, xq, k, D, I);
            double dt = elapsed() - t0;
            out_qps   = nq_batch / dt;
            out_recall = compute_recall(I, gt_ids, nq_batch, k);
            return;
        }

        // Batched run with progress reporting
        int batch_size = reportfreq;
        int n_batches  = (nq_batch + batch_size - 1) / batch_size;
        size_t cumul_correct = 0;
        double cumul_time = 0.0;

        printf("  [%s] running %d queries in %d batches of %d ...\n",
               label, nq_batch, n_batches, batch_size);

        for (int b = 0; b < n_batches; b++) {
            int offset = b * batch_size;
            int this_batch = std::min(batch_size, nq_batch - offset);
            // point xq, I, D, gt_ids to the current batch
            float* xq_batch  = xq + offset * d;
            idx_t* I_batch   = I + offset * k;
            float* D_batch   = D + offset * k;
            idx_t* gt_batch  = gt_ids + offset * k;

            double t0 = elapsed();
            index.search(this_batch, xq_batch, k, D_batch, I_batch);
            double dt = elapsed() - t0;
            cumul_time += dt;

            // partial recall for this batch
            size_t batch_correct = 0;
            for (int i = 0; i < this_batch; i++) {
                for (int j = 0; j < k; j++) {
                    if (I_batch[i * k + j] == gt_batch[i * k + j])
                        batch_correct++;
                }
            }
            cumul_correct += batch_correct;

            int done = offset + this_batch;
            double cumul_recall = double(cumul_correct) / double(done * k);
            double cumul_qps    = double(done) / cumul_time;

            printf("  [%s] batch %d/%d  queries %d/%d  "
                   "batch_time=%.3fs  cumul_recall=%.4f  cumul_QPS=%.0f\n",
                   label, b + 1, n_batches, done, nq_batch,
                   dt, cumul_recall, cumul_qps);
        }

        out_recall = double(cumul_correct) / double(nq_batch * k);
        out_qps    = double(nq_batch) / cumul_time;
    };

    /* =============================================================
     *  1) IVFFlat
     * ============================================================= */
    {
        int nlist = 100;
        printf("[%.3f s] Training IVFFlat(nlist=%d) ...\n",
               elapsed() - t_start, nlist);

        double t0 = elapsed();
        faiss::IndexFlatL2 quantizer(d);
        faiss::IndexIVFFlat ivf_flat(&quantizer, d, nlist);
        ivf_flat.train(nt, xt);
        ivf_flat.add(nb, xb);
        double t_build = elapsed() - t0;

        printf("[%.3f s] IVFFlat built in %.3f s\n",
               elapsed() - t_start, t_build);

        for (int nprobe : {1, 2, 4, 8, 16, 32, 64}) {
            ivf_flat.nprobe = nprobe;

            double recall, qps;
            run_search("IVFFlat", ivf_flat, nq, recall, qps);

            char params[64];
            snprintf(params, sizeof(params), "nlist=%d nprobe=%d",
                     nlist, nprobe);

            all_results.push_back(
                {"IVFFlat", params, (float)recall, qps, t_build});
            fprintf(csv, "IVFFlat,%s,%.6f,%.1f,%.3f\n",
                    params, recall, qps, t_build);
        }
    }

    /* =============================================================
     *  2) IVFPQ
     * ============================================================= */
    {
        int nlist = 100;
        int m = best_pq_m(d);

        printf("[%.3f s] Training IVFPQ(nlist=%d, m=%d, dim=%zu) ...\n",
               elapsed() - t_start, nlist, m, d);

        double t0 = elapsed();
        faiss::IndexFlatL2 quantizer(d);
        faiss::IndexIVFPQ ivf_pq(&quantizer, d, nlist, m, 8);
        ivf_pq.train(nt, xt);
        ivf_pq.add(nb, xb);
        double t_build = elapsed() - t0;

        printf("[%.3f s] IVFPQ built in %.3f s\n",
               elapsed() - t_start, t_build);

        for (int nprobe : {1, 2, 4, 8, 16, 32, 64}) {
            ivf_pq.nprobe = nprobe;

            double recall, qps;
            run_search("IVFPQ", ivf_pq, nq, recall, qps);

            char params[64];
            snprintf(params, sizeof(params), "nlist=%d nprobe=%d m=%d",
                     nlist, nprobe, m);

            all_results.push_back(
                {"IVFPQ", params, (float)recall, qps, t_build});
            fprintf(csv, "IVFPQ,%s,%.6f,%.1f,%.3f\n",
                    params, recall, qps, t_build);
        }
    }

    /* =============================================================
     *  3) HNSW
     * ============================================================= */
    {
        int M = 32;
        printf("[%.3f s] Building HNSWFlat(M=%d) ...\n",
               elapsed() - t_start, M);

        double t0 = elapsed();
        faiss::IndexHNSWFlat hnsw(d, M);
        hnsw.add(nb, xb);
        double t_build = elapsed() - t0;

        printf("[%.3f s] HNSW built in %.3f s\n",
               elapsed() - t_start, t_build);

        for (int efSearch : {16, 32, 64, 128, 256, 512}) {
            hnsw.hnsw.efSearch = efSearch;

            double recall, qps;
            run_search("HNSW", hnsw, nq, recall, qps);

            char params[64];
            snprintf(params, sizeof(params), "M=%d efSearch=%d",
                     M, efSearch);

            all_results.push_back(
                {"HNSW", params, (float)recall, qps, t_build});
            fprintf(csv, "HNSW,%s,%.6f,%.1f,%.3f\n",
                    params, recall, qps, t_build);
        }
    }

    /* ---- Close CSV ---- */
    fclose(csv);

    /* ---- Summary ---- */
    printf("\n");
    printf("============================================================\n");
    printf("RESULTS  (saved to bench_results.csv)\n");
    printf("============================================================\n");
    printf("%-10s %-24s %10s %12s %12s\n",
           "Index", "Params", "Recall", "QPS", "Build(s)");
    printf("------------------------------------------------------------\n");
    for (const auto& r : all_results) {
        printf("%-10s %-24s %10.4f %12.1f %12.3f\n",
               r.index_name.c_str(), r.params.c_str(),
               r.recall, r.qps, r.build_time);
    }
    printf("============================================================\n");
    printf("Total time: %.3f s\n", elapsed() - t_start);

    /* ---- Cleanup ---- */
    delete[] all_data;
    delete[] gt_ids;
    delete[] gt_dist;
    delete[] I;
    delete[] D;

    return 0;
}
