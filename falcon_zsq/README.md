# Linux aarch64 falcon ZSQ/RaBitQ/Float 对比测试

本目录是 falcon 真实 C++ 搜索引擎在 **Linux aarch64 + Bazel** 环境下的对比测试,
直接复用 falcon 的 builder/searcher 实现(含 SVE 路径),**不重新实现算法**。

## 测的是什么

对比三种基于 blink graph 的近邻搜索器在 SIFT 数据集上的 **QPS / Recall@K / 延迟分位数 (P50/P70/P80/P90/P95/P99)**:

| 搜索器 | falcon C++ 类 | 量化 | 粗筛距离 | 重排 |
|---|---|---|---|---|
| **ZSQ** | `BlinkGraphZSQBuilder` + `BlinkGraphZSQSearcherAdaptive` | Z 旋转 + 8-bit SQ (按维度 min/max) | uint8 L2 (sqFstdistfunc_) | decode→float 真实距离 (GetDecodeDist) |
| **RaBitQ** | `BlinkGraphRaBitQBuilder` + `BlinkGraphRaBitQSearcherAdaptive` (step=2, Batch) | Z 旋转 + 1-bit 符号位 + LUT | RaBitQ 估计距离 (BatchEstimate, factorAdd + factorRescale*(delta*accu+sumLut+k1sumq)) | float 真实距离 (resPool) |
| **Float** | `BlinkGraphRaBitQBuilder` + `FloatSearcher` (本文件新定义, 继承 `BlinkGraphRaBitQSearcher`, NoBatch) | 不量化 (存原始 float) | float32 L2 (GetDist) | 无需重排 |

`FloatSearcher` 是本测试文件内定义的 baseline 类(继承 falcon 的 `BlinkGraphRaBitQSearcher`,
覆盖 `Search` 方法直接用 `GetDist` 真实距离搜索,不走 `BatchEstimate`),
**不修改 falcon 源码**,只是定义一个子类。

## 环境要求 (Linux aarch64 机器)

- **OS**: Linux aarch64 (ARM64, 支持 armv8.2-a + SVE)
- **Bazel**: 6.5.0 (falcon 自带二进制 `devel/builder/bazel-6.5.0-linux-arm64`,不用系统 bazel)
- **falcon 源码**: 完整的 falcon WORKSPACE (含 `WORKSPACE` / `third_party.bzl` / `common/` / `falcon/` / `resource/` / `devel/builder/`)
- **C++ 工具链**: gcc / clang (支持 C++17 + SVE)
- **不依赖额外包**: falcon WORKSPACE 已声明 grpc/protobuf/googletest 等依赖,bazel 自动拉取

### 目录结构 (全相对路径, 跨机器拷贝不用改)

```
某根目录/
├── falcon/                          # falcon 源码 (含 WORKSPACE)
└── Opencode_done/
    └── linux_falcon_zsq/            # 本测试代码
        ├── run.sh                   # 一键运行脚本
        ├── BUILD                    # bazel cc_test
        ├── linux_falcon_zsq_test.cpp
        └── README.md
```

`run.sh` 自动在 falcon 树内创建符号链接 `falcon/linux_falcon_zsq -> ../Opencode_done/linux_falcon_zsq`,
让 bazel 能在 WORKSPACE 根看到本测试代码。**不修改 falcon 源码**,只加一个符号链接。

### bazel 调用方式 (falcon 自带二进制)

falcon 仓库 `devel/builder/` 下自带 bazel 二进制,**不使用系统 bazel**。
对应 falcon `scripts/build_iac.sh` 的用法 (`export BAZEL_BINARY=devel/builder/bazel-X.Y.Z-linux-arm64`)。

由于用自带 bazel 6.5.0,`.bazelversion` 检查自动通过(二进制版本即 6.5.0)。

**Bzlmod 兼容**: falcon `.bazelrc` 里有 `common --noenable_bzlmod`。bazel 6.5.0 默认不开 Bzlmod,
这个 flag 在 6.5.0 下会被识别(6.x 已引入实验性 Bzlmod),不影响运行。

**BUILD 语法兼容**: 本测试的 `BUILD` 文件只用 `cc_test` / `deps` / `data` / `copts` / `linkopts` /
`visibility` 等稳定语法,bazel 6.x 与 7.x 完全兼容,无需修改。

## 安装与运行

### 1. 拷贝整个根目录到 Linux aarch64 机器

```bash
# 假设根目录拷到 ~/work
# 结构: ~/work/falcon/ + ~/work/Opencode_done/linux_falcon_zsq/
cd ~/work/Opencode_done/linux_falcon_zsq
```

### 2. Smoke 测试 (1k base, 几秒跑完, 验证流程)

```bash
cd ~/work/Opencode_done/linux_falcon_zsq
chmod +x run.sh
./run.sh smoke
```

走 `falcon/resource/sift_*.dat` (1k 子集, 通过 bazel data 提供),
对比 ZSQ / RaBitQ / Float 三种 searcher 的 recall/qps/latency。

