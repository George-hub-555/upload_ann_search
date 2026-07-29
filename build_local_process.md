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
cd /opt/huawei/data3/g50064150/falcon

mkdir -p \
  /opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/index_output

chmod +x tools/index_factory/build_local_v1

./tools/index_factory/build_local_v1 \
  --schema_path=/opt/huawei/data3/g50064150/falcon/tools/index_factory/schema.json \
  --data_path=/opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/falcon_data/0 \
  --output_path=/opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/index_output \
  --shard_id=0
  
**#Error**
 ./tools/index_factory/build_local_v1 \
>   --schema_path=/opt/huawei/data3/g50064150/falcon/tools/index_factory/schema.json \
>   --data_path=/opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/falcon_data/0 \
>   --output_path=/opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/index_output \
>   --shard_id=0
2026-07-29 16:00:13,795|INFO|{"address": "127.0.0.1", "details": "[index_factory/public/writer/section/fusion_index/fusion_index_writer.cpp:62] No other index"}
2026-07-29 16:00:13,797|ERROR|{"address": "127.0.0.1", "details": "[index_factory/public/writer/section/fusion_index/fusion_index_writer.cpp:236] Faiss model ivf centroids number is invalid, cluster count16384"}
2026-07-29 16:00:13,797|ERROR|{"address": "127.0.0.1", "details": "[index_factory/public/writer/section/fusion_index/fusion_index_writer.cpp:229] Set centroids failed"}
2026-07-29 16:00:13,797|ERROR|{"address": "127.0.0.1", "details": "[index_factory/public/writer/section/fusion_index/fusion_index_writer.cpp:159] Parse model failed"}
2026-07-29 16:00:13,797|ERROR|{"address": "127.0.0.1", "details": "[index_factory/public/writer/section/fusion_index/fusion_index_writer.cpp:126] Init kmeans failed"}
2026-07-29 16:00:13,797|ERROR|{"address": "127.0.0.1", "details": "[index_factory/public/builder/fusion_vector_section_builder.cpp:391] Open fusion vector writer failed."}
2026-07-29 16:00:13,797|ERROR|{"address": "127.0.0.1", "details": "[index_factory/public/builder/fusion_vector_section_builder.cpp:207] init fusion index writer failed."}
2026-07-29 16:00:13,797|ERROR|{"address": "127.0.0.1", "details": "[index_factory/index_builder/build_section_index.cpp:167] vector section finalize failed."}
2026-07-29 16:00:13,797|INFO|{"address": "127.0.0.1", "details": "[common/shard_format/bundles/bundle_file_writer.cpp:131] Bundle file path /opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/index_output/vectorIndex_relevance_softLtrAfmConditionv1//shard00000.section.relevance_softLtrAfmConditionv1 length 0
"}
2026-07-29 16:00:14,153|ERROR|{"address": "127.0.0.1", "details": "[index_factory/index_builder/index_builder_impl.cpp:81] Failed to build section"}
2026-07-29 16:00:14,312|ERROR|{"address": "127.0.0.1", "details": "[tools/index_factory/build_local_v1.cpp:86] local index build failed"}



```bash
cd /实际路径/falcon

bazel build \
  --config=linux_arm64 \
  //tools/index_factory:build_local_v1
```

运行：
chmod +x /home/tysearch/g500_test/falcon/tools/index_factory/build_local_v1
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


./devel/builder/bazel-6.5.0-linux-arm64 build \
  --config=linux_arm64 \
  //tools/index_factory:build_local_v1

**#报错仅表示远程仓库没有定义 linux_arm64 配置。**
cd /home/tysearch/g500_test/falcon

./devel/builder/bazel-6.5.0-linux-arm64 build \
  --copt=-march=armv8.2-a+crypto+crc+dotprod \
  --cxxopt=-march=armv8.2-a+crypto+crc+dotprod \
  //tools/index_factory:build_local_v1
**#如果不需要指定 ARM 指令集，也可以直接：**
./devel/builder/bazel-6.5.0-linux-arm64 build \
  //tools/index_factory:build_local_v1

./devel/builder/bazel-6.5.0-linux-arm64 build \
>   --copt=-march=armv8.2-a+crypto+crc+dotprod \
>   --cxxopt=-march=armv8.2-a+crypto+crc+dotprod \
>   //tools/index_factory:build_local_v1
$TEST_TMPDIR defined: output root default is '/opt/huawei/data1/bazel-cache' and max_idle_secs default is '15'.
Starting local Bazel server and connecting to it...
ERROR: Skipping '//tools/index_factory:build_local_v1': no such target '//tools/index_factory:build_local_v1': target 'build_local_v1' not declared in package 'tools/index_factory' defined by /root/g50064150/falcon/tools/index_factory/BUILD (Tip: use `query "//tools/index_factory:*"` to see all the targets in that package)
WARNING: Target pattern parsing failed.
ERROR: no such target '//tools/index_factory:build_local_v1': target 'build_local_v1' not declared in package 'tools/index_factory' defined by /root/g50064150/falcon/tools/index_factory/BUILD (Tip: use `query "//tools/index_factory:*"` to see all the targets in that package)
INFO: Elapsed time: 8.840s
INFO: 0 processes.
FAILED: Build did NOT complete successfully (1 packages loaded)

**#Error**
The Boost C++ Libraries were successfully built!

The following directory should be added to compiler include paths:

    /opt/huawei/data1/bazel-cache/_bazel_root/c309df92d697e21992c95b5ef7ff6b65/sandbox/linux-sandbox/199/execroot/falcon/external/com_github_boostorg_boost

The following directory should be added to linker library paths:

    /opt/huawei/data1/bazel-cache/_bazel_root/c309df92d697e21992c95b5ef7ff6b65/sandbox/linux-sandbox/199/execroot/falcon/external/com_github_boostorg_boost/stage/lib

/opt/huawei/data1/bazel-cache/_bazel_root/c309df92d697e21992c95b5ef7ff6b65/sandbox/linux-sandbox/199/execroot/falcon
INFO: From Executing genrule @log4cplus//:log4cplus-compile:
/opt/huawei/data1/bazel-cache/_bazel_root/c309df92d697e21992c95b5ef7ff6b65/sandbox/linux-sandbox/5/execroot/falcon
INFO: From Compiling src/google/protobuf/compiler/rust/relative_path.cc [for tool]:
external/com_google_protobuf/src/google/protobuf/compiler/rust/relative_path.cc: In member function 'std::string google::protobuf::compiler::rust::RelativePath::Relative(const google::protobuf::compiler::rust::RelativePath&) const':
external/com_google_protobuf/src/google/protobuf/compiler/rust/relative_path.cc:66:21: warning: comparison of integer expressions of different signedness: 'int' and 'std::vector<std::basic_string_view<char> >::size_type' {aka 'long unsigned int'} [-Wsign-compare]
   66 |   for (int i = 0; i < current_segments.size(); ++i) {
      |                   ~~^~~~~~~~~~~~~~~~~~~~~~~~~
INFO: From Compiling src/google/protobuf/wire_format_lite.cc [for tool]:
external/com_google_protobuf/src/google/protobuf/wire_format_lite.cc:669: warning: ignoring '#pragma clang loop' [-Wunknown-pragmas]
  669 | #pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      | 
external/com_google_protobuf/src/google/protobuf/wire_format_lite.cc:711: warning: ignoring '#pragma clang loop' [-Wunknown-pragmas]
  711 | #pragma clang loop vectorize(disable) unroll(disable) interleave(disable)
      | 
INFO: From Compiling src/google/protobuf/arena.cc [for tool]:
external/com_google_protobuf/src/google/protobuf/arena.cc: In member function 'void* google::protobuf::internal::SerialArena::AllocateAlignedFallback(size_t)':
external/com_google_protobuf/src/google/protobuf/arena.cc:194:10: warning: 'ret' may be used uninitialized in this function [-Wmaybe-uninitialized]
  194 |   return ret;
      |          ^~~
INFO: From Compiling src/google/protobuf/generated_message_tctable_lite.cc [for tool]:
In file included from bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/generated_message_tctable_decl.h:23,
                 from external/com_google_protobuf/src/google/protobuf/generated_message_tctable_lite.cc:23:
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1077:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1077 | ParseContext::ParseLengthDelimitedInlined(const char* ptr, const Func& func) {
      | ^~~~~~~~~~~~
bazel-out/aarch64-opt-exec-2B5CBBC6/bin/external/com_google_protobuf/src/google/protobuf/_virtual_includes/protobuf_lite/google/protobuf/parse_context.h:1091:1: warning: 'always_inline' function might not be inlinable [-Wattributes]
 1091 | ParseContext::ParseGroupInlined(const char* ptr, uint32_t start_tag,
      | ^~~~~~~~~~~~
external/com_google_protobuf/src/google/protobuf/generated_message_tctable_lite.cc:838:36: warning: 'always_inline' function might not be inlinable [-Wattributes]
  838 | PROTOBUF_ALWAYS_INLINE const char* TcParser::FastVarintS1(
      |                                    ^~~~~~~~
external/com_google_protobuf/src/google/protobuf/generated_message_tctable_lite.cc:838:36: warning: 'always_inline' function might not be inlinable [-Wattributes]
external/com_google_protobuf/src/google/protobuf/generated_message_tctable_lite.cc:761:29: warning: 'always_inline' function might not be inlinable [-Wattributes]
  761 | PROTOBUF_ALWAYS_INLINE bool EnumIsValidAux(int32_t val, uint16_t xform_val,
      |                             ^~~~~~~~~~~~~~
external/com_google_protobuf/src/google/protobuf/generated_message_tctable_lite.cc:749:29: warning: 'always_inline' function might not be inlinable [-Wattributes]
  749 | PROTOBUF_ALWAYS_INLINE void PrefetchEnumData(uint16_t xform_val,
      |                             ^~~~~~~~~~~~~~~~
Target //tools/index_factory:build_local_v1 up-to-date:
  bazel-bin/tools/index_factory/build_local_v1
INFO: Elapsed time: 383.703s, Critical Path: 226.38s
INFO: 1173 processes: 376 internal, 797 linux-sandbox.
INFO: Build completed successfully, 1173 total actions
