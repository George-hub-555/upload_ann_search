/*
 * bench_glove.cpp — FAISS QPS / Recall benchmark for aarch64 (no GPU)
 *
 * Reads the GloVe Twitter 27B text dataset, builds multiple index types,
 * measures QPS and Recall@k for each, and writes results to bench_results.csv.
 *
 * Compile (from faiss-main/):
 *   g++ -O3 -march=armv8-a -std=c++17 -I. -o bench_glove bench_glove.cpp \
 *       -Lbuild/faiss -lfaiss -lopenblas -lpthread -Wl,-rpath,build/faiss
 *
 * Run (from faiss-main/):
 *   ./bench_glove [dataset_path] [k]
 *
 * Defaults:
 *   dataset_path = dataset/glove.twitter.27B.25d.txt
 *   k           = 10
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
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

/* ── Timer helper ── */
double elapsed() {
    using namespace std::chrono;
    return duration_cast<microseconds>(
               steady_clock::now().time_since_epoch())
               .count() /
           1e6;
}

/* ── Read GloVe text format: each line is "<word> <f1> <f2> ... <fd>" ── */
float* read_glove(const std::string& filename, size_t& d_out, size_t& n_out) {
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
        if (line.empty())
            continue;
        std::istringstream iss(line);
        std::string word;
        iss >> word; // skip the word token
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
    printf("  read %zu vectors, dim %zu\n", n, d);

    float* x = new float[n * d];
    memcpy(x, all_data.data(), n * d * sizeof(float));
    return x;
}

/* ── Compute Recall@k: fraction of ground-truth IDs that appear in results ── */
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

/* ── Struct for one row of results ── */
struct BenchResult {
    std::string index_name;
    std::string params;
    float recall;
    double qps;
    double build_time;
};

/* ================================================================= */

