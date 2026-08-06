/*
 * Copyright (c) 2026. All rights reserved.
 * Description: Linux aarch64 适配 falcon ZSQ/RaBitQ/Float 三种 searcher 对比测试
 *              对应 falcon blink_graph_zsq_searcher_adaptive.cpp / blink_graph_rabitq_searcher_adaptive.cpp
 *              / blink_graph_rabitq_searcher.cpp (NoBatch 真实距离 baseline)
 *
 * 运行环境: Linux aarch64 + falcon 自带 bazel 6.5.0 (devel/builder/bazel-6.5.0-linux-arm64)
 * 不依赖额外包,直接复用 falcon 的 builder/searcher 实现 (含 SVE 路径)
 *
 * 编译和测试分两台机器:
 *   A 机器 (编译机): 有 falcon + Opencode_done, 用 build.sh 编译打包
 *   B 机器 (测试机): 拷 tar 包, 用 run.sh 运行
 *
 * 目录结构 (全相对路径):
 *   某根目录/
 *   ├── falcon/                          # falcon 源码 (A 机器需要)
 *   └── Opencode_done/
 *       └── linux_falcon_zsq/            # 本目录
 *           ├── build.sh                 # A 机器: ./build.sh -> dist/*.tar.gz
 *           ├── run.sh                   # B 机器: ./run.sh smoke|full
 *           ├── BUILD
 *           └── linux_falcon_zsq_test.cpp
 *
 * A 机器流程:
 *   cd Opencode_done/linux_falcon_zsq
 *   ./build.sh                # 编译 + 打包
 *   ./build.sh --run-smoke    # 可选: A 机器直接验证 smoke
 *
 * B 机器流程:
 *   tar xzf linux_falcon_zsq_test.tar.gz
 *   cd linux_falcon_zsq_test
 *   ./run.sh smoke                            # 1k base, 数据在 runfiles
 *   ./run.sh full /path/to/sift/dataset       # SIFT-1M, 需数据目录
 *      (bazel test 默认捕获 LOG(INFO) 输出, 加 --test_output=all 可见)
 *        bazel test --config=linux_arm64 --test_output=all --spawn_strategy=local \
 *          --test_env=FALCON_SIFT_DIR=$(pwd)/falcon/dataset \
 *          --test_arg=--full //linux_falcon_zsq:linux_falcon_zsq_test
 */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <random>
#include <vector>

#include "common/public/utils/common_macro.h"
#include "common/public/utils/proto_util.h"
#include "common/public/utils/logging.h"
#include "common/shard_format/bundles/bundle_file_writer.h"
#include "common/shard_format/bundles/bundle_file_reader.h"
#include "common/shard_format/bundles/bundle_ids.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_rabitq_builder.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_rabitq_searcher.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_rabitq_searcher_adaptive.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_rabitq_searcher_interface.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_zsq_builder.h"
#include "common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec/blink_graph_zsq_searcher_adaptive.h"
#include "common/shard_format/utils/utils.h"
#include "common/utils/filesystem/scoped_temp_file.h"
#include "common/utils/rand_generator.h"
#include "gtest/gtest.h"

