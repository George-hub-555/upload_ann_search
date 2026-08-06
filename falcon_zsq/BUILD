# Copyright (c) 2026. All rights reserved.
#
# Linux aarch64 适配 falcon ZSQ/RaBitQ/Float 三种 searcher 对比测试
# 复用 falcon 真实 C++ 实现 (含 SVE 路径), 不重新实现算法
# 兼容 bazel 6.5.0 (BUILD 语法只用 cc_test/deps/data/copts 等稳定语法)
#
# 目录结构 (全相对路径, 跨机器拷贝不用改):
#   某根目录/
#   ├── falcon/                          # falcon 源码 (含 WORKSPACE)
#   └── Opencode_done/
#       └── linux_falcon_zsq/            # 本目录
#           ├── run.sh                   # 一键运行 (自动建符号链接 + 调 bazel)
#           ├── BUILD                   # 本文件
#           └── linux_falcon_zsq_test.cpp
#
# 使用方式 (在 Opencode_done/linux_falcon_zsq/ 下执行):
#   ./run.sh smoke       # 1k base, 几秒, 走 falcon/resource 的 bazel data
#   ./run.sh full        # SIFT-1M, 构图几分钟, 读 falcon/dataset/
#
# run.sh 自动:
#   1. 在 falcon 树内创建符号链接 falcon/linux_falcon_zsq -> 本目录 (不修改 falcon 源码)
#   2. 用 falcon 自带 bazel: devel/builder/bazel-6.5.0-linux-arm64
#   3. 在 falcon 根目录执行 bazel test (WORKSPACE 必须在根)
#
#   # 完整 SIFT-1M (dataset 在源码树内, --spawn_strategy=local 绕过沙箱读文件)
#   bazel test --config=linux_arm64 --test_output=all --spawn_strategy=local \
#       --test_env=FALCON_SIFT_DIR=$(pwd)/falcon/dataset \
#       --test_env=FALCON_RUN_FULL=1 \
#       --test_filter=FalconZSQCompare.FullSIFT1M \
#       //linux_falcon_zsq:linux_falcon_zsq_test
#
# 不依赖额外包: falcon WORKSPACE 已声明 grpc/protobuf/googletest 等依赖, bazel 自动拉取.
# aarch64 SVE: falcon .bazelrc 的 --config=linux_arm64 启用 -march=armv8.2-a+crypto+crc+dotprod,
#              extended_rabitq_sve.cpp 等内核走 SVE 路径 (HasArmSve() 运行时分发).

cc_test(
    name = "linux_falcon_zsq_test",
    srcs = ["linux_falcon_zsq_test.cpp"],
    data = [
        "//resource:vector_test_data",  # 1k SIFT 子集 (smoke 模式)
    ],
    copts = [
        "-O3",
        "-fopenmp",
    ],
    linkopts = [
        "-fopenmp",
    ],
    deps = [
        # ZSQ builder + searcher (含 SQ 编码/解码/旋转)
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_zsq_builder",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_zsq_searcher_adaptive",
        # RaBitQ builder + 两种 searcher (NoBatch 真实距离 / Adaptive Batch LUT 估计)
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_rabitq_builder",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_rabitq_searcher",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_rabitq_searcher_adaptive",
        # GetData 模板 (utils.h, 提供 fvecs/dat 加载工具)
        "//common/shard_format/utils:utils",
        # 日志 / 临时目录 / 随机目录 (参考现有 blink_graph_rabitq_unittest 的 deps)
        # proto_util.h / common_macro.h 通过 //common/public/utils:logging 传递
        "//common/public/utils:logging",
        "//common/utils:logging",
        "//common/utils:rand_generator",
        "//common/utils/filesystem",
        "@com_google_googletest//:gtest_main",
    ],
)
