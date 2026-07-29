chmod +x devel/builder/bazel-7.4.1-linux-arm64

bash tools/index_factory/run_local_leaf_arm64_v1.sh start /opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/index_output 6635 --output_user_root=/opt/huawei/data3/g50064150/bazel-cache


mkdir -p /opt/huawei/data3/g50064150/bazel-cache

export TEST_TMPDIR=/opt/huawei/data3/g50064150/bazel-cache

cd /opt/huawei/data3/g50064150/falcon

bash tools/index_factory/run_local_leaf_arm64_bazel65.sh \
  start \
  /opt/huawei/data3/g50064150/falcon/index_output \
  6635

**#Error**
$TEST_TMPDIR defined: output root default is '/opt/huawei/data3/g50064150/bazel-cache' and max_idle_secs default is '15'.
Extracting Bazel installation...
Starting local Bazel server and connecting to it...
ERROR: Config value 'linux_arm64' is not defined in any .rc file


grep -n -- '--config=linux_arm64' \
  tools/index_factory/run_local_leaf_arm64_bazel65.sh

bash tools/index_factory/run_local_leaf_arm64_v1.sh start /opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/index_output 6635 --output_user_root=/opt/huawei/data3/g50064150/bazel-cache
[1/3] Building ARM64 //falcon/serving/leaf/bootstrap:leaf
$TEST_TMPDIR defined: output root default is '/opt/huawei/data3/g50064150/bazel-cache' and max_idle_secs default is '15'.
Starting local Bazel server and connecting to it...
INFO: Repository com_github_grpc_grpc instantiated at:
  /opt/huawei/data3/g50064150/falcon/WORKSPACE:7:10: in <toplevel>
  /opt/huawei/data3/g50064150/falcon/devel/builder/deps.bzl:16:14: in load_deps
  /opt/huawei/data3/g50064150/bazel-cache/_bazel_tysearch/e4b6c79cd8857e0e718b02230c162c4b/external/bazel_tools/tools/build_defs/repo/utils.bzl:233:18: in maybe
Repository rule http_archive defined at:
  /opt/huawei/data3/g50064150/bazel-cache/_bazel_tysearch/e4b6c79cd8857e0e718b02230c162c4b/external/bazel_tools/tools/build_defs/repo/http.bzl:372:31: in <toplevel>
WARNING: Download from https://cmc.cloudartifact.szv.dragon.tools.huawei.com/artifactory/opensource_general/gRPC/v1.65.4/package/grpc-1.65.4.zip failed: class com.google.devtools.build.lib.bazel.repository.downloader.UnrecoverableHttpException Unknown host: cmc.cloudartifact.szv.dragon.tools.huawei.com
ERROR: An error occurred during the fetch of repository 'com_github_grpc_grpc':
   Traceback (most recent call last):
        File "/opt/huawei/data3/g50064150/bazel-cache/_bazel_tysearch/e4b6c79cd8857e0e718b02230c162c4b/external/bazel_tools/tools/build_defs/repo/http.bzl", line 132, column 45, in _http_archive_impl
                download_info = ctx.download_and_extract(
Error in download_and_extract: java.io.IOException: Error downloading [https://cmc.cloudartifact.szv.dragon.tools.huawei.com/artifactory/opensource_general/gRPC/v1.65.4/package/grpc-1.65.4.zip] to /opt/huawei/data3/g50064150/bazel-cache/_bazel_tysearch/e4b6c79cd8857e0e718b02230c162c4b/external/com_github_grpc_grpc/temp3148825428414308911/grpc-1.65.4.zip: Unknown host: cmc.cloudartifact.szv.dragon.tools.huawei.com
ERROR: /opt/huawei/data3/g50064150/falcon/WORKSPACE:7:10: fetching http_archive rule //external:com_github_grpc_grpc: Traceback (most recent call last):
        File "/opt/huawei/data3/g50064150/bazel-cache/_bazel_tysearch/e4b6c79cd8857e0e718b02230c162c4b/external/bazel_tools/tools/build_defs/repo/http.bzl", line 132, column 45, in _http_archive_impl
                download_info = ctx.download_and_extract(
Error in download_and_extract: java.io.IOException: Error downloading [https://cmc.cloudartifact.szv.dragon.tools.huawei.com/artifactory/opensource_general/gRPC/v1.65.4/package/grpc-1.65.4.zip] to /opt/huawei/data3/g50064150/bazel-cache/_bazel_tysearch/e4b6c79cd8857e0e718b02230c162c4b/external/com_github_grpc_grpc/temp3148825428414308911/grpc-1.65.4.zip: Unknown host: cmc.cloudartifact.szv.dragon.tools.huawei.com
ERROR: Error computing the main repository mapping: no such package '@com_github_grpc_grpc//bazel': java.io.IOException: Error downloading [https://cmc.cloudartifact.szv.dragon.tools.huawei.com/artifactory/opensource_general/gRPC/v1.65.4/package/grpc-1.65.4.zip] to /opt/huawei/data3/g50064150/bazel-cache/_bazel_tysearch/e4b6c79cd8857e0e718b02230c162c4b/external/com_github_grpc_grpc/temp3148825428414308911/grpc-1.65.4.zip: Unknown host: cmc.cloudartifact.szv.dragon.tools.huawei.com
Loading: 