namespace Falcon::Common::ShardFormat::FusionIndex {
namespace {

// ann-benchmarks fvecs/ivecs 加载 (4 字节 int32 dim 前缀 + d 个值)
// falcon 自带的 GetData 假设无前缀, 这里适配 dataset/sift_*.fvecs 的有前缀格式
bool LoadFvecs(const std::string& path, std::vector<std::shared_ptr<float[]>>& values, int count, int dim)
{
    std::ifstream fin(path, std::ios::binary);
    RETURN_FALSE_IF_FALSE_WITH_LOG(fin.is_open(), "open failed: " + path);
    for (int i = 0; i < count; ++i) {
        int32_t d = 0;
        fin.read(reinterpret_cast<char*>(&d), sizeof(int32_t));
        RETURN_FALSE_IF_FALSE_WITH_LOG(fin.good() && d == dim, "dim mismatch or read failed");
        std::shared_ptr<float[]> data(new float[dim], std::default_delete<float[]>());
        fin.read(reinterpret_cast<char*>(data.get()), dim * sizeof(float));
        RETURN_FALSE_IF_FALSE_WITH_LOG(fin.good(), "read float data failed");
        values.push_back(data);
    }
    return true;
}

bool LoadIvecs(const std::string& path, std::vector<std::vector<int>>& values, int count, int maxK)
{
    std::ifstream fin(path, std::ios::binary);
    RETURN_FALSE_IF_FALSE_WITH_LOG(fin.is_open(), "open failed: " + path);
    for (int i = 0; i < count; ++i) {
        int32_t d = 0;
        fin.read(reinterpret_cast<char*>(&d), sizeof(int32_t));
        RETURN_FALSE_IF_FALSE_WITH_LOG(fin.good() && d > 0, "read dim failed");
        int32_t take = std::min(static_cast<int>(d), maxK);
        std::vector<int> gt(take);
        fin.read(reinterpret_cast<char*>(gt.data()), take * sizeof(int32_t));
        RETURN_FALSE_IF_FALSE_WITH_LOG(fin.good(), "read gt data failed");
        if (take < d) {
            fin.ignore((d - take) * sizeof(int32_t));
        }
        values.push_back(gt);
    }
    return true;
}

// Float baseline searcher: 继承 BlinkGraphRaBitQSearcher (NoBatch 模式),
// 覆盖 Search 直接用真实 L2 距离 (GetDist) 做粗筛, 不走 RaBitQ BatchEstimate
// falcon 没有 "不量化" 的 searcher, 这是最接近的 baseline
// 不修改 falcon 源码, 在本测试文件内定义
class FloatSearcher : public BlinkGraphRaBitQSearcher {
public:
    FloatSearcher() = default;
    ~FloatSearcher() override = default;

