# Linux aarch64 falcon ZSQ/RaBitQ/Float 对比测试

本目录是 falcon 真实 C++ 搜索引擎在 **Linux aarch64** 上的对比测试,
直接复用 falcon 的 builder/searcher 实现(含 SVE 路径),**不重新实现算法**。

**编译和测试分两台机器**:
- **A 机器(编译机)**: 有 falcon 源码,用 bazel 编译出二进制 + 打包
- **B 机器(测试机)**: 拷贝打包产物,直接跑二进制

## 测的是什么

对比三种基于 blink graph 的近邻搜索器在 SIFT 数据集上的 **QPS / Recall@K / 延迟分位数 (P50/P70/P80/P90/P95/P99)**:

| 搜索器 | falcon C++ 类 | 量化 | 粗筛距离 | 重排 |
|---|---|---|---|---|
| **ZSQ** | `BlinkGraphZSQBuilder` + `BlinkGraphZSQSearcherAdaptive` | Z 旋转 + 8-bit SQ | uint8 L2 | decode→float |
| **RaBitQ** | `BlinkGraphRaBitQBuilder` + `BlinkGraphRaBitQSearcherAdaptive` (step=2, Batch) | Z 旋转 + 1-bit 符号位 + LUT | RaBitQ 估计 | float 真实距离 |
| **Float** | `BlinkGraphRaBitQBuilder` + `FloatSearcher` (本文件新定义, 继承 `BlinkGraphRaBitQSearcher`, NoBatch) | 不量化 | float32 L2 | 无需 |

## 环境要求

### A 机器(编译机)

- **OS**: Linux aarch64 (ARM64, 支持 armv8.2-a + SVE)
- **falcon 源码**: 完整 WORKSPACE (含 `WORKSPACE` / `common/` / `falcon/` / `resource/` / `devel/builder/`)
- **Bazel**: 6.5.0 (falcon 自带 `devel/builder/bazel-6.5.0-linux-arm64`,不用系统 bazel)
- **C++ 工具链**: gcc / clang (支持 C++17 + SVE)
- **不依赖额外包**: falcon WORKSPACE 已声明 grpc/protobuf/googletest 等,bazel 自动拉取

### B 机器(测试机)

- **OS**: Linux aarch64 (与 A 机器**相同架构**,glibc 版本相近)
- **不需要 falcon 源码**,只需要 A 机器打包的 tar.gz
- **smoke 模式**: 数据(sift_*.dat)在 tar 包里,不需要额外数据
- **full 模式**: 需要 SIFT-1M 数据集(`sift_base.fvecs` 等),可从 falcon/dataset/ 拷贝

### A/B 机器路径差异说明

- **A 和 B 的绝对路径不同**(如 A 在 `/home/alice/work/`,B 在 `/home/bob/test/`),
  但**所有脚本和代码都用相对路径**,跨机器拷贝不用改任何路径配置。
- `build.sh` 用 `../../falcon` 相对定位 falcon 根目录(A 机器)
- `run.sh` 用相对当前目录定位二进制和 runfiles(B 机器解压后)
- 测试代码内部加载文件用 `FALCON_SIFT_DIR` 环境变量(由 `run.sh` 传入),
  不写死绝对路径

### 数据加载方式(纯本地,不联网)

- **测试是本地加载 SIFT 数据集,不联网拉起任何服务**(无 etcd / gRPC / OBS 等服务调用)
- `LoadFvecs` / `LoadIvecs` 用 `std::ifstream` 直接读本地 `.fvecs` / `.ivecs` 文件
- smoke 模式的 `falcon/resource/sift_*.dat` 通过 bazel `data` 进 runfiles,B 机器从 runfiles 读
- full 模式的 `falcon/dataset/sift_*.fvecs` 通过 `FALCON_SIFT_DIR` 环境变量指定本地路径
- 索引构建(`BlinkGraphZSQBuilder::Build`)和搜索(`Search`)都是纯内存计算,不涉及任何网络 I/O
- `BundleFileReader` 用 `ReaderType::MEM_ONLY` 构造,纯内存加载索引文件,不联网
- **注意**: bazel 编译时(`build.sh` 阶段)会联网拉取 grpc/protobuf 等依赖源码包,
  这是编译期行为;**运行测试二进制时不联网**

## 目录结构(全相对路径,跨机器拷贝不用改)

