如果 ARM 机器上已经有同版本的完整 Falcon 仓库，只需复制这 3 个文件：

1. [build_local_v1.cpp](E:/Graduation/GPTproject/FS/falcon/tools/index_factory/build_local_v1.cpp)
2. [schema.json](E:/Graduation/GPTproject/FS/falcon/tools/index_factory/schema.json)
3. [BUILD](E:/Graduation/GPTproject/FS/falcon/tools/index_factory/BUILD)

如果 ARM 仓库的 `BUILD` 有自己的修改，不要直接覆盖，只添加下面目标：

```python
cc_binary(
    name = "build_local_v1",
    srcs = ["build_local_v1.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/utils/filesystem:file",
        "//index_factory/index_builder:index_builder_impl",
        "@com_github_gflags_gflags//:gflags",
    ],
)
```

ARM 适配不需要修改 `build_local_v1.cpp`。仓库的 `.bazelrc` 已有 `linux_arm64` 配置。

需要修改的只有 schema 模型路径。如果 ARM 上仍解压到：

```text
/opt/huawei/data2/l00856060/1kw_64dim_test_data/
```

则无需修改。否则修改 [schema.json](E:/Graduation/GPTproject/FS/falcon/tools/index_factory/schema.json) 中：

```json
"faiss_model_path": [
    "/ARM机器上的实际路径/index_model_relevance_softLtrAfmConditionv1"
]
```

ARM 机器上的操作：

```bash
cd /opt/huawei/data2/l00856060
unzip 1kw_64dim_test_data.zip

cd 1kw_64dim_test_data
tar -xzf falcon_data.tar.gz
mkdir -p index_output
```

回到 Falcon 仓库编译：

```bash
cd /实际路径/falcon

bazel build \
  --config=linux_arm64 \
  //tools/index_factory:build_local_v1
```

运行：

```bash
./bazel-bin/tools/index_factory/build_local_v1 \
  --schema_path=/实际路径/falcon/tools/index_factory/schema.json \
  --data_path=/opt/huawei/data2/l00856060/1kw_64dim_test_data/falcon_data/0 \
  --output_path=/opt/huawei/data2/l00856060/1kw_64dim_test_data/index_output \
  --shard_id=0
```

注意：

- 必须在 ARM 机器上重新编译，不能复制 x86 构建出的 `build_local_v1`。
- 不需要修改 `.bazelrc`；使用 `--config=linux_arm64`。
- 如果 ARM CPU 不支持 `armv8.2-a+dotprod`，可能出现 `Illegal instruction`，届时再调整 ARM 编译参数。
- 当前 schema 暂不构建 `range_field` 和 `adgroup_id`，先验证主向量索引和 scalar 构建。
- 如果构建阶段内存不足，可在运行参数后增加：

```bash
--build_concurrency=false
```

如果 ARM 机器没有完整同版本仓库，仅复制这三个文件不够，因为该目标依赖整个 `common/`、`index_factory/` 和全部 Bazel 第三方配置；这种情况应复制整个源码仓库。


./devel/builder/bazel-7.4.1-linux-arm64 build \
  --config=linux_arm64 \
  //tools/index_factory:build_local_v1
