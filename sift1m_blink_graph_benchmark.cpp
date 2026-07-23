/*
 * Standalone SIFT1M QPS/recall benchmark for FalconSearch BlinkGraph.
 *
 * This is intentionally a client of the existing builders/searchers.  It does
 * not modify or duplicate any search algorithm implementation.
 */

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "common/shard_format/bundles/bundle_file_reader.h"
#include "common/shard_format/bundles/bundle_file_writer.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_erq_builder.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_erq_searcher_adaptive.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_rabitq_builder.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_rabitq_searcher.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_rabitq_searcher_adaptive.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_zsq_builder.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_zsq_searcher_adaptive.h"
#include "gflags/gflags.h"

namespace Falcon::Common::ShardFormat::FusionIndex {
namespace {

using Clock = std::chrono::steady_clock;

template<typename T>
bool ReadVecs(const std::string& path, uint32_t expectedDim, uint32_t limit,
              std::vector<std::vector<T>>& output)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "Cannot open " << path << std::endl;
        return false;
    }
    while (limit == 0 || output.size() < limit) {
        uint32_t dim = 0;
        input.read(reinterpret_cast<char*>(&dim), sizeof(dim));
        if (input.eof()) {
            break;
        }
        if (!input || dim != expectedDim) {
            std::cerr << "Invalid vector dimension in " << path << ": got " << dim
                      << ", expected " << expectedDim << std::endl;
            return false;
        }
        std::vector<T> vector(dim);
        input.read(reinterpret_cast<char*>(vector.data()), sizeof(T) * dim);
        if (!input) {
            std::cerr << "Truncated vector in " << path << std::endl;
            return false;
        }
        output.emplace_back(std::move(vector));
    }
    return !output.empty();
}

bool ReadBase(const std::string& path, uint32_t dim, uint32_t limit,
              std::vector<std::shared_ptr<float[]>>& output)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        std::cerr << "Cannot open " << path << std::endl;
        return false;
    }
    while (limit == 0 || output.size() < limit) {
        uint32_t vectorDim = 0;
        input.read(reinterpret_cast<char*>(&vectorDim), sizeof(vectorDim));
        if (input.eof()) {
            break;
        }
        if (!input || vectorDim != dim) {
            std::cerr << "Invalid vector dimension in " << path << ": got " << vectorDim
                      << ", expected " << dim << std::endl;
            return false;
        }
        std::shared_ptr<float[]> point(new float[dim], std::default_delete<float[]>());
        input.read(reinterpret_cast<char*>(point.get()), sizeof(float) * dim);
        if (!input) {
            std::cerr << "Truncated vector in " << path << std::endl;
            return false;
        }
        output.emplace_back(std::move(point));
    }
    return !output.empty();
}

std::vector<uint32_t> ParseRanges(const std::string& text)
{
    std::vector<uint32_t> ranges;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ',')) {
        if (!token.empty()) {
            ranges.push_back(static_cast<uint32_t>(std::stoul(token)));
        }
    }
    return ranges;
}

SectionConfig MakeConfig(uint32_t dim, uint32_t threads, uint32_t linkRange,
                         uint32_t searchRange, uint32_t batchSize, uint32_t rotatorType)
{
    SectionConfig config;
    auto* fusion = config.mutable_fusion_section_config();
    fusion->set_dimension(dim);
    fusion->set_distance_metric_type(DistanceMetricType::L2);
    fusion->set_thread_count(threads);
    auto* blink = fusion->mutable_embedding_section_config()
                      ->mutable_graph_section_config()
                      ->mutable_blink_graph_config();
    blink->set_link_range(linkRange);
    blink->set_batch_size(batchSize);
    blink->set_link_candidate_size(300);
    blink->set_build_iter_count(3);
    blink->set_search_range(searchRange);
    blink->mutable_rabitq_section_config()->set_ext_bit_len(8);
    blink->mutable_rabitq_section_config()->set_rotator_type(static_cast<RotatorType>(rotatorType));
    return config;
}