    std::vector<std::pair<float, uint32_t>> Search(const float* query, uint32_t topK) const override
    {
        std::vector<std::pair<float, uint32_t>> resultVec;
        CandidateBuffer<float> candidatePool(ef_);
        candidatePool.Insert(enterPoint_, std::numeric_limits<float>::max());
        CandidateBuffer<float> resPool(topK);
        auto* vl = visitedListPool_->GetFreeVisitedList();
        while (candidatePool.HasNext()) {
            auto curNode = candidatePool.Pop();
            if (vl->Get(curNode)) {
                continue;
            }
            vl->Set(curNode);
            // 真实距离 (对应 falcon ScanNeighbors 中的 GetDist)
            // query 不旋转: L2 在正交旋转下不变, 直接用原始 query + 原始 float embedding
            float curDist = GetDist(query, GetEmbedding(curNode));
            resPool.Insert(curNode, curDist);
            // 邻居用真实距离粗筛 (区别于 RaBitQ 用 BatchEstimate 估计距离)
            const uint32_t* nnPtr = GetNeighbor(curNode);
            for (uint32_t i = 0; i < m_; ++i) {
                uint32_t nn = nnPtr[i];
                CONTINUE_WHEN(vl->Get(nn));
                float dist = GetDist(query, GetEmbedding(nn));
                CONTINUE_WHEN(candidatePool.IsFull(dist));
                candidatePool.Insert(nn, dist);
            }
        }
        UpdateResults(query, resPool, vl);
        visitedListPool_->ReleaseVisitedList(vl);
        resPool.GetResult(resultVec);
        return resultVec;
    }
};

struct SearchStats {
    std::string name;
    int nQuery = 0;
    int topK = 0;
    int recall = 0;
    double recallRatio = 0.0;
    double qps = 0.0;
    double meanLatencyUs = 0.0;
    double p50 = 0.0;
    double p70 = 0.0;
    double p80 = 0.0;
    double p90 = 0.0;
    double p95 = 0.0;
    double p99 = 0.0;
};

double Percentile(std::vector<double>& sorted, double p)
{
    if (sorted.empty()) {
        return 0.0;
    }
    size_t idx = static_cast<size_t>(p / 100.0 * sorted.size());
    if (idx >= sorted.size()) {
        idx = sorted.size() - 1;
    }
    return sorted[idx];
}

SearchStats RunSearch(BlinkGraphRaBitQSearcherInterface* searcher, const std::vector<std::vector<float>>& queries,
                      const std::vector<std::vector<int>>& groundTruth, int topK, const std::string& name)
{
    SearchStats stats;
    stats.name = name;
    stats.nQuery = static_cast<int>(queries.size());
    stats.topK = topK;
    std::vector<double> latencies;
    latencies.reserve(queries.size());
    uint64_t totalNs = 0;
    for (int i = 0; i < stats.nQuery; ++i) {
        auto t1 = std::chrono::high_resolution_clock::now();
        auto res = searcher->Search(queries[i].data(), topK);
        auto t2 = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration<double, std::micro>(t2 - t1).count();
        latencies.push_back(us);
        totalNs += static_cast<uint64_t>(us * 1000.0);  // ns
        for (auto& r : res) {
            auto it = std::find(groundTruth[i].begin(), groundTruth[i].end(), static_cast<int>(r.second));
            if (it != groundTruth[i].end()) {
                stats.recall++;
            }
        }
    }
    double totalS = static_cast<double>(totalNs) / 1e9;
    stats.qps = stats.nQuery / totalS;
    stats.recallRatio = 1.0 * stats.recall / (stats.nQuery * topK);
    std::sort(latencies.begin(), latencies.end());
    stats.meanLatencyUs = latencies.empty() ? 0.0
                          : std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
    stats.p50 = Percentile(latencies, 50);
    stats.p70 = Percentile(latencies, 70);
    stats.p80 = Percentile(latencies, 80);
    stats.p90 = Percentile(latencies, 90);
    stats.p95 = Percentile(latencies, 95);
    stats.p99 = Percentile(latencies, 99);
    return stats;
}

void PrintStats(const SearchStats& s)
{
    LOG(INFO) << "[" << s.name << "] QPS=" << s.qps << " R@" << s.topK << "=" << s.recallRatio
              << " mean(us)=" << s.meanLatencyUs << " P50=" << s.p50 << " P70=" << s.p70
              << " P80=" << s.p80 << " P90=" << s.p90 << " P95=" << s.p95 << " P99=" << s.p99;
}

void PrintHeader()
{
    LOG(INFO) << "================================================================================";
    LOG(INFO) << "ZSQ vs RaBitQ vs Float Searcher Comparison (falcon real C++ impl on aarch64)";
    LOG(INFO) << "================================================================================";
}

void PrintTable(const std::vector<SearchStats>& all)
{
    LOG(INFO) << "--------------------------------------------------------------------------------";
    LOG(INFO) << "Searcher    QPS       R@K        mean(us)   P50        P70        P80        P90        "
                 "P95        P99";
    LOG(INFO) << "--------------------------------------------------------------------------------";
    for (auto& s : all) {
        int pad = std::max(0, 10 - static_cast<int>(s.name.size()));
        LOG(INFO) << s.name << std::string(pad, ' ') << std::fixed
                  << std::setprecision(1) << s.qps << "    " << std::setprecision(4) << s.recallRatio
                  << "    " << std::setprecision(1) << s.meanLatencyUs << "    " << s.p50 << "    "
                  << s.p70 << "    " << s.p80 << "    " << s.p90 << "    " << s.p95 << "    " << s.p99;
    }
    LOG(INFO) << "================================================================================";
}

} // namespace

// 测试 fixture, 复用 falcon blink_graph_rabitq_unittest.cpp 的 BlinkGraphRaBitQUnittest 模式
class FalconZSQCompareTest {
public:
    Falcon::Common::Utils::FileSystem::File* path_ = nullptr;
    Falcon::Common::Utils::FileSystem::ScopedTempFile* tmpDir_ = nullptr;
    std::string filePath_;
    std::string indexPath_;
    SectionConfig config_;
    uint32_t dim_ = 128;
    uint32_t maxElements_ = 1000;  // smoke; full 模式由 caller 覆盖
    uint32_t queryCount_ = 1000;
    uint32_t k_ = 10;
    std::default_random_engine random_;

