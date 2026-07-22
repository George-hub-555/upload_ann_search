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

**#Add**
faiss-main/bench_glove.cpp


**#add to tutorial/cpp/CMakeLists.txt**
add_executable(bench_glove EXCLUDE_FROM_ALL bench_glove.cpp)
target_link_libraries(bench_glove PRIVATE faiss)
