# Copyright (c) 2026. All rights reserved.
#
# Linux aarch64 适配 falcon ZSQ/RaBitQ/Float 三种 searcher 对比测试
# 复用 falcon 真实 C++ 实现 (含 SVE 路径), 不重新实现算法
# 兼容 bazel 6.5.0 (BUILD 语法只用 cc_test/deps/data/copts 等稳定语法)
#
# 编译和测试分两台机器:
#   A 机器 (编译机): 有 falcon 源码 + Opencode_done, 用 build.sh 编译 + 打包
#   B 机器 (测试机): 拷贝 tar 包, 用 run.sh 解压运行
#
# 目录结构 (全相对路径, 跨机器拷贝不用改):
#   某根目录/
#   ├── falcon/                          # falcon 源码 (A 机器需要)
#   └── Opencode_done/
#       └── linux_falcon_zsq/            # 本目录
#           ├── build.sh                 # A 机器: 编译 + 打包 -> dist/*.tar.gz
#           ├── run.sh                   # B 机器: 运行二进制
#           ├── BUILD                   # 本文件
#           └── linux_falcon_zsq_test.cpp
#
# A 机器流程:
#   cd Opencode_done/linux_falcon_zsq
#   ./build.sh                # 编译 + 打包 -> dist/linux_falcon_zsq_test.tar.gz
#   ./build.sh --run-smoke    # 可选: 在 A 机器直接验证 smoke
#
# B 机器流程:
#   tar xzf linux_falcon_zsq_test.tar.gz
#   cd linux_falcon_zsq_test
#   ./run.sh smoke            # 1k base, 数据在 runfiles
#   ./run.sh full /path/to/dataset   # SIFT-1M, 需指定数据目录
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
