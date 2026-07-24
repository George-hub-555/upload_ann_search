/*
 * bench_fastscan.cpp — FAISS IVFPQ vs IVFPQFastScan benchmark for aarch64
 *
 * Compares standard IVFPQ with the SIMD-accelerated IVFPQ FastScan variant
 * on the same dataset, same nlist, same m, same nprobe sweep.
 *
 * Compile (from faiss-main/):
 *   g++ -O3 -march=armv8-a -std=c++17 -I. -o bench_fastscan bench_fastscan.cpp \
 *       -Lbuild/faiss -lfaiss -lopenblas -lpthread -Wl,-rpath,build/faiss
 *
 * Run:
 *   # GloVe dataset (text format)
 *   ./bench_fastscan -dataset dataset/glove.twitter.27B.200d.txt -k 10 -nlist 4096 -nprobe_sweep 1,4,16,64,256 -reportfreq 1000
 *
 *   # SIFT dataset (fvecs format)
 *   ./bench_fastscan -dataset sift1M/sift_base.fvecs -learn sift1M/sift_learn.fvecs -query sift1M/sift_query.fvecs -gt sift1M/sift_groundtruth.ivecs -fvecs -k 10 -nlist 4096 -m 16
 *
 * All flags are optional:
 *   Common:
 *     -dataset       <path>  Dataset file path  (required)
 *     -k             <int>   top-k recall   (default: 10)
 *     -nlist         <int>   IVF centroids  (default: 1024)
 *     -m             <int>   PQ sub-quantizers (default: auto from dataset dim)
 *     -maxturn       <int>   max vectors to use  (default: all)
 *     -nprobe_sweep  <str>   comma-separated nprobe values  (default: "1,2,4,8,16,32,64")
 *     -reportfreq    <int>   progress every N queries  (default: 0 = off)
 *
 *   GloVe text format (default):
 *     -dataset  dataset/glove.twitter.27B.25d.txt
 *
 *   SIFT fvecs format (use -fvecs flag and provide learn/query/gt):
 *     -fvecs                    enable fvecs/ivecs format
 *     -learn   <path>           training vectors  (sift_learn.fvecs)
 *     -query   <path>           query vectors     (sift_query.fvecs)
 *     -gt      <path>           ground truth       (sift_groundtruth.ivecs)
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <sys/stat.h>

#include <faiss/IndexFlat.h>
#include <faiss/IndexIVFPQ.h>
#include <faiss/IndexIVFPQFastScan.h>
#include <faiss/IndexRefine.h>

using idx_t = faiss::idx_t;

/* =============================================================
 *  Argument parser
 * ============================================================= */
static const char* arg_get_str(int argc, char** argv, const char* flag,
                                const char* def) {
    for (int i = 1; i < argc - 1; i++)
        if (strcmp(argv[i], flag) == 0)
            return argv[i + 1];
    return def;
}

static int arg_get_int(int argc, char** argv, const char* flag, int def) {
    const char* v = arg_get_str(argc, argv, flag, nullptr);
    return v ? atoi(v) : def;
}

static bool arg_has(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; i++)
        if (strcmp(argv[i], flag) == 0)
            return true;
    return false;
}