    FalconZSQCompareTest()
    {
        random_ = std::default_random_engine(time(0));
        filePath_ = Falcon::Common::Utils::GenerateRandDirName(nullptr, "FalconZSQCompare") + "/";
        auto* file = new Falcon::Common::Utils::FileSystem::File(filePath_);
        if (!file->CreateDirectory(false)) {
            delete file;
            return;
        }
        tmpDir_ = new Falcon::Common::Utils::FileSystem::ScopedTempFile(filePath_);
        path_ = new Falcon::Common::Utils::FileSystem::File("resource/sift_");
        indexPath_ = filePath_ + "rbq.bin";
    }

    ~FalconZSQCompareTest()
    {
        delete tmpDir_;
        delete path_;
    }

    // step: batch_size, 0=NoBatch, >0=Batch
    // linkRange: 出度 (必须 32 的倍数, 见 BlinkGraphRaBitQBuilder::Init)
    void SetConfig(uint32_t linkRange, uint32_t step, uint32_t searchRange = 100)
    {
        auto* fusionConfig = config_.mutable_fusion_section_config();
        fusionConfig->set_dimension(dim_);
        fusionConfig->set_distance_metric_type(DistanceMetricType::L2);
        fusionConfig->set_thread_count(1);
        auto* blinkGraphConfig = fusionConfig->mutable_embedding_section_config()
                                     ->mutable_graph_section_config()
                                     ->mutable_blink_graph_config();
        blinkGraphConfig->set_link_range(linkRange);
        blinkGraphConfig->set_batch_size(step);
        blinkGraphConfig->set_link_candidate_size(400);
        blinkGraphConfig->set_build_iter_count(3);
        blinkGraphConfig->set_search_range(searchRange);
        LOG(INFO) << "config: " << ::Common::ProtoToString(config_);
    }

    // falcon resource/sift_*.dat (1k 子集, 无 dim 前缀)
    bool LoadSmokeData(std::vector<std::shared_ptr<float[]>>& base,
                       std::vector<std::vector<float>>& queries,
                       std::vector<std::vector<int>>& groundTruth)
    {
        auto baseDir = path_->GetPath().GetCanonicalPath();
        RETURN_FALSE_IF_FALSE(GetData<float>(baseDir + "base.dat", base, maxElements_, dim_));
        RETURN_FALSE_IF_FALSE(GetData<float>(baseDir + "query.dat", queries, queryCount_, dim_));
        RETURN_FALSE_IF_FALSE(GetData<int>(baseDir + "groundtruth.dat", groundTruth, queryCount_, k_));
        return true;
    }

    // SIFT-1M (ann-benchmarks 格式, 有 dim 前缀)
    bool LoadFullData(const std::string& dir, std::vector<std::shared_ptr<float[]>>& base,
                      std::vector<std::vector<float>>& queries, std::vector<std::vector<int>>& groundTruth)
    {
        LOG(INFO) << "loading SIFT-1M from " << dir;
        RETURN_FALSE_IF_FALSE(LoadFvecs(dir + "/sift_base.fvecs", base, maxElements_, dim_));
        LOG(INFO) << "base loaded: " << base.size();
        RETURN_FALSE_IF_FALSE(LoadFvecs(dir + "/sift_query.fvecs", queries, queryCount_, dim_));
        LOG(INFO) << "query loaded: " << queries.size();
        // groundtruth 文件每行 100 个, 只取前 k_ 个
        RETURN_FALSE_IF_FALSE(LoadIvecs(dir + "/sift_groundtruth.ivecs", groundTruth, queryCount_, 100));
        LOG(INFO) << "groundtruth loaded: " << groundTruth.size();
        return true;
    }

