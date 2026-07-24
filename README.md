# upload_ann_search
upload ann search for arm


instruct :
cmake -B build . -DFAISS_ENABLE_GPU=OFF -DFAISS_ENABLE_PYTHON=OFF -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DFAISS_OPT_LEVEL=generic

make -C build -j$(nproc)


g++ -O3 -march=armv8-a -std=c++17 -I. -o bench_glove bench_glove.cpp \
    -Lbuild/faiss -lfaiss -lopenblas -lpthread -Wl,-rpath,build/faiss

cmake -B build . -DFAISS_ENABLE_GPU=OFF -DFAISS_ENABLE_PYTHON=OFF -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DFAISS_OPT_LEVEL=generic -DBLA_VENDOR=OpenBLAS
make -C build -j$(nproc) bench_glove

./bench_glove

**Add parameters Version**
# 编译（和原版一样）
g++ -O3 -march=armv8-a -std=c++17 -I. -o bench_glove_custom bench_glove_custom.cpp \
    -Lbuild/faiss -lfaiss -lopenblas -lpthread -Wl,-rpath,build/faiss

# 例1：只取 20 万条向量，不出进度
./bench_glove_custom -maxturn 200000

# 例2：取 50 万条，每 1000 个 query 报一次进度
./bench_glove_custom -maxturn 500000 -reportfreq 1000

# 例3：改 k 值和 dataset 路径
./bench_glove_custom -dataset dataset/glove.twitter.27B.200d.txt -k 20 -maxturn 300000 -reportfreq 500

# 查看帮助
./bench_glove_custom --help

**#Add**
faiss-main/bench_glove.cpp


**#add to tutorial/cpp/CMakeLists.txt**
add_executable(bench_glove EXCLUDE_FROM_ALL bench_glove.cpp)
target_link_libraries(bench_glove PRIVATE faiss)

[0.110 s] Training IVFPQ(nlist=100, m=8) ...
terminate called after throwing an instance of 'faiss::FaissException'
  what():  Error in void faiss::ProductQuantizer::set_derived_values() at /home/tysearch/g500_test/faiss/faiss-main/faiss/impl/ProductQuantizer.cpp:64: Error: '!(d % M == 0)' failed: The dimension of the vector (d) should be a multiple of the number of subquantizers (M)
Aborted (core dumped)

**复制文件到目标机器**
scp 10.37.1.10:/home/tysearch/g500_test/faiss/faiss-main/bench_results.csv ./
scp bench_glove_custom.cpp 10.37.1.10:/home/tysearch/g500_test/faiss/faiss-main/

ssh 10.37.1.10 "rm /home/tysearch/g500_test/faiss/faiss-main/progress*.csv"
scp 10.37.1.10:/home/tysearch/g500_test/faiss/faiss-main/progress*.csv ./Res/

cc_binary(
    name = "sift1m_blink_graph_benchmark",
    srcs = ["sift1m_blink_graph_benchmark.cpp"],
    visibility = ["//visibility:public"],
    deps = [
        "//common/shard_format/bundles:bundle_file_reader",
        "//common/shard_format/bundles:bundle_file_writer",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_erq_builder",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_erq_searcher_adaptive",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_rabitq_builder",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_rabitq_searcher",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_rabitq_searcher_adaptive",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_zsq_builder",
        "//common/shard_format/fusion_index/embedding_index/quantizer_index/rabitq_index/rabitq_codec:blink_graph_zsq_searcher_adaptive",
        "@com_github_gflags_gflags//:gflags",
    ],
)

bazel build \
  --config=linux_arm64 \
  --copt=-march=armv8.2-a+crypto+crc+dotprod \
  --cxxopt=-march=armv8.2-a+crypto+crc+dotprod \
  //tools/index_factory:sift1m_blink_graph_benchmark

bazel build \
  --config=linux_arm64 \
  --copt=-march=armv8-a+fp+simd+crypto+crc \
  --cxxopt=-march=armv8-a+fp+simd+crypto+crc \
  //tools/index_factory:sift1m_blink_graph_benchmark

./bazel-bin/tools/index_factory/sift1m_blink_graph_benchmark \
  --base=dataset/sift_base.fvecs \
  --query=dataset/sift_query.fvecs \
  --groundtruth=dataset/sift_groundtruth.ivecs \
  --index=dataset/sift1m_erq_blink_graph.index \
  --csv=sift1m_arm_erq.csv \
  --build=true \
  --index_type=2 \
  --dim=128 \
  --base_limit=0 \
  --query_limit=10000 \
  --top_k=10 \
  --thread_count=8 \
  --link_range=32 \
  --batch_size=1024 \
  --search_ranges=20,40,80,120,160,200,300,400 \
  --warmup=200 \
  --repeats=3

uname -m
aarch64
command -v g++
/usr/bin/g++
g++ --version
g++ (GCC) 10.3.1
Copyright (C) 2020 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
command -v make
/usr/bin/make
make --version
GNU Make 4.3
Built for aarch64-openEuler-linux-gnu
Copyright (C) 1988-2020 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
command -v cmake
/usr/bin/cmake
cmake --version
cmake version 3.22.0

CMake suite maintained and supported by Kitware (kitware.com/cmake).

uname -m

command -v g++
g++ --version

command -v cmake
cmake --version

command -v make
make --version

command -v protoc
protoc --version

ldconfig -p 2>/dev/null | grep -E "protobuf|gflags"