/* Parse comma-separated integers into a vector */
static std::vector<int> parse_int_list(const char* s) {
    std::vector<int> out;
    std::string str(s);
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ',')) {
        out.push_back(std::stoi(token));
    }
    return out;
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
 *  GloVe text reader
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
    size_t d = 0, n = 0;
    while (std::getline(fin, line)) {
        if (max_vectors > 0 && n >= max_vectors)
            break;
        if (line.empty())
            continue;
        std::istringstream iss(line);
        std::string word;
        iss >> word;
        float val;
        size_t cnt = 0;
        while (iss >> val) {
            all_data.push_back(val);
            cnt++;
        }
        if (n == 0)
            d = cnt;
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
 *  fvecs / ivecs readers (SIFT format)
 * ============================================================= */
float* fvecs_read(const char* fname, size_t* d_out, size_t* n_out) {
    FILE* f = fopen(fname, "rb");
    if (!f) {
        fprintf(stderr, "Error: could not open %s\n", fname);
        perror("");
        exit(1);
    }
    int d;
    fread(&d, 1, sizeof(int), f);
    assert((d > 0 && d < 1000000) && "unreasonable dimension");
    fseek(f, 0, SEEK_SET);
    struct stat st;
    fstat(fileno(f), &st);
    size_t sz = st.st_size;
    assert(sz % ((d + 1) * 4) == 0 && "weird file size");
    size_t n = sz / ((d + 1) * 4);
    *d_out = d;
    *n_out = n;
    float* x = new float[n * (d + 1)];
    size_t nr __attribute__((unused)) = fread(x, sizeof(float), n * (d + 1), f);
    assert(nr == n * (d + 1) && "could not read whole file");
    for (size_t i = 0; i < n; i++)
        memmove(x + i * d, x + 1 + i * (d + 1), d * sizeof(*x));
    fclose(f);
    return x;
}

int* ivecs_read(const char* fname, size_t* d_out, size_t* n_out) {
    return (int*)fvecs_read(fname, d_out, n_out);
}

/* =============================================================
 *  Pick the best m for PQ
 * ============================================================= */
int best_pq_m(size_t d, int prefer_m) {
    if (prefer_m > 0 && d % (size_t)prefer_m == 0)
        return prefer_m;
    int best = 1;
    for (int m = 1; m <= (int)std::min(d, size_t(64)); m++) {
        if (d % m == 0) {
            if (best == 1 ||
                std::abs(m - prefer_m) < std::abs(best - prefer_m))
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
    for (size_t i = 0; i < nq; i++)
        for (size_t j = 0; j < k; j++)
            if (I[i * k + j] == gt[i * k + j])
                correct++;
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
    std::string dataset_path = arg_get_str(argc, argv, "-dataset",
        "dataset/glove.twitter.27B.25d.txt");
    std::string learn_path  = arg_get_str(argc, argv, "-learn", "");
    std::string query_path  = arg_get_str(argc, argv, "-query", "");
    std::string gt_path     = arg_get_str(argc, argv, "-gt", "");
    bool use_fvecs          = arg_has(argc, argv, "-fvecs");
    int k                   = arg_get_int(argc, argv, "-k", 10);
    int nlist               = arg_get_int(argc, argv, "-nlist", 1024);
    int prefer_m            = arg_get_int(argc, argv, "-m", 0);
    size_t maxturn          = (size_t)arg_get_int(argc, argv, "-maxturn", 0);
    int reportfreq          = arg_get_int(argc, argv, "-reportfreq", 0);
    int k_reorder           = arg_get_int(argc, argv, "-k_reorder", 0);

    std::vector<int> nprobe_vals = {1, 2, 4, 8, 16, 32, 64};
    const char* nprobe_str = arg_get_str(argc, argv, "-nprobe_sweep", nullptr);
    if (nprobe_str)
        nprobe_vals = parse_int_list(nprobe_str);

    if (arg_has(argc, argv, "-h") || arg_has(argc, argv, "--help")) {
        printf("Usage: %s [flags]\n", argv[0]);
        printf("  -dataset       <path>  Dataset file path\n");
        printf("  -k             <int>   top-k recall   (default: 10)\n");
        printf("  -nlist         <int>   IVF centroids  (default: 1024)\n");
        printf("  -m             <int>   PQ sub-quantizers (default: auto)\n");
        printf("  -maxturn       <int>   max vectors   (default: all)\n");
        printf("  -nprobe_sweep  <str>   nprobe values, comma-sep  (default: 1,2,4,8,16,32,64)\n");
        printf("  -reportfreq    <int>   progress every N queries  (default: 0)\n");
        printf("  -k_reorder     <int>   FastScan refine k_factor  (default: 0 = off)\n");
        printf("\n");
        printf("  SIFT fvecs format:\n");
        printf("    -fvecs               enable fvecs/ivecs format\n");
        printf("    -learn   <path>      training vectors\n");
        printf("    -query   <path>      query vectors\n");
        printf("    -gt      <path>      ground truth  (.ivecs)\n");
        return 0;
    }

    printf("============================================================\n");
    printf("FAISS IVFPQ vs IVFPQFastScan Benchmark  (aarch64)\n");
    printf("============================================================\n");

    /* ---- Load dataset ---- */
    size_t d = 0, n_total = 0, nq = 0, nt = 0;
    float *xb = nullptr, *xt = nullptr, *xq = nullptr;
    size_t nb = 0;
    idx_t* gt_ids_ext = nullptr; // external ground truth (fvecs mode)

    if (use_fvecs) {
        /* ---- SIFT fvecs format ---- */
        printf("[%.3f s] Loading SIFT dataset (fvecs format)\n",
               elapsed() - t_start);

        size_t d1, n1;
        xb = fvecs_read(dataset_path.c_str(), &d, &nb);
        printf("  database: %zu vectors, dim %zu\n", nb, d);

        if (!learn_path.empty()) {
            xt = fvecs_read(learn_path.c_str(), &d1, &nt);
            printf("  train:    %zu vectors, dim %zu\n", nt, d1);
        } else {
            nt = nb;
            xt = xb;
            printf("  train:    using database (no -learn given)\n");
        }

        if (!query_path.empty()) {
            xq = fvecs_read(query_path.c_str(), &d1, &nq);
            printf("  queries:  %zu vectors, dim %zu\n", nq, d1);
        } else {
            nq = 10000;
            if (nq > nb / 3)
                nq = nb / 10;
            if (nq < 100)
                nq = 100;
            nq = std::min(nq, nb / 2);
            nb -= nq;
            xq = xb + nb * d;
            printf("  queries:  %zu (taken from end of database)\n", nq);
        }

        if (!gt_path.empty()) {
            size_t dd;
            gt_ids_ext = (idx_t*)ivecs_read(gt_path.c_str(), &dd, &nq);
            printf("  ground truth loaded: %zu queries, dim %zu\n", nq, dd);
        }

    } else {
        /* ---- GloVe text format ---- */
        printf("[%.3f s] Loading GloVe dataset: %s\n", elapsed() - t_start,
               dataset_path.c_str());

        size_t n_all;
        float* all_data = read_glove(dataset_path, d, n_all, maxturn);

        nq = 10000;
        if (nq > n_all / 3)
            nq = n_all / 10;
        if (nq < 100)
            nq = 100;

        nb = n_all - nq;
        nt = nb;
        xb = all_data;
        xt = xb;
        xq = all_data + nb * d;
        printf("  using all %zu vectors for training\n", nt);
    }

    printf("[%.3f s] Split: db=%zu  train=%zu  queries=%zu  "
           "dim=%zu  k=%d  nlist=%d\n",
           elapsed() - t_start, nb, nt, nq, d, k, nlist);

    /* ---- Resolve PQ m ---- */
    int m = best_pq_m(d, prefer_m);
    printf("[%.3f s] PQ m=%d (dim=%zu, prefer_m=%d)\n",
           elapsed() - t_start, m, d, prefer_m);

    /* ---- Compute / load ground truth ---- */
    idx_t* gt_ids = nullptr;
    float* gt_dist = nullptr;
    double t_gt_qps = 0;

    if (gt_ids_ext) {
        /* Use pre-loaded ground truth from fvecs */
        gt_ids = new idx_t[k * nq];
        for (size_t i = 0; i < nq; i++)
            for (int j = 0; j < k; j++)
                gt_ids[i * k + j] = gt_ids_ext[i * k + j];
        printf("[%.3f s] Using external ground truth\n", elapsed() - t_start);
    } else {
        /* Compute via FlatL2 brute force */
        printf("[%.3f s] Computing ground truth (IndexFlatL2) ...\n",
               elapsed() - t_start);

        faiss::IndexFlatL2 flat_index(d);
        flat_index.add(nb, xb);

        gt_ids = new idx_t[k * nq];
        gt_dist = new float[k * nq];

        double t0 = elapsed();
        flat_index.search(nq, xq, k, gt_dist, gt_ids);
        double dt = elapsed() - t0;
        t_gt_qps = nq / dt;
        printf("[%.3f s] Ground truth done  (%.3f s,  %.0f QPS)\n",
               elapsed() - t_start, dt, t_gt_qps);
    }

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

    /* ---- Flat baseline (if GloVe mode) ---- */
    if (gt_dist == nullptr) {
        gt_dist = new float[k * nq];
    }
    if (t_gt_qps > 0) {
        BenchResult r{"Flat", "", 1.0f, t_gt_qps, 0.0};
        all_results.push_back(r);
        fprintf(csv, "Flat,,%.6f,%.1f,%.3f\n", r.recall, r.qps, r.build_time);
        printf("[%.3f s] Flat:                recall=1.0000  QPS=%9.1f\n",
               elapsed() - t_start, r.qps);
    }

    /* =============================================================
     *  run_search lambda with progress CSV
     * ============================================================= */
    auto run_search = [&](const char* label, const char* params_tag,
                          faiss::Index& index, int nq_batch,
                          double& out_recall, double& out_qps) {

        if (reportfreq <= 0 || nq_batch <= reportfreq) {
            double t0 = elapsed();
            index.search(nq_batch, xq, k, D, I);
            double dt = elapsed() - t0;
            out_qps    = nq_batch / dt;
            out_recall = compute_recall(I, gt_ids, nq_batch, k);
            return;
        }

        std::string tag = std::string(label) + "_" + params_tag;
        for (auto& c : tag) {
            if (c == ' ' || c == '=' || c == ',' || c == '.')
                c = '_';
        }
        char prog_fname[256];
        snprintf(prog_fname, sizeof(prog_fname),
                 "progress_%s.csv", tag.c_str());

        int batch_size = reportfreq;
        int n_batches  = (nq_batch + batch_size - 1) / batch_size;
        size_t cumul_correct = 0;
        double cumul_time = 0.0;

        FILE* pfp = fopen(prog_fname, "w");
        if (pfp)
            fprintf(pfp, "batch,queries_done,batch_time_s,"
                         "cumul_recall,cumul_qps\n");

        printf("  [%s] running %d queries in %d batches of %d  "
               "(progress -> %s)\n",
               label, nq_batch, n_batches, batch_size, prog_fname);

        for (int b = 0; b < n_batches; b++) {
            int offset = b * batch_size;
            int this_batch = std::min(batch_size, nq_batch - offset);
            float* xq_batch  = xq + offset * d;
            idx_t* I_batch   = I + offset * k;
            float* D_batch   = D + offset * k;
            idx_t* gt_batch  = gt_ids + offset * k;

            double t0 = elapsed();
            index.search(this_batch, xq_batch, k, D_batch, I_batch);
            double dt = elapsed() - t0;
            cumul_time += dt;

            size_t batch_correct = 0;
            for (int i = 0; i < this_batch; i++)
                for (int j = 0; j < k; j++)
                    if (I_batch[i * k + j] == gt_batch[i * k + j])
                        batch_correct++;
            cumul_correct += batch_correct;

            int done = offset + this_batch;
            double cumul_recall = double(cumul_correct) / double(done * k);
            double cumul_qps    = double(done) / cumul_time;

            printf("  [%s] batch %d/%d  queries %d/%d  "
                   "batch_time=%.3fs  cumul_recall=%.4f  cumul_QPS=%.0f\n",
                   label, b + 1, n_batches, done, nq_batch,
                   dt, cumul_recall, cumul_qps);

            if (pfp)
                fprintf(pfp, "%d,%d,%.6f,%.6f,%.2f\n",
                        b + 1, done, dt, cumul_recall, cumul_qps);
        }

        if (pfp)
            fclose(pfp);

        out_recall = double(cumul_correct) / double(nq_batch * k);
        out_qps    = double(nq_batch) / cumul_time;
    };

    /* =============================================================
     *  1) IVFPQ
     * ============================================================= */
    {
        printf("[%.3f s] Training IVFPQ  (nlist=%d, m=%d) ...\n",
               elapsed() - t_start, nlist, m);

        double t0 = elapsed();
        faiss::IndexFlatL2 quantizer_pq(d);
        faiss::IndexIVFPQ ivf_pq(&quantizer_pq, d, nlist, m, 8);
        ivf_pq.train(nt, xt);
        ivf_pq.add(nb, xb);
        double t_build = elapsed() - t0;

        printf("[%.3f s] IVFPQ built in %.3f s\n",
               elapsed() - t_start, t_build);

        for (int nprobe : nprobe_vals) {
            ivf_pq.nprobe = nprobe;

            double recall, qps;
            char params[64];
            snprintf(params, sizeof(params), "nlist=%d nprobe=%d m=%d",
                     nlist, nprobe, m);
            run_search("IVFPQ", params, ivf_pq, nq, recall, qps);

            all_results.push_back(
                {"IVFPQ", params, (float)recall, qps, t_build});
            fprintf(csv, "IVFPQ,%s,%.6f,%.1f,%.3f\n",
                    params, recall, qps, t_build);
        }
    }

    /* =============================================================
     *  2) IVFPQFastScan  (with optional k_reorder refinement)
     * ============================================================= */
    {
        double t0 = elapsed();

        printf("[%.3f s] Building IVFPQFastScan  (nlist=%d, m=%d)",
               elapsed() - t_start, nlist, m);

        faiss::IndexFlatL2 quantizer_fs(d);
        auto* ivf_fs = new faiss::IndexIVFPQFastScan(
            &quantizer_fs, d, nlist, m, 8, faiss::METRIC_L2, 32);
        ivf_fs->train(nt, xt);

        faiss::Index* search_index = nullptr;
        std::string fs_label;

        if (k_reorder > 0) {
            /* Wrap with IndexRefineFlat for two-stage re-ranking.
             * Add data through the wrapper so both base and refine get it. */
            auto* refine = new faiss::IndexRefineFlat(ivf_fs);
            refine->k_factor = (float)k_reorder;
            refine->own_fields = true;
            refine->own_refine_index = true;
            refine->add(nb, xb);   // adds to both ivf_fs and internal flat index
            search_index = refine;
            fs_label = "IVFPQFS_Refine";
            printf("  + k_reorder=%d", k_reorder);
        } else {
            ivf_fs->add(nb, xb);
            search_index = ivf_fs;
            fs_label = "IVFPQFS";
        }
        printf("\n");

        double t_build = elapsed() - t0;
        printf("[%.3f s] IVFPQFastScan built in %.3f s%s\n",
               elapsed() - t_start, t_build,
               k_reorder > 0 ? " (with refinement)" : "");

        for (int nprobe : nprobe_vals) {
            ivf_fs->nprobe = nprobe;

            double recall, qps;
            char params[64];
            if (k_reorder > 0) {
                snprintf(params, sizeof(params),
                         "nlist=%d nprobe=%d m=%d k_reorder=%d",
                         nlist, nprobe, m, k_reorder);
            } else {
                snprintf(params, sizeof(params), "nlist=%d nprobe=%d m=%d",
                         nlist, nprobe, m);
            }
            run_search(fs_label.c_str(), params, *search_index,
                       nq, recall, qps);

            all_results.push_back(
                {fs_label, params, (float)recall, qps, t_build});
            fprintf(csv, "%s,%s,%.6f,%.1f,%.3f\n",
                    fs_label.c_str(), params, recall, qps, t_build);
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
    delete[] gt_ids;
    delete[] gt_dist;
    delete[] I;
    delete[] D;
    // xb / xt / xq cleanup depends on load path — simplified

    return 0;
}