bool BuildIndex(uint32_t indexType, const SectionConfig& config, const std::string& basePath,
                const std::string& indexPath, uint32_t dim, uint32_t baseLimit)
{
    std::vector<std::shared_ptr<float[]>> points;
    if (!ReadBase(basePath, dim, baseLimit, points)) {
        return false;
    }
    std::cout << "Building index from " << points.size() << " vectors" << std::endl;
    const auto start = Clock::now();
    std::shared_ptr<BundleFileWriter> writer = std::move(BundleFileWriter::New(indexPath));
    bool ok = false;
    if (indexType == 0) {
        BlinkGraphRaBitQBuilder builder;
        ok = builder.Init(config) && builder.Build(points) && builder.Save(writer);
    } else if (indexType == 1) {
        BlinkGraphZSQBuilder builder;
        ok = builder.Init(config) && builder.Build(points) && builder.Save(writer);
    } else if (indexType == 2) {
        BlinkGraphERQBuilder builder;
        ok = builder.Init(config) && builder.Build(points) && builder.Save(writer);
    }
    const double seconds = std::chrono::duration<double>(Clock::now() - start).count();
    std::cout << "Build completed: ok=" << ok << ", seconds=" << seconds << std::endl;
    return ok;
}

std::unique_ptr<BlinkGraphRaBitQSearcherInterface> MakeSearcher(uint32_t indexType, uint32_t batchSize)
{
    if (indexType == 0 && batchSize == 0) {
        return std::make_unique<BlinkGraphRaBitQSearcher>();
    }
    if (indexType == 0) {
        return std::make_unique<BlinkGraphRaBitQSearcherAdaptive>();
    }
    if (indexType == 1) {
        return std::make_unique<BlinkGraphZSQSearcherAdaptive>();
    }
    if (indexType == 2) {
        return std::make_unique<BlinkGraphERQSearcherAdaptive>();
    }
    return nullptr;
}

bool Benchmark(const std::string& indexPath, uint32_t indexType, uint32_t batchSize,
               const SectionConfig& config, const std::vector<std::vector<float>>& queries,
               const std::vector<std::vector<uint32_t>>& truth, uint32_t topK,
               uint32_t warmup, uint32_t repeats, double& qps, double& recall)
{
    std::vector<BundleStorageInfo> bundles;
    auto reader = std::make_shared<BundleFileReader>(indexPath, ReaderType::MEM_ONLY, bundles);
    auto searcher = MakeSearcher(indexType, batchSize);
    if (!searcher || !searcher->Init(config, reader)) {
        return false;
    }
    const uint32_t warmupCount = std::min<uint32_t>(warmup, queries.size());
    for (uint32_t i = 0; i < warmupCount; ++i) {
        searcher->Search(queries[i].data(), topK);
    }

    uint64_t hits = 0;
    double searchSeconds = 0.0;
    for (uint32_t repeat = 0; repeat < repeats; ++repeat) {
        for (uint32_t i = 0; i < queries.size(); ++i) {
            const auto searchStart = Clock::now();
            const auto found = searcher->Search(queries[i].data(), topK);
            searchSeconds += std::chrono::duration<double>(Clock::now() - searchStart).count();
            std::set<uint32_t> uniqueFound;
            for (const auto& item : found) {
                uniqueFound.insert(item.second);
            }
            for (uint32_t j = 0; j < topK; ++j) {
                if (uniqueFound.count(truth[i][j]) != 0) {
                    ++hits;
                }
            }
        }
    }
    const double searches = static_cast<double>(queries.size()) * repeats;
    qps = searches / searchSeconds;
    recall = static_cast<double>(hits) / (searches * topK);
    return true;
}

}  // namespace
}  // namespace Falcon::Common::ShardFormat::FusionIndex