```
某根目录/
├── falcon/                          # falcon 源码 (A 机器需要)
└── Opencode_done/
    └── linux_falcon_zsq/            # 本测试代码
        ├── build.sh                 # A 机器: 编译 + 打包
        ├── run.sh                   # B 机器: 运行二进制
        ├── BUILD                    # bazel cc_test 规则
        ├── linux_falcon_zsq_test.cpp
        └── README.md
```

## 完整流程

### 第 1 步: A 机器编译 + 打包

```bash
# 在 A 机器上 (有 falcon + Opencode_done)
cd Opencode_done/linux_falcon_zsq
chmod +x build.sh
./build.sh
```

`build.sh` 做:
1. 找 falcon 根目录(相对路径 `../../falcon`)
2. 在 falcon 树内创建符号链接 `falcon/linux_falcon_zsq`(不修改 falcon 源码)
3. 用 falcon 自带 bazel(`devel/builder/bazel-6.5.0-linux-arm64`)编译
4. 打包二进制 + runfiles(含 smoke 数据)为 `dist/linux_falcon_zsq_test.tar.gz`
5. 打印动态库依赖(`ldd`,B 机器需相同的 .so)

产物: **`dist/linux_falcon_zsq_test.tar.gz`**(约几十 MB)

可选: `./build.sh --run-smoke` 在 A 机器直接跑 smoke 验证编译成功。

### 第 2 步: 拷贝 tar 包到 B 机器

```bash
# 用 scp / rsync / U 盘等
scp Opencode_done/linux_falcon_zsq/dist/linux_falcon_zsq_test.tar.gz user@B-machine:~/
```

如果 B 机器要跑 full 模式,还需拷 SIFT-1M 数据:
```bash
scp -r falcon/dataset user@B-machine:~/
```

### 第 3 步: B 机器运行

```bash
# 解压
cd ~
tar xzf linux_falcon_zsq_test.tar.gz
cd linux_falcon_zsq_test
chmod +x run.sh

# smoke (1k base, 数据在 runfiles 里, 几秒跑完)
./run.sh smoke

# full SIFT-1M (1M base + 10k query, 需指定数据目录)
./run.sh full /path/to/dataset   # 数据目录含 sift_base.fvecs 等

# 列出所有测试 case
./run.sh list
```

## 输出示例

```
================================================================================
ZSQ vs RaBitQ vs Float Searcher Comparison (falcon real C++ impl on aarch64)
================================================================================
Data: base=1000 query=1000 gt=1000 dim=128 topK=10
--------------------------------------------------------------------------------
Searcher    QPS       R@K        mean(us)   P50        P70        P80        P90        P95        P99
--------------------------------------------------------------------------------
[ZSQ] QPS=131.6 R@10=0.976 mean(us)=7600 P50=7500 P70=7700 P80=7800 P90=7900 P95=8100 P99=8500
[RaBitQ] QPS=124.1 R@10=0.378 mean(us)=8100 P50=8000 P70=8200 P80=8300 P90=8400 P95=8600 P99=9000
[Float] QPS=149.7 R@10=0.994 mean(us)=6700 P50=6600 P70=6800 P80=6900 P90=7000 P95=7200 P99=7600
================================================================================
```

bazel 编译的二进制默认通过 stdout 输出 LOG(INFO),`run.sh` 会直接显示。

## 文件结构

```
linux_falcon_zsq/
├── build.sh                     # A 机器: 编译 + 打包 (找 falcon, 建符号链接, 调 bazel)
├── run.sh                       # B 机器: 运行二进制 (smoke / full / list)
├── BUILD                        # bazel cc_test (deps 引用 falcon 真实 builder/searcher)
├── linux_falcon_zsq_test.cpp    # 主测试 (LoadFvecs + FloatSearcher + 对比逻辑)
└── README.md                    # 本文件
```

## 关键设计

