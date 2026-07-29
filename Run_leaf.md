chmod +x devel/builder/bazel-7.4.1-linux-arm64

bash tools/index_factory/run_local_leaf_arm64_v1.sh \
  start /绝对路径/index_output 6635 --output_user_root=/opt/huawei/data3/g50064150/bazel-cache


mkdir -p /opt/huawei/data3/g50064150/bazel-cache

export TEST_TMPDIR=/opt/huawei/data3/g50064150/bazel-cache

cd /opt/huawei/data3/g50064150/falcon

bash tools/index_factory/run_local_leaf_arm64_bazel65.sh \
  start \
  /opt/huawei/data3/g50064150/falcon/index_output \
  6635
