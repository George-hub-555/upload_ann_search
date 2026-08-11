# ARM64 SIFT1M ANN 统一测试

本目录用于在 ARM64 Linux 机器 A 编译、在目录结构相同的 ARM64 Linux
机器 B 执行。配置、命令和结果引用均使用相对于 `test_all` 的路径。

## 当前状态

默认启用：

- Faiss HNSWFlat：扫描 `efSearch`。
- Faiss IVFPQ：固定 `nlist=1024, M=16, nbits=8`，扫描 `nprobe`。
- hnswlib HNSW：扫描 `efSearch`。
- NGT 2.7.4 GraphAndTree：扫描 `epsilon`。

明确跳过但会在统一 CSV 中记录原因：

- KGN：现有文件只有 `linux_x86_64` wheel，没有 ARM 源码。
- QSG-NGT：现有文件只有 x86_64 wheel、程序和动态库，没有 ARM 源码。
- 当前 ParlayANN：原构建参数和距离实现包含 x86 专用内容，默认不参与 ARM 构建。

当前离线源码按仓库内以下相对路径接入：

```text
hnswlib/hnswlib-master/
NGT/NGT-main/
```

NGT 只构建普通 NGT，关闭 QG/QBG；其源码已经包含 ARM64 NEON 距离实现。

## 数据

必须存在：

```text
dataset/sift_base.fvecs
dataset/sift_query.fvecs
dataset/sift_learn.fvecs
dataset/sift_groundtruth.ivecs
```

测试使用完整 SIFT1M，距离为 squared L2，同时生成 Recall@10 和 Recall@100。
Recall 定义为返回 top-k 与 ground-truth top-k 的交集数除以 `nq * k`。

## A 机：离线编译

系统需预装 CMake 3.22.0+、支持 C++17 的 G++、OpenMP、BLAS 和 LAPACK。
当前 A、B 机器的 CMake 3.22.0 满足要求。构建过程不会联网，也不会调用
`git clone` 或 `pip`。

```bash
cd test_all
chmod +x build_on_a.sh run_on_b.sh
./build_on_a.sh
```

默认使用 OpenBLAS。若系统 BLAS 名称不同：

```bash
BLAS_VENDOR=Generic ./build_on_a.sh
```

产物：

```text
bin/ann_faiss_benchmark
bin/ann_hnswlib_benchmark
bin/ann_ngt_benchmark
bin/build_info.txt
```

一次执行 `build_on_a.sh` 会编译并安装以上三个 benchmark。Faiss 固定
`FAISS_OPT_LEVEL=generic`；NGT 固定 `NGT_MARCH_NATIVE_DISABLED=ON`；hnswlib
不启用其上游 examples 中的 `-march=native`。三个适配器都不使用
`-mcpu=native`，避免 A、B 两台 ARM 机器微架构不同导致非法指令。

## B 机：一次执行

保持整个仓库的相对目录结构与 A 机一致，然后：

```bash
cd test_all
./run_on_b.sh
```

一次执行 `run_on_b.sh` 会依次测试 Faiss-HNSW、Faiss-IVFPQ、hnswlib-HNSW
和 NGT，并合并成一个 CSV；单个算法失败不会阻止后续算法。脚本会拒绝在
非 ARM64 主机运行。当前默认测试线程数为 1 和 16，每个搜索参数执行
1000 条预热查询和 5 次正式搜索，QPS 取中位数。参数可在
`config.json` 中修改。

B 机仍需具备与 A 机构建结果兼容的系统运行库，通常包括 `libgomp`，以及
Faiss 实际链接到的 BLAS/LAPACK 动态库。脚本不会联网安装这些库。若 A、B
使用相同 Linux 发行版和工具链环境，通常可以直接运行；否则根据
`results/logs/*.log` 中的 `not found` 信息补齐运行库。

主要输出：

```text
results/all_results.csv
results/run_metadata.json
results/logs/faiss.log
results/logs/hnswlib.log
results/logs/ngt.log
results/raw/faiss.csv
results/raw/hnswlib.csv
results/raw/ngt.csv
```

如果 B 机装有 matplotlib，还会生成：

```text
results/plots/qps_recall_at_10_threads_1.png
results/plots/qps_recall_at_10_threads_16.png
results/plots/qps_recall_at_100_threads_1.png
results/plots/qps_recall_at_100_threads_16.png
```

没有 matplotlib 不影响 CSV。将 `results/all_results.csv` 发回即可继续分析和
绘图。

## 计时边界

QPS 只计批量查询：不包含数据读取、索引训练/构建、预热和 Recall 计算。
Faiss HNSW 与 IVFPQ 在同一进程内依次构建和测试，避免同时保留两个大型
索引；hnswlib 和 NGT 各自在独立进程中只构建一次索引，然后完成全部
Recall@10、Recall@100、线程数和搜索参数组合。

## CSV 字段

每个曲线点写入后都会立即刷新到 raw CSV。若长时间测试中途失败，汇总器会
保留本次已经完成的 `ok` 曲线点，并额外增加 `failed` 状态行；不会误用上次
运行遗留的 raw CSV。

成功曲线点的 `status` 为 `ok`；不兼容算法为 `skipped`，执行失败为
`failed`。绘图脚本只读取 `ok` 行。关键字段包括：

- `algorithm`
- `top_k`
- `threads`
- `search_parameter` / `search_value`
- `recall_at_k`
- `qps`
- `index_parameters`
- `status` / `note`
