A 机器负责编译，需要传入两个脚本：

```text
tools/test/perf/build_leaf_perf_package_arm64_bazel65.sh
tools/test/perf/run_leaf_perf_package_arm64.sh
```

运行脚本也要放到 A，因为构建脚本会把它一起打进运行包。

### A 机器：编译并打包

```bash
cd /opt/huawei/data3/g50064150/falcon
chmod +x devel/builder/bazel-6.5.0-linux-arm64
chmod +x tools/test/perf/*.sh
bash tools/test/perf/build_leaf_perf_package_arm64_bazel65.sh
```

生成：

```text
/opt/huawei/data3/g50064150/falcon/dist/leaf_perf_arm64_bazel65.tar.gz
```

Bazel 缓存位于：

```text
/opt/huawei/data3/g50064150/falcon/bazel-cache
```

### B 机器：只传运行包

只需要把 A 机器生成的这个文件传到 B：

```text
leaf_perf_arm64_bazel65.tar.gz
```

B 机器不需要 Bazel，也不需要单独传构建脚本。

在 B 机器解压：

```bash
cd /opt/huawei/data3/g50064150/falcon
mkdir -p leaf_perf_runtime
tar -xzf dist/leaf_perf_arm64_bazel65.tar.gz -C leaf_perf_runtime
```

### B 机器：最终运行

使用你的 JSON 请求文件：

```bash
cd /opt/huawei/data3/g50064150/falcon

bash leaf_perf_runtime/leaf_perf_arm64_bazel65/run_leaf_perf_package_arm64.sh all \
  dataset/1kw_64dim_test_data/index_output \
  dataset/1kw_64dim_test_data/falcon_request_new.txt
```

该命令会依次：

1. 启动本机 Leaf。
2. 加载 `index_output`。
3. 通过 `127.0.0.1:6635` 执行本地压测。
4. 输出 `perf.cpp` 提供的 QPS、成功/失败数量、P99/P95 等延迟指标。

默认参数为 20 线程、运行 60 秒。测试完成后停止 Leaf：

```bash
bash leaf_perf_runtime/leaf_perf_arm64_bazel65/run_leaf_perf_package_arm64.sh stop
```

注意：当前 `perf.cpp` 流程不计算 Recall，只输出性能及请求成功率指标。


###找日志格式
```
LOG=leaf_perf_runtime/leaf_perf_arm64_bazel65/runtime_6635/leaf.log
tail -n 50 "$LOG"
```


###修正
### 已确认

`tail -n 50` 不会发送 `SIGTERM`，它只读取日志。

这个 `SIGTERM` 是运行脚本主动发送的。证据是：

- 14:49:31：Leaf 已加载索引并进入服务状态。
- 14:59:29：刚好约600秒后收到 `SIGTERM`。
- 当前脚本超时后会执行 `kill "${leaf_pid}"`。

因此不是 Leaf 崩溃，而是脚本误判启动失败后关闭了 Leaf。

无需重新编译。在 B 机器临时修正脚本：

```bash
cd /opt/huawei/data3/g50064150/falcon
RUN=leaf_perf_runtime/leaf_perf_arm64_bazel65/run_leaf_perf_package_arm64.sh

sed -i 's#add new shard:test_corpus/#add new shard:test_corpus#' "$RUN"
sed -i 's#grpc server begin loop#Leaf service waiting#' "$RUN"
bash -n "$RUN"
```

然后重新执行：

```bash
bash "$RUN" all \
  dataset/1kw_64dim_test_data/index_output \
  dataset/1kw_64dim_test_data/falcon_request_new.txt
```

这次识别到 `add new shard:test_corpus` 和 `Leaf service waiting` 后，就会立即进入 `[2/2] Running perf.cpp`。
