#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

case "$(uname -m)" in
  aarch64|arm64) ;;
  *)
    echo "ERROR: run_on_b.sh must run on Linux ARM64; detected $(uname -m)." >&2
    exit 2
    ;;
esac

command -v python3 >/dev/null || {
  echo "ERROR: python3 is required." >&2
  exit 2
}

# Linux 可执行权限属于文件元数据。通过 Windows、FAT/U 盘或部分压缩工具复制
# 时，这个权限位可能丢失；B 机上的 Python 随后会报 Errno 13 Permission denied。
# 在测试前统一补回当前用户的执行权限，不需要重新编译算法。
benchmark_binaries=(
  bin/ann_faiss_benchmark
  bin/ann_hnswlib_benchmark
  bin/ann_ngt_benchmark
)

for benchmark_binary in "${benchmark_binaries[@]}"; do
  if [[ ! -f "$benchmark_binary" ]]; then
    echo "ERROR: missing benchmark binary: $benchmark_binary" >&2
    echo "Please copy test_all/bin from machine A to the same relative path on machine B." >&2
    exit 2
  fi

  chmod u+x "$benchmark_binary" || {
    echo "ERROR: cannot add execute permission to $benchmark_binary" >&2
    echo "Check the file owner and whether the filesystem is read-only." >&2
    exit 2
  }

  if [[ ! -x "$benchmark_binary" ]]; then
    echo "ERROR: $benchmark_binary is still not executable." >&2
    echo "The test_all directory may be on a filesystem mounted with noexec." >&2
    echo "Copy the repository to an executable Linux filesystem and retry." >&2
    exit 2
  fi
done

python3 run_benchmarks.py --config config.json