### 3. 完整 SIFT-1M 测试 (1M base + 10k query, 构图几分钟)

```bash
cd ~/work/Opencode_done/linux_falcon_zsq
./run.sh full
```

- `--spawn_strategy=local`: 绕过 bazel 沙箱,允许读取 `falcon/dataset/` 下的 fvecs 文件(不在 bazel data 中)
- `--test_env=FALCON_SIFT_DIR`: 指向 `$(pwd)/falcon/dataset`(脚本内动态生成绝对路径)
- `--test_env=FALCON_RUN_FULL=1`: 启用 FullSIFT1M 测试(否则 GTEST_SKIP)
- `--test_filter`: 只跑 FullSIFT1M (跳过 smoke)

### 4. 查看 LOG(INFO) 输出

bazel 默认捕获测试的 stdout/stderr,`--test_output=all` 显示全部输出。
对比表格式:

```
================================================================================
ZSQ vs RaBitQ vs Float Searcher Comparison (falcon real C++ impl on aarch64)
================================================================================
--------------------------------------------------------------------------------
Searcher    QPS       R@K        mean(us)   P50        P70        P80        P90        P95        P99
--------------------------------------------------------------------------------
ZSQ         131.6     0.9760     7600.0     7500.0     7700.0     7800.0     7900.0     8100.0     8500.0
RaBitQ      124.1     0.3780     8100.0     8000.0     8200.0     8300.0     8400.0     8600.0     9000.0
Float       149.7     0.9940     6700.0     6600.0     6800.0     6900.0     7000.0     7200.0     7600.0
================================================================================
```

## aarch64 SVE 适配

- falcon `.bazelrc` 的 `--config=linux_arm64` 启用 `-march=armv8.2-a+crypto+crc+dotprod`
- `extended_rabitq_sve.cpp` 等内核走 SVE 路径 (`HasArmSve()` 运行时分发)
- `common/shard_format/utils:arm_sve` / `aarch64_sve_enabled` config_setting 处理 SVE 编译选项
- 无需在本测试代码中做额外的 aarch64 适配,falcon 自己处理

## 文件结构

```
linux_falcon_zsq/
├── run.sh                       # 一键运行 (自动建符号链接 + 调 bazel)
├── BUILD                        # bazel cc_test (deps 引用 falcon 真实 builder/searcher)
├── linux_falcon_zsq_test.cpp    # 主测试 (LoadFvecs + FloatSearcher + 对比逻辑)
└── README.md                    # 本文件
```

## 与 falcon 现有 unittest 的关系

falcon 已有 `blink_graph_rabitq_unittest.cpp` 中的 `TEST(BlinkGraphRaBitQ, ZSQ)`
(只测 ZSQ 召回 > 0.8, 不对比 RaBitQ/Float)。

本测试是增强版:
- 对比 ZSQ / RaBitQ / Float 三种 searcher
- 统计 QPS / Recall / P50/P70/P80/P90/P95/P99 延迟分位数
- 支持 smoke (1k) 和 full (1M) 两种数据规模
- `FloatSearcher` 子类作为不量化 baseline (falcon 原生没有)

## 不修改 falcon 源码

- 所有代码在 `Opencode_done/linux_falcon_zsq/` 下
- `FloatSearcher` 是 `BlinkGraphRaBitQSearcher` 的子类,定义在测试文件内
- BUILD 通过 `deps` 引用 falcon 的 `cc_library`,不修改 falcon 的 BUILD 文件
- 数据加载用 `falcon/dataset/sift_*.fvecs` (ann-benchmarks 格式),自定义 `LoadFvecs` 适配维度前缀
  (falcon 自带的 `GetData` 假设无前缀, 用于 `resource/sift_*.dat`)

## 常见问题

**Q: 为什么不用 falcon/dataset/sift_*.fvecs 作为 bazel data?**
A: `falcon/dataset/` 没有 BUILD 文件,无法作为 bazel target 引用。所以用环境变量
   `FALCON_SIFT_DIR` + `--spawn_strategy=local` 直接读文件系统。

**Q: 为什么 Float baseline 用 BlinkGraphRaBitQBuilder 构图?**
A: falcon 没有"不量化"的 builder。`BlinkGraphRaBitQBuilder` 存原始 float embedding
   (`embLen_ = sizeof(float) * dim_`),所以 `FloatSearcher` 可以直接用 `GetDist` 算
   真实 L2 距离,不需要解码。图构建用 RaBitQ 估计距离做候选搜索,但图质量由真实
   距离排序决定,与 Float search 兼容。

**Q: ZSQ builder 和 RaBitQ builder 构建的图一样吗?**
A: 不一样。ZSQ builder 用 SQ 编码做候选搜索 (`SearchCandidate` 中 `BatchEstimate`
   用 SQ uint8 距离),RaBitQ builder 用 RaBitQ bitmap 编码。图结构不同,这是 falcon
   的设计 (builder 和 searcher 配套)。本测试尊重 falcon 的设计,不强行统一图。
