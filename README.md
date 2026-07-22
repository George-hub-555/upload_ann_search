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
