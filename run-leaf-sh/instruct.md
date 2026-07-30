**#Build**
cd /opt/huawei/data3/g50064150/falcon

bash tools/test/perf/build_leaf_perf_package_arm64_bazel65.sh

dist/leaf_perf_arm64_bazel65_20260730_120000.tar.gz

BAZEL=/实际路径/bazel-6.5.0 bash tools/test/perf/build_leaf_perf_package_arm64_bazel65.sh




**#Run**
mkdir -p /opt/huawei/data3/g50064150/leaf_perf_runtime
tar -xzf leaf_perf_arm64_bazel65_20260730_120000.tar.gz \
  -C /opt/huawei/data3/g50064150/leaf_perf_runtime

cd /opt/huawei/data3/g50064150/leaf_perf_runtime/leaf_perf_arm64_bazel65_20260730_120000


bash run_leaf_perf_package_arm64.sh all \
  /opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/index_output \
  /opt/huawei/data3/g50064150/falcon/dataset/1kw_64dim_test_data/request.txt





**#View Logs**

bash run_leaf_perf_package_arm64.sh logs
bash run_leaf_perf_package_arm64.sh stop
