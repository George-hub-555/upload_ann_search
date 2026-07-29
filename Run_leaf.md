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
