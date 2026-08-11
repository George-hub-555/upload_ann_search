#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

case "$(uname -m)" in
  aarch64|arm64) ;;
  *)
    echo "ERROR: build_on_a.sh must run on Linux ARM64; detected $(uname -m)." >&2
    exit 2
    ;;
esac

command -v cmake >/dev/null || {
  echo "ERROR: cmake is required." >&2
  exit 2
}
command -v g++ >/dev/null || {
  echo "ERROR: g++ is required." >&2
  exit 2
}

# 只检查仓库中的离线源码，不执行 git clone、pip 或任何联网下载。
# 这里全部使用相对于 test_all 的路径，A/B 目录前缀不同也不受影响。
for required_source in \
  ../faiss/CMakeLists.txt \
  ../hnswlib/hnswlib-master/hnswlib/hnswlib.h \
  ../NGT/NGT-main/CMakeLists.txt; do
  if [[ ! -f "$required_source" ]]; then
    echo "ERROR: missing offline source: $required_source" >&2
    exit 2
  fi
done

build_dir="build-aarch64"
blas_vendor="${BLAS_VENDOR:-OpenBLAS}"

cmake -S . -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_FLAGS_RELEASE="-O3 -DNDEBUG" \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DNDEBUG" \
  -DBLA_VENDOR="$blas_vendor"

cmake --build "$build_dir" \
  --target ann_faiss_benchmark ann_hnswlib_benchmark ann_ngt_benchmark \
  -j "${BUILD_JOBS:-$(nproc)}"
cmake --install "$build_dir" --prefix . --component ann_benchmarks

{
  echo "build_arch=$(uname -m)"
  echo "build_kernel=$(uname -sr)"
  echo "compiler=$(g++ --version | head -n 1)"
  echo "blas_vendor=$blas_vendor"
  echo "faiss_opt_level=generic"
  echo "native_cpu_flags=disabled"
  echo "hnswlib_source=../hnswlib/hnswlib-master"
  echo "ngt_source=../NGT/NGT-main"
} > bin/build_info.txt

echo "Built: bin/ann_faiss_benchmark"
echo "Built: bin/ann_hnswlib_benchmark"
echo "Built: bin/ann_ngt_benchmark"
echo "Copy the repository with the same relative layout to machine B."