- **不重新实现算法**: 直接通过 bazel `deps` 引用 falcon 的 `:blink_graph_zsq_builder` / `:blink_graph_zsq_searcher_adaptive` / `:blink_graph_rabitq_builder` / `:blink_graph_rabitq_searcher` / `:blink_graph_rabitq_searcher_adaptive`,复用真实 C++ 实现(含 SVE 路径)
- **FloatSearcher**: 测试文件内定义的子类,继承 `BlinkGraphRaBitQSearcher`(NoBatch),覆盖 `Search` 直接用 `GetDist` 真实距离搜索,不走 `BatchEstimate`(falcon 原生没有"不量化"的 searcher)
- **不修改 falcon 源码**: 所有代码在 `Opencode_done/linux_falcon_zsq/`,`build.sh` 只在 falcon 树内加符号链接
- **数据加载**: `LoadFvecs`/`LoadIvecs` 适配 ann-benchmarks 的维度前缀格式(falcon 自带 `GetData` 假设无前缀,用于 `resource/sift_*.dat`)
- **跨机器**: tar 包含二进制 + runfiles(含 smoke 数据),B 机器解压即跑

## 常见问题

**Q: A 和 B 机器绝对路径不同,需要改路径配置吗?**
A: 不需要。所有脚本用相对路径:
- `build.sh` 用 `../../falcon` 找 falcon(在 A 机器的 `Opencode_done/linux_falcon_zsq/` 下执行)
- `run.sh` 用相对当前目录找二进制和 runfiles(在 B 机器解压后的 `linux_falcon_zsq_test/` 下执行)
- 数据路径用 `FALCON_SIFT_DIR` 环境变量,由 `run.sh` 传入,不写死在代码里

**Q: 测试会联网拉服务吗(etcd/gRPC 等)?**
A: 不会。测试纯本地:
- `LoadFvecs`/`LoadIvecs` 用 `std::ifstream` 读本地文件
- builder `Build` 和 searcher `Search` 是纯内存计算
- `BundleFileReader` 用 `MEM_ONLY` 模式,纯内存加载
- 只有 `build.sh` 编译阶段会联网拉 bazel 依赖源码包(grpc/protobuf 等),运行二进制时不联网

**Q: B 机器跑二进制报 "error while loading shared libraries"?**
A: 缺动态库。`ldd linux_falcon_zsq_test` 查依赖,在 B 机器装对应的包。两台机器用相同 Linux 发行版最稳妥。

**Q: B 机器跑 full 模式报 "FALCON_SIFT_DIR env not set"?**
A: `./run.sh full` 需指定数据目录,如 `./run.sh full /path/to/dataset`。数据目录需含 `sift_base.fvecs` / `sift_query.fvecs` / `sift_groundtruth.ivecs`(从 falcon/dataset/ 拷)。

**Q: 为什么 smoke 数据在 tar 里,full 数据不在?**
A: smoke 用 `falcon/resource/sift_*.dat`(1k 子集),通过 bazel `data` 引用,自动进 runfiles。full 用 `falcon/dataset/sift_*.fvecs`(1M,512MB),太大不放 tar,用环境变量 `FALCON_SIFT_DIR` 指定路径。

**Q: 为什么 Float baseline 用 BlinkGraphRaBitQBuilder 构图?**
A: falcon 没有"不量化"的 builder。`BlinkGraphRaBitQBuilder` 存原始 float embedding(`embLen_ = sizeof(float) * dim_`),所以 `FloatSearcher` 可以直接用 `GetDist` 算真实 L2,不需要解码。

**Q: ZSQ builder 和 RaBitQ builder 构建的图一样吗?**
A: 不一样。ZSQ builder 用 SQ 编码做候选搜索,RaBitQ builder 用 RaBitQ bitmap 编码。图结构不同,这是 falcon 的设计(builder 和 searcher 配套)。本测试尊重 falcon 的设计。

## aarch64 SVE 适配

- falcon `.bazelrc` 的 `--config=linux_arm64` 启用 `-march=armv8.2-a+crypto+crc+dotprod`
- `extended_rabitq_sve.cpp` 等内核走 SVE 路径(`HasArmSve()` 运行时分发)
- `common/shard_format/utils:arm_sve` / `aarch64_sve_enabled` config_setting 处理 SVE 编译选项
- 无需在本测试代码中做额外的 aarch64 适配,falcon 自己处理

## bazel 6.5.0 兼容

- falcon `.bazelversion` 可能写 7.4.1,但用自带 `devel/builder/bazel-6.5.0-linux-arm64` 二进制,版本自动匹配
- `.bazelrc` 的 `--noenable_bzlmod` 在 6.5.0 下被识别(6.x 已引入实验性 Bzlmod)
- BUILD 语法只用 `cc_test`/`deps`/`data`/`copts` 等稳定语法,6.x 与 7.x 兼容
