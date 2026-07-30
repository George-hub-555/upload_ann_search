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
```
查看端口是否空闲
```
ss -H -lntp | grep ':6335' || echo "port 6335 is free"
```
更改端口为目标要使用的端口
```
LEAF_PORT=6335 bash leaf_perf_runtime/leaf_perf_arm64_bazel65/run_leaf_perf_package_arm64.sh all \
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



`perf.log` 没有最终结果，是因为没有任何请求成功解析：

```text
req num: 0 failed num: 10908
```

依据当前 `perf.cpp`：当请求列表为空时，`Run()` 会直接返回，所以不会输出 QPS、P99 等结果。

先在 B 机器检查生成的请求文件是否是合法 JSON：

```bash
cd /opt/huawei/data3/g50064150/falcon

F=leaf_perf_runtime/leaf_perf_arm64_bazel65/runtime_6635/requests_relevance_softLtrAfmConditionv1.jsonl

head -n 1 "$F" | python3 -m json.tool >/dev/null \
  && echo JSON_OK \
  || echo JSON_BAD
```

再检查文件和日志大小：

```bash
wc -lc "$F" leaf_perf_runtime/leaf_perf_arm64_bazel65/runtime_6635/perf.log
```

判断：

- `JSON_BAD`：生成后的请求文件本身不是合法JSON。
- `JSON_OK`：JSON语法正常，但与 A 机器仓库中 `SearchRequest` 的 protobuf 字段定义不兼容。
- `perf.log` 为0字节：远程版本的日志系统可能绕过了 `tee`，但当前没有性能结果的根因仍然是请求解析数为0。

当前 Leaf 应该仍在运行，不需要再次加载索引。找到请求问题后，可以只运行：

```bash
bash "$RUN" perf dataset/1kw_64dim_test_data/falcon_request_new.txt
```

把 `JSON_OK/JSON_BAD` 和 `wc -lc` 输出发给我，再继续定位。



###查看字段
```
sed -n '120,155p' tools/test/perf/perf.cpp
python3 -c 'import json,sys;x=json.loads(sys.stdin.readline());print({k:type(v).__name__ for k,v in x.items()})' < "$F"
```



不建议直接覆盖整个 `perf.cpp`。

已经确认两个版本不完全相同，例如：

- A 机器版本使用 `NowMillisecond()`
- 当前本地版本使用 `NowMicrosecond()`
- 当前本地版本还增加了其他统计逻辑

整体复制可能与 A 仓库的 proto、依赖或其他源码不兼容。

最安全的是只修改 A 原有 `perf.cpp` 的这一行：

```cpp
auto st = ::google::protobuf::json::JsonStringToMessage(line, &req);
```

替换为：

```cpp
google::protobuf::json::ParseOptions parseOptions;
parseOptions.ignore_unknown_fields = true;
auto st = ::google::protobuf::json::JsonStringToMessage(line, &req, parseOptions);
```

即：

- 不复制整个本地 `perf.cpp`
- 不修改 Leaf
- 只在 A 原有文件中增加这两行并修改调用
- 之后仅重新编译 `//tools/test/perf:perf`

这是对两个略有差异仓库风险最小的处理。



### 已确认

真正错误是：

```text
Address already in use
Error in bind for address '127.0.0.1:6635'
```

上一次启动成功的旧 Leaf 仍然占用6635端口。删除运行目录只会删除文件，不会停止已经运行的 Linux 进程。

`stop` 显示未运行，是因为它依赖：

```text
runtime_6635/leaf.pid
```

这个 PID 文件已经随目录一起被删除，所以脚本找不到旧进程。

`Etcd endpoint not set` 是本地模式下的预期日志，不是这次失败原因。

### 处理方法

上一次输出显示旧 Leaf 的 PID 是 `2323466`。先确认它：

```bash
ps -p 2323466 -o pid,user,etime,cmd
readlink -f /proc/2323466/exe
```

确认是旧的 Leaf 后，正常停止：

```bash
kill 2323466
```

确认6635端口已经释放：

```bash
ss -lntp | grep ':6635' || echo "port 6635 is free"
```

如果没有任何监听，重新运行：

```bash
bash leaf_perf_runtime/leaf_perf_arm64_bazel65/run_leaf_perf_package_arm64.sh all \
  dataset/1kw_64dim_test_data/index_output \
  dataset/1kw_64dim_test_data/falcon_request_new.txt
```

不要终止日志中的 `2382634`；这是本次绑定端口失败后已经退出的新 Leaf。