    // 构建 ZSQ 索引并保存到 indexPath_
    bool BuildZSQ(const std::vector<std::shared_ptr<float[]>>& base)
    {
        auto builder = std::make_unique<BlinkGraphZSQBuilder>();
        SetConfig(32, 0);  // ZSQ 用 NoBatch (step=0), 与现有 unittest ZSQ case 一致
        RETURN_FALSE_IF_FALSE_WITH_LOG(builder->Init(config_), "ZSQ builder init failed");
        RETURN_FALSE_IF_FALSE_WITH_LOG(builder->Build(base), "ZSQ build failed");
        auto writer = std::move(BundleFileWriter::New(indexPath_));
        RETURN_FALSE_IF_FALSE_WITH_LOG(builder->Save(writer), "ZSQ save failed");
        return true;
    }

    // 构建 RaBitQ 索引 (step=2, Batch 模式)
    bool BuildRaBitQBatch(const std::vector<std::shared_ptr<float[]>>& base)
    {
        auto builder = std::make_unique<BlinkGraphRaBitQBuilder>();
        SetConfig(32, 2);  // RaBitQ Batch 用 step=2
        RETURN_FALSE_IF_FALSE_WITH_LOG(builder->Init(config_), "RaBitQ builder init failed");
        RETURN_FALSE_IF_FALSE_WITH_LOG(builder->Build(base), "RaBitQ build failed");
        auto writer = std::move(BundleFileWriter::New(indexPath_));
        RETURN_FALSE_IF_FALSE_WITH_LOG(builder->Save(writer), "RaBitQ save failed");
        return true;
    }

    // 构建 RaBitQ 索引 (step=0, NoBatch 模式, Float baseline 复用此图)
    bool BuildRaBitQNoBatch(const std::vector<std::shared_ptr<float[]>>& base)
    {
        auto builder = std::make_unique<BlinkGraphRaBitQBuilder>();
        SetConfig(32, 0);  // NoBatch
        RETURN_FALSE_IF_FALSE_WITH_LOG(builder->Init(config_), "RaBitQ NoBatch builder init failed");
        RETURN_FALSE_IF_FALSE_WITH_LOG(builder->Build(base), "RaBitQ NoBatch build failed");
        auto writer = std::move(BundleFileWriter::New(indexPath_));
        RETURN_FALSE_IF_FALSE_WITH_LOG(builder->Save(writer), "RaBitQ NoBatch save failed");
        return true;
    }

