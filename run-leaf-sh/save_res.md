可以，只改 B 机器上的运行脚本，不需要重新编译 Leaf 或 perf。

执行：

```bash
cd /opt/huawei/data3/g50064150/falcon

RUN=leaf_perf_runtime/leaf_perf_arm64_bazel65/run_leaf_perf_package_arm64.sh

sed -i 's#^readonly PERF_LOG=.*#readonly PERF_LOG="${RUNTIME_DIR}/per_thread${PERF_THREADS}.log"#' "$RUN"

bash -n "$RUN"
grep -n 'readonly PERF_LOG' "$RUN"
```

检查结果应为：

```bash
readonly PERF_LOG="${RUNTIME_DIR}/per_thread${PERF_THREADS}.log"
```

之后运行：

```bash
LEAF_PORT=6335 PERF_THREADS=20 PERF_SECONDS=60 \
bash "$RUN" perf \
dataset/1kw_64dim_test_data/falcon_request_new.txt
```

日志将保存为：

```text
leaf_perf_runtime/leaf_perf_arm64_bazel65/runtime_6335/per_thread20.log
```

不同线程数会自动生成：

```text
per_thread1.log
per_thread2.log
per_thread4.log
per_thread20.log
```

注意：相同线程数运行多轮时仍会覆盖同一个文件。如果要保存多轮，建议进一步改成：

```bash
readonly PERF_ROUND="${PERF_ROUND:-1}"
readonly PERF_LOG="${RUNTIME_DIR}/per_thread${PERF_THREADS}_round${PERF_ROUND}.log"
```

运行时指定：

```bash
PERF_ROUND=1 PERF_THREADS=20 ...
PERF_ROUND=2 PERF_THREADS=20 ...
```

对应生成 `per_thread20_round1.log`、`per_thread20_round2.log`。



已新增独立脚本：[run_leaf_perf_sweep_arm64.sh](E:/Graduation/GPTproject/FS/falcon/tools/test/perf/run_leaf_perf_sweep_arm64.sh)。

本次没有修改任何现有脚本或 C++ 文件。已通过 `bash -n`，并确认使用 Linux LF 换行。

将它复制到 B：

```text
leaf_perf_runtime/leaf_perf_arm64_bazel65/run_leaf_perf_sweep_arm64.sh
```

Leaf 提前启动后运行：

```bash
cd /opt/huawei/data3/g50064150/falcon

LEAF_PORT=6335 PERF_SECONDS=60 PERF_STUB_NUM=10 \
bash leaf_perf_runtime/leaf_perf_arm64_bazel65/run_leaf_perf_sweep_arm64.sh \
dataset/1kw_64dim_test_data/falcon_request_new.txt
```

脚本会：

- 自动执行5轮、共45次压测。
- 正序和倒序交替。
- 每组间隔20秒。
- 不启动、停止或终止 Leaf。
- 生成 `001_round1_thread1.log` 至 `045_round5_thread20.log`。
- 日志保存在 `runtime_6335/sweep_results/`。
- 同名日志已存在时拒绝覆盖。