int main(int argc, char** argv) {
    double t_start = elapsed();

    /* ---- Parse arguments ---- */
    std::string dataset_path =
        (argc > 1) ? argv[1] : "dataset/glove.twitter.27B.25d.txt";
    int k = (argc > 2) ? std::atoi(argv[2]) : 10;

    printf("============================================\n");
    printf("FAISS QPS / Recall Benchmark (aarch64, CPU)\n");
    printf("============================================\n");

    /* ---- Load dataset ---- */
    printf("[%.3f s] Loading dataset: %s\n", elapsed() - t_start,
           dataset_path.c_str());

    size_t d, n_total;
    float* all_data = read_glove(dataset_path, d, n_total);

    /* ---- Split into database / train / queries ---- */
    size_t nb = 1000000;         // database vectors
    size_t nt = 100000;          // training vectors (subset of db)
    size_t nq = 10000;           // query vectors

    if (n_total < nb + nq) {
        nb = n_total - nq;
        fprintf(stderr, "Warning: adjusting nb to %zu (not enough vectors)\n",
                nb);
    }
    if (nt > nb)
        nt = nb / 2;

    float* xb = all_data;                // first nb = database
    float* xt = xb;                      // first nt of db = training
    float* xq = all_data + nb * d;       // last nq = queries

    printf("[%.3f s] Split: db=%zu  train=%zu  queries=%zu  dim=%zu  k=%d\n",
           elapsed() - t_start, nb, nt, nq, d, k);

    /* ---- Compute ground truth via brute-force FlatL2 ---- */
    printf("[%.3f s] Computing ground truth (IndexFlatL2) ...\n",
           elapsed() - t_start);

    faiss::IndexFlatL2 flat_index(d);
    flat_index.add(nb, xb);

    idx_t* gt_ids = new idx_t[k * nq];
    float* gt_dist = new float[k * nq];

    double t_gt0 = elapsed();
    flat_index.search(nq, xq, k, gt_dist, gt_ids);
    double t_gt = elapsed() - t_gt0;

    printf("[%.3f s] Ground truth done  (%.3f s,  %.0f QPS)\n",
           elapsed() - t_start, t_gt, nq / t_gt);

    /* ---- Open results CSV ---- */
    FILE* csv = fopen("bench_results.csv", "w");
    if (!csv) {
        perror("fopen bench_results.csv");
        return 1;
    }
    fprintf(csv, "index,params,recall@%d,qps,build_time_s\n", k);

    /* ---- Storage for per-index results ---- */
    std::vector<BenchResult> all_results;

    // We'll reuse these buffers for each index
    idx_t* I = new idx_t[k * nq];
    float* D = new float[k * nq];

    /* ============================================================= */
    /*  1) Flat  (baseline — already done above)                     */
    /* ============================================================= */
    {
        BenchResult r{"Flat", "", 1.0f, nq / t_gt, 0.0};
        all_results.push_back(r);
        fprintf(csv, "Flat,,%.6f,%.1f,%.3f\n", r.recall, r.qps, r.build_time);
        printf("[%.3f s] Flat:                recall=1.0000  QPS=%9.1f\n",
               elapsed() - t_start, r.qps);
    }

    /* ============================================================= */
    /*  2) IVFFlat  (vary nprobe)                                    */
    /* ============================================================= */
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

            double t1 = elapsed();
            ivf_flat.search(nq, xq, k, D, I);
            double t_search = elapsed() - t1;

            float recall = compute_recall(I, gt_ids, nq, k);
            double qps = nq / t_search;

            char params[64];
            snprintf(params, sizeof(params), "nlist=%d nprobe=%d", nlist,
                     nprobe);

            all_results.push_back({"IVFFlat", params, recall, qps, t_build});
            fprintf(csv, "IVFFlat,%s,%.6f,%.1f,%.3f\n", params, recall, qps,
                    t_build);
            printf("[%.3f s] IVFFlat  %-22s recall=%.4f  QPS=%9.1f\n",
                   elapsed() - t_start, params, recall, qps);
        }
    }

    /* ============================================================= */
    /*  3) IVFPQ  (vary nprobe)                                      */
    /* ============================================================= */
    {
        int nlist = 100;
        int m = 8; // PQ sub-vectors (m * 8 bits ≈ code size)

        printf("[%.3f s] Training IVFPQ(nlist=%d, m=%d) ...\n",
               elapsed() - t_start, nlist, m);

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

            double t1 = elapsed();
            ivf_pq.search(nq, xq, k, D, I);
            double t_search = elapsed() - t1;

            float recall = compute_recall(I, gt_ids, nq, k);
            double qps = nq / t_search;

            char params[64];
            snprintf(params, sizeof(params), "nlist=%d nprobe=%d m=%d", nlist,
                     nprobe, m);

            all_results.push_back({"IVFPQ", params, recall, qps, t_build});
            fprintf(csv, "IVFPQ,%s,%.6f,%.1f,%.3f\n", params, recall, qps,
                    t_build);
            printf("[%.3f s] IVFPQ   %-22s recall=%.4f  QPS=%9.1f\n",
                   elapsed() - t_start, params, recall, qps);
        }
    }

    /* ============================================================= */
    /*  4) HNSW  (vary efSearch)                                     */
    /* ============================================================= */
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

            double t1 = elapsed();
            hnsw.search(nq, xq, k, D, I);
            double t_search = elapsed() - t1;

            float recall = compute_recall(I, gt_ids, nq, k);
            double qps = nq / t_search;

            char params[64];
            snprintf(params, sizeof(params), "M=%d efSearch=%d", M, efSearch);

            all_results.push_back({"HNSW", params, recall, qps, t_build});
            fprintf(csv, "HNSW,%s,%.6f,%.1f,%.3f\n", params, recall, qps,
                    t_build);
            printf("[%.3f s] HNSW    %-22s recall=%.4f  QPS=%9.1f\n",
                   elapsed() - t_start, params, recall, qps);
        }
    }

    /* ---- Close CSV ---- */
    fclose(csv);

    /* ---- Print summary table ---- */
    printf("\n");
    printf("============================================================\n");
    printf("RESULTS  (saved to bench_results.csv)\n");
    printf("============================================================\n");
    printf("%-10s %-24s %10s %12s %12s\n", "Index", "Params", "Recall", "QPS",
           "Build(s)");
    printf("------------------------------------------------------------\n");
    for (const auto& r : all_results) {
        printf("%-10s %-24s %10.4f %12.1f %12.3f\n", r.index_name.c_str(),
               r.params.c_str(), r.recall, r.qps, r.build_time);
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