    // 加载索引 + 创建 searcher
    std::unique_ptr<BlinkGraphRaBitQSearcherInterface> MakeSearcher(const std::string& kind)
    {
        std::vector<BundleStorageInfo> bundleDescription;
        auto reader = std::make_shared<BundleFileReader>(indexPath_, ReaderType::MEM_ONLY, bundleDescription);
        std::unique_ptr<BlinkGraphRaBitQSearcherInterface> s;
        if (kind == "ZSQ") {
            s = std::make_unique<BlinkGraphZSQSearcherAdaptive>();
        } else if (kind == "RaBitQ") {
            s = std::make_unique<BlinkGraphRaBitQSearcherAdaptive>();  // Batch
        } else if (kind == "Float") {
            s = std::make_unique<FloatSearcher>();  // NoBatch, 真实距离
        } else {
            return nullptr;
        }
        if (!s->Init(config_, reader)) {
            LOG(ERROR) << "searcher init failed: " << kind;
            return nullptr;
        }
        return s;
    }
};

// 主测试: smoke (1k) - 验证流程跑通
TEST(FalconZSQCompare, SmokeSIFT1k)
{
    FalconZSQCompareTest test;
    test.maxElements_ = 1000;
    test.queryCount_ = 1000;
    test.k_ = 10;

    std::vector<std::shared_ptr<float[]>> base;
    std::vector<std::vector<float>> queries;
    std::vector<std::vector<int>> groundTruth;
    ASSERT_TRUE(test.LoadSmokeData(base, queries, groundTruth));

    PrintHeader();
    LOG(INFO) << "Data: base=" << base.size() << " query=" << queries.size() << " gt=" << groundTruth.size()
              << " dim=" << test.dim_ << " topK=" << test.k_;

    std::vector<SearchStats> allStats;

    // 1. ZSQ
    {
        ASSERT_TRUE(test.BuildZSQ(base));
        auto s = test.MakeSearcher("ZSQ");
        ASSERT_NE(s, nullptr);
        allStats.push_back(RunSearch(s.get(), queries, groundTruth, test.k_, "ZSQ"));
        PrintStats(allStats.back());
    }
    // 2. RaBitQ (Batch, step=2)
    {
        ASSERT_TRUE(test.BuildRaBitQBatch(base));
        auto s = test.MakeSearcher("RaBitQ");
        ASSERT_NE(s, nullptr);
        allStats.push_back(RunSearch(s.get(), queries, groundTruth, test.k_, "RaBitQ"));
        PrintStats(allStats.back());
    }
    // 3. Float (NoBatch, 真实距离)
    {
        ASSERT_TRUE(test.BuildRaBitQNoBatch(base));
        auto s = test.MakeSearcher("Float");
        ASSERT_NE(s, nullptr);
        allStats.push_back(RunSearch(s.get(), queries, groundTruth, test.k_, "Float"));
        PrintStats(allStats.back());
    }

    PrintTable(allStats);
    // ZSQ 召回下限校验 (与现有 unittest TEST(BlinkGraphRaBitQ, ZSQ) 一致: ratio > 0.8)
    auto zsqStat = std::find_if(allStats.begin(), allStats.end(),
                                [](const SearchStats& s) { return s.name == "ZSQ"; });
    ASSERT_NE(zsqStat, allStats.end());
    EXPECT_GT(zsqStat->recallRatio, 0.8);
}

// 完整 SIFT-1M 测试 (需 --test_env=FALCON_SIFT_DIR=... + --test_env=FALCON_RUN_FULL=1)
TEST(FalconZSQCompare, FullSIFT1M)
{
    const char* runFull = std::getenv("FALCON_RUN_FULL");
    if (runFull == nullptr || std::string(runFull) != "1") {
        GTEST_SKIP() << "skip FullSIFT1M (need --test_env=FALCON_RUN_FULL=1)";
    }

    const char* dir = std::getenv("FALCON_SIFT_DIR");
    ASSERT_NE(dir, nullptr) << "FALCON_SIFT_DIR env not set";
    std::string dataDir(dir);

    FalconZSQCompareTest test;
    test.maxElements_ = 1000000;
    test.queryCount_ = 10000;
    test.k_ = 10;

    std::vector<std::shared_ptr<float[]>> base;
    std::vector<std::vector<float>> queries;
    std::vector<std::vector<int>> groundTruth;
    ASSERT_TRUE(test.LoadFullData(dataDir, base, queries, groundTruth));

    PrintHeader();
    LOG(INFO) << "Data: base=" << base.size() << " query=" << queries.size() << " gt=" << groundTruth.size()
              << " dim=" << test.dim_ << " topK=" << test.k_;

    std::vector<SearchStats> allStats;

    {
        ASSERT_TRUE(test.BuildZSQ(base));
        auto s = test.MakeSearcher("ZSQ");
        ASSERT_NE(s, nullptr);
        allStats.push_back(RunSearch(s.get(), queries, groundTruth, test.k_, "ZSQ"));
        PrintStats(allStats.back());
    }
    {
        ASSERT_TRUE(test.BuildRaBitQBatch(base));
        auto s = test.MakeSearcher("RaBitQ");
        ASSERT_NE(s, nullptr);
        allStats.push_back(RunSearch(s.get(), queries, groundTruth, test.k_, "RaBitQ"));
        PrintStats(allStats.back());
    }
    {
        ASSERT_TRUE(test.BuildRaBitQNoBatch(base));
        auto s = test.MakeSearcher("Float");
        ASSERT_NE(s, nullptr);
        allStats.push_back(RunSearch(s.get(), queries, groundTruth, test.k_, "Float"));
        PrintStats(allStats.back());
    }
    PrintTable(allStats);
    EXPECT_GT(allStats[0].recallRatio, 0.8);  // ZSQ
}

} // namespace Falcon::Common::ShardFormat::FusionIndex