DEFINE_string(base, "dataset/sift_base.fvecs", "SIFT base .fvecs path");
DEFINE_string(query, "dataset/sift_query.fvecs", "SIFT query .fvecs path");
DEFINE_string(groundtruth, "dataset/sift_groundtruth.ivecs", "SIFT ground truth .ivecs path");
DEFINE_string(index, "dataset/sift1m_blink_graph.index", "Falcon index output/input path");
DEFINE_string(csv, "sift1m_arm_qps_recall.csv", "CSV result path");
DEFINE_bool(build, false, "Build the index before benchmarking");
DEFINE_uint32(index_type, 2, "0=RaBitQ, 1=ZSQ, 2=ERQ");
DEFINE_uint32(dim, 128, "Vector dimension");
DEFINE_uint32(base_limit, 0, "Base vector limit; 0 means the complete SIFT1M");
DEFINE_uint32(query_limit, 10000, "Query count; 0 means all");
DEFINE_uint32(top_k, 10, "Recall@K and search result count");
DEFINE_uint32(thread_count, 4, "Falcon index build thread count");
DEFINE_uint32(link_range, 32, "BlinkGraph link range");
DEFINE_string(search_ranges, "20,40,80,120,160,200,300,400", "Comma-separated search_range values");
DEFINE_uint32(batch_size, 1024, "Adaptive search batch size; zero selects non-adaptive RaBitQ");
DEFINE_uint32(rotator_type, 0, "0=matrix, 1=FHT KAC");
DEFINE_uint32(warmup, 200, "Warm-up query count, excluded from QPS");
DEFINE_uint32(repeats, 3, "Number of measured full-query passes");

int main(int argc, char** argv)
{
    using namespace Falcon::Common::ShardFormat::FusionIndex;
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    if (FLAGS_index_type > 2 || FLAGS_top_k == 0 || FLAGS_repeats == 0) {
        std::cerr << "Invalid index_type/top_k/repeats" << std::endl;
        return 2;
    }
    auto ranges = ParseRanges(FLAGS_search_ranges);
    if (ranges.empty()) {
        std::cerr << "search_ranges must not be empty" << std::endl;
        return 2;
    }
    SectionConfig buildConfig = MakeConfig(FLAGS_dim, FLAGS_thread_count, FLAGS_link_range,
                                           ranges.front(), FLAGS_batch_size, FLAGS_rotator_type);
    if (FLAGS_build &&
        !BuildIndex(FLAGS_index_type, buildConfig, FLAGS_base, FLAGS_index, FLAGS_dim, FLAGS_base_limit)) {
        return 1;
    }

    std::vector<std::vector<float>> queries;
    std::vector<std::vector<uint32_t>> truth;
    if (!ReadVecs(FLAGS_query, FLAGS_dim, FLAGS_query_limit, queries) ||
        !ReadVecs(FLAGS_groundtruth, 100, FLAGS_query_limit, truth)) {
        return 1;
    }
    const size_t count = std::min(queries.size(), truth.size());
    queries.resize(count);
    truth.resize(count);
    if (count == 0 || FLAGS_top_k > truth.front().size()) {
        std::cerr << "No queries, or top_k exceeds ground-truth width" << std::endl;
        return 2;
    }

    std::ofstream csv(FLAGS_csv);
    if (!csv) {
        std::cerr << "Cannot create " << FLAGS_csv << std::endl;
        return 1;
    }
    csv << "architecture,index_type,dim,link_range,batch_size,rotator_type,build_threads,"
           "search_range,top_k,queries,repeats,qps,avg_latency_us,recall\n";
#if defined(__aarch64__)
    const char* architecture = "aarch64";
#elif defined(__x86_64__)
    const char* architecture = "x86_64";
#else
    const char* architecture = "other";
#endif
    for (uint32_t range : ranges) {
        SectionConfig config = MakeConfig(FLAGS_dim, FLAGS_thread_count, FLAGS_link_range, range,
                                          FLAGS_batch_size, FLAGS_rotator_type);
        double qps = 0.0;
        double recall = 0.0;
        if (!Benchmark(FLAGS_index, FLAGS_index_type, FLAGS_batch_size, config, queries, truth,
                       FLAGS_top_k, FLAGS_warmup, FLAGS_repeats, qps, recall)) {
            return 1;
        }
        std::cout << "search_range=" << range << " QPS=" << qps
                  << " avg_latency_us=" << (1000000.0 / qps)
                  << " Recall@" << FLAGS_top_k << "=" << recall << std::endl;
        csv << architecture << ',' << FLAGS_index_type << ',' << FLAGS_dim << ',' << FLAGS_link_range << ','
            << FLAGS_batch_size << ',' << FLAGS_rotator_type << ',' << FLAGS_thread_count << ',' << range << ','
            << FLAGS_top_k << ',' << count << ',' << FLAGS_repeats << ',' << qps << ','
            << (1000000.0 / qps) << ',' << recall << '\n';
        csv.flush();
    }
    return 0;
}
