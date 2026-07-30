**###Result**

```
[1/2] Starting local leaf and loading /opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/index_output
Leaf is ready: pid=2323466, address=127.0.0.1:6635
Local shard key: corpus=test_corpus, cycle=<empty>, shard=0, replica=0
Leaf log: /opt/huawei/data3/g50064150/falcon/leaf_perf_runtime/leaf_perf_arm64_bazel65/runtime_6635/leaf.log
Preparing a local request copy:
  relevance_learning2rank -> relevance_softLtrAfmConditionv1
WARNING: this changes only the section identifier; it does not prove that both names use the same embedding model.
[2/2] Running perf.cpp: threads=20, seconds=60
Perf input: /opt/huawei/data3/g50064150/falcon/leaf_perf_runtime/leaf_perf_arm64_bazel65/runtime_6635/requests_relevance_softLtrAfmConditionv1.jsonl
Perf log: /opt/huawei/data3/g50064150/falcon/leaf_perf_runtime/leaf_perf_arm64_bazel65/runtime_6635/perf.log
2026-07-30 15:14:37,621|INFO|{"address": "127.0.0.1", "details": "[tools/test/perf/perf.cpp:147] req num: 0 failed num: 10908"}
```
