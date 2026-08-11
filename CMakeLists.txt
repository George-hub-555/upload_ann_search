cmake_minimum_required(VERSION 3.22.0 FATAL_ERROR)

project(ann_arm_benchmarks LANGUAGES C CXX)

set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Build a generic CPU binary. Machine A and machine B may use different ARM
# cores, so this project deliberately never adds -march=native/-mcpu=native.
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(FAISS_ENABLE_GPU OFF CACHE BOOL "" FORCE)
set(FAISS_ENABLE_PYTHON OFF CACHE BOOL "" FORCE)
set(FAISS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(FAISS_ENABLE_EXTRAS OFF CACHE BOOL "" FORCE)
set(FAISS_ENABLE_MKL OFF CACHE BOOL "" FORCE)
set(FAISS_ENABLE_SVS OFF CACHE BOOL "" FORCE)
set(FAISS_OPT_LEVEL generic CACHE STRING "" FORCE)

# NGT 上游默认启用 -march=native；这里关闭它，保证 A 机构建的程序能在
# 微架构不同的 ARMv8 B 机运行。QG/QBG 需要更多 BLAS/LAPACK 组件，本轮只
# 编译普通 NGT，减少离线依赖。
set(NGT_QBG_DISABLED ON CACHE BOOL "" FORCE)
set(NGT_MARCH_NATIVE_DISABLED ON CACHE BOOL "" FORCE)
set(NGT_AVX_DISABLED OFF CACHE BOOL "" FORCE)
set(NGT_SIMD_DISABLED OFF CACHE BOOL "" FORCE)
set(NGT_OPENMP_DISABLED OFF CACHE BOOL "" FORCE)
set(NGT_ASSERT_DISABLED ON CACHE BOOL "" FORCE)
set(NGT_SHARED_MEMORY_ALLOCATOR OFF CACHE BOOL "" FORCE)
set(NGT_BFLOAT_DISABLED ON CACHE BOOL "" FORCE)
set(HNSWLIB_EXAMPLES OFF CACHE BOOL "" FORCE)

find_package(OpenMP REQUIRED)
find_package(Threads REQUIRED)

add_subdirectory(../faiss faiss-build EXCLUDE_FROM_ALL)
add_subdirectory(../hnswlib/hnswlib-master hnswlib-build EXCLUDE_FROM_ALL)
add_subdirectory(../NGT/NGT-main ngt-build EXCLUDE_FROM_ALL)

# 三个适配器只使用 C++17；不无故要求 C++20，可兼容更多 ARM GCC 版本。
add_executable(ann_faiss_benchmark faiss_benchmark.cpp)
set_property(TARGET ann_faiss_benchmark PROPERTY CXX_STANDARD 17)
set_property(TARGET ann_faiss_benchmark PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(ann_faiss_benchmark PRIVATE faiss OpenMP::OpenMP_CXX)

add_executable(ann_hnswlib_benchmark hnswlib_benchmark.cpp)
set_property(TARGET ann_hnswlib_benchmark PROPERTY CXX_STANDARD 17)
set_property(TARGET ann_hnswlib_benchmark PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(
    ann_hnswlib_benchmark
    PRIVATE hnswlib::hnswlib OpenMP::OpenMP_CXX Threads::Threads)

add_executable(ann_ngt_benchmark ngt_benchmark.cpp)
set_property(TARGET ann_ngt_benchmark PROPERTY CXX_STANDARD 17)
set_property(TARGET ann_ngt_benchmark PROPERTY CXX_STANDARD_REQUIRED ON)
target_include_directories(
    ann_ngt_benchmark
    PRIVATE
        ../NGT/NGT-main/lib
        ${CMAKE_CURRENT_BINARY_DIR}/ngt-build/lib)
target_link_libraries(
    ann_ngt_benchmark
    PRIVATE ngtstatic OpenMP::OpenMP_CXX Threads::Threads rt)

# 独立安装组件可避免第三方子工程的 install() 规则把额外头文件/库复制进来。
install(
    TARGETS ann_faiss_benchmark ann_hnswlib_benchmark ann_ngt_benchmark
    RUNTIME DESTINATION bin
    COMPONENT ann_benchmarks)