uname -m
aarch64
[tysearch@default-keplerengine-37-1-10 falcon-main]$ 
[tysearch@default-keplerengine-37-1-10 falcon-main]$ command -v g++
/usr/bin/g++
[tysearch@default-keplerengine-37-1-10 falcon-main]$ g++ --version
g++ (GCC) 10.3.1
Copyright (C) 2020 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

[tysearch@default-keplerengine-37-1-10 falcon-main]$ 
[tysearch@default-keplerengine-37-1-10 falcon-main]$ command -v cmake
/usr/bin/cmake
[tysearch@default-keplerengine-37-1-10 falcon-main]$ cmake --version
cmake version 3.22.0

CMake suite maintained and supported by Kitware (kitware.com/cmake).
[tysearch@default-keplerengine-37-1-10 falcon-main]$ 
[tysearch@default-keplerengine-37-1-10 falcon-main]$ command -v make
/usr/bin/make
[tysearch@default-keplerengine-37-1-10 falcon-main]$ make --version
GNU Make 4.3
Built for aarch64-openEuler-linux-gnu
Copyright (C) 1988-2020 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
[tysearch@default-keplerengine-37-1-10 falcon-main]$ 
[tysearch@default-keplerengine-37-1-10 falcon-main]$ command -v protoc
[tysearch@default-keplerengine-37-1-10 falcon-main]$ protoc --version
-bash: protoc: command not found
[tysearch@default-keplerengine-37-1-10 falcon-main]$ 
[tysearch@default-keplerengine-37-1-10 falcon-main]$ ldconfig -p 2>/dev/null | grep -E "protobuf|gflags"
[tysearch@default-keplerengine-37-1-10 falcon-main]$ 

https://github.com/bazelbuild/bazel/releases/download/7.4.1/bazel-7.4.1-linux-arm64

g++ -O3 -march=armv8-a -std=c++17 -I. \
  -o bench_sift_custom bench_sift_custom.cpp \
  -Lbuild/faiss -lfaiss -lopenblas -fopenmp -lpthread \
  -Wl,-rpath,build/faiss

./bench_sift_custom -maxtrn 5000 -reportfreq 500 -k 10

./bench_sift_custom \
  -maxtrn 5000 \
  -reportfreq 500 \
  -k 10


**sift_faiss_ivfpqfs:**
g++ -O3 -march=armv8-a -std=c++17 -I. -o bench_fastscan bench_fastscan.cpp \
    -Lbuild/faiss -lfaiss -lopenblas -lpthread -Wl,-rpath,build/faiss

./bench_fastscan \
  -fvecs \
  -dataset dataset/sift_base.fvecs \
  -learn dataset/sift_learn.fvecs \
  -query dataset/sift_query.fvecs \
  -gt dataset/sift_groundtruth.ivecs \
  -k 10 \
  -nlist 1204 \
  -m 64 \
  -nprobe_sweep 1,2,4,8,16,32,64,128,256 \
  -k_reorder 0

./bench_fastscan \
  -fvecs \
  -dataset dataset/sift_base.fvecs \
  -learn dataset/sift_learn.fvecs \
  -query dataset/sift_query.fvecs \
  -gt dataset/sift_groundtruth.ivecs \
  -k 10 \
  -nlist 1204 \
  -m 64 \
  -nprobe_sweep 1,2,4,8,16,32,64,128,256 \
  -k_reorder 10

**sift_faiss_ivfpqfs_codex:**
-nlist       IVF 倒排列表数量，默认 512
-nprobe      每次查询探测列表数量，默认 5
-m           PQ 子量化器数量，默认 64
-k_reorder   精确重排倍率，0 表示关闭
-bbs         FastScan 块大小，默认 32
-k           Top-K
-maxtrn      最大查询轮数
-reportfreq  每多少轮在终端打印局部统计

g++ -O3 -march=armv8-a -std=c++17 -I. \
  -o bench_sift_faisspqfs bench_sift_faisspqfs.cpp \
  -Lbuild/faiss -lfaiss -lopenblas -fopenmp -lpthread \
  -Wl,-rpath,build/faiss

./bench_sift_faisspqfs \
  -indexes ivfpqfs \
  -nlist 512 \
  -nprobe 5 \
  -m 64 \
  -k_reorder 0 \
  -k 10 \
  -maxtrn 1000000 \
  -reportfreq 1000

./bench_sift_faisspqfs \
  -indexes ivfflat,ivfpq,ivfpqfs \
  -nlist 512 \
  -nprobe 1,2,4,8,16,32,64,128 \
  -m 64 \
  -k_reorder 0 \
  -k 10 \
  -maxtrn 1000000 \
  -reportfreq 1000

./bench_sift_faisspqfs \
  -indexes ivfpqfs \
  -nlist 512 \
  -nprobe 5 \
  -m 64 \
  -k_reorder 10 \
  -k 10

**glove200_faiss_ivfpqfs_codex:**
g++ -O3 -march=armv8-a -std=c++17 -I. \
  -o bench_glove200_faisspqfs bench_glove200_faisspqfs.cpp \
  -Lbuild/faiss -lfaiss -lopenblas -fopenmp -lpthread \
  -Wl,-rpath,build/faiss

./bench_glove200_faisspqfs \
  -dataset dataset/glove.twitter.27B.200d.txt \
  -indexes all \
  -nlist 128,256,512,1024,2048,4096 \
  -nprobe 1,2,4,8,16,32,64,128,256 \
  -m 100 \
  -k_reorder 10 \
  -k 10 \
  -maxtrn 10000 \
  -reportfreq 1000
