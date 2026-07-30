目前两个日志只能确认失败数量，不能确认失败原因，因为默认没有打印每条失败请求的 gRPC 状态和 Leaf 返回错误。

由于实际仓库与参考仓库不同，先检查 A 机器实际编译的代码是否支持失败详情。

### 1. 在 A 机器检查

进入 A 自己的 Falcon 根目录，执行：

```bash
grep -n 'failed_detail' tools/test/perf/perf.cpp
```

再执行：

```bash
grep -n -A 12 'if (FLAGS_failed_detail)' tools/test/perf/perf.cpp
```

如果能看到类似：

```cpp
DEFINE_bool(failed_detail, false, ...)
```

以及：

```cpp
if (FLAGS_failed_detail) {
    ...
    LOG(ERROR) << "failed req: " ...
}
```

说明已经支持，不需要重新编译。

如果没有这些代码，把实际输出发给我，需要按照 A 的实际源码做最小修改，不能直接照搬当前仓库。

```
grep -n 'cluster_posting_type\|cluster_ivf_section_type' tools/index_factory/schema.json
grep -n -A 30 'GetScorer' falcon/serving/leaf/indexing/shard.cpp
```

### 2. 在 B 机器运行小规模诊断

先确认6335的 Leaf 仍在运行：

```bash
LEAF_PORT=6335 bash leaf_perf_runtime/leaf_perf_arm64_bazel65/run_leaf_perf_package_arm64.sh status
```

然后在 B 自己的 Falcon 根目录执行：

```bash
PKG=leaf_perf_runtime/leaf_perf_arm64_bazel65
DATA="$PKG/runtime_6335/requests_relevance_softLtrAfmConditionv1.jsonl"
```

只使用1线程运行1秒，避免打印几十万条失败详情：

```bash
"$PKG/bin/perf" \
  --tgt=leaf \
  --dst=127.0.0.1:6335 \
  --data="$DATA" \
  --thread_num=1 \
  --seconds=1 \
  --stub_num=1 \
  --corpus=test_corpus \
  --cycle= \
  --shard=0 \
  --replica=0 \
  --failed_detail=true \
  > "$PKG/runtime_6335/perf_failed_detail.log" 2>&1
```

查看前几条失败响应，去掉很长的请求内容：

```bash
grep -m 5 'failed req:' "$PKG/runtime_6335/perf_failed_detail.log" |
sed 's/.* rsp: /rsp: /'
```

同时检查 Leaf 服务端错误：

```bash
grep -Ei 'ERROR|WARN|failed|invalid' "$PKG/runtime_6335/leaf.log" |
tail -n 100
```

判断方式：

- `st code` 非0：gRPC调用或连接层失败，查看 `st msg`。
- `st code` 为0，但响应中的 `errorCode` 非0：Leaf成功收到请求，但搜索处理失败，重点查看 `errorMessage`。
- Leaf 日志出现 section、query、plugin、corpus 等错误：按对应服务端错误继续定位。

这些命令中，A 只检查源码；请求文件、索引路径和6335端口全部只属于 B，不会混用两台机器的绝对路径。



### 当前结论

从现有证据看，问题发生在 **Leaf 拉起后的搜索评分阶段**，暂时不应归因于索引构建。

依据：

- 索引已经成功加载：

```text
add new shard:test_corpus
Leaf service waiting
```

- 150935次搜索成功并返回结果，说明索引可以被读取和检索。
- 报错位置是：

```text
search_processor.cpp
cannot find scorer[vector_scorer_plugin]
```

这是请求进入 Leaf 后，在 `Score()` 评分阶段查找 scorer 失败，不是构建索引时产生的错误。

### 仍不能完全排除的情况

参考仓库中，`GetScorer()` 返回空有两种可能：

1. Leaf 二进制根本没有注册 `vector_scorer_plugin`。
2. scorer 已注册，但初始化时找不到索引中需要的 section/attachment。

第一种属于 Leaf 编译或请求配置问题；第二种才可能与索引构建内容有关。

### 最小定位命令

在 A 的实际仓库根目录执行：

```bash
grep -R -n 'vector_scorer_plugin' falcon/serving/leaf | head -n 30
```

判断：

- 完全没有输出：实际 Leaf 代码不包含这个插件。问题属于请求与 Leaf 能力不匹配，与索引构建无关。
- 找到 `REGISTER_SCORER` 或插件实现：继续检查它是否被 Leaf 编译链接，以及初始化需要哪些索引字段。
- 只在测试或请求样例中出现：同样说明生产 Leaf 没有注册它。

再查看实际仓库的 `GetScorer`：

```bash
grep -n -A 30 'GetScorer' falcon/serving/leaf/indexing/shard.cpp
```

在确认 scorer 是否存在之前，不建议重新构建索引。当前最高概率是 Leaf 中缺少 `vector_scorer_plugin`，或者请求不应该指定这个插件。
```
strings leaf_perf_runtime/leaf_perf_arm64_bazel65/bin/leaf |
grep -F 'vector_scorer_plugin'
```
