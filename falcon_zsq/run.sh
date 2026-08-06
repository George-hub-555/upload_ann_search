#!/bin/bash
# run.sh - B 机器 (测试机) 用: 运行 A 机器编译好的测试二进制
#
# 前提: B 机器上已经解压 A 机器打包的 dist/linux_falcon_zsq_test.tar.gz
#   解压后目录结构:
#   linux_falcon_zsq_test/
#   ├── linux_falcon_zsq_test          (二进制)
#   ├── linux_falcon_zsq_test.runfiles/ (运行时依赖 + smoke 数据)
#   └── run.sh                         (本脚本, 从 Opencode_done/linux_falcon_zsq/ 拷来)
#
# 用法:
#   tar xzf linux_falcon_zsq_test.tar.gz
#   cd linux_falcon_zsq_test
#   ./run.sh smoke                          # 1k base smoke (数据在 runfiles 里)
#   ./run.sh full /path/to/sift/dataset     # SIFT-1M full (需指定数据目录)
#   ./run.sh list                           # 列出所有测试 case
#
# 注意:
#   - B 机器需与 A 机器相同架构 (aarch64) 和相近的 glibc 版本
#   - 如缺 .so, 用 ldd linux_falcon_zsq_test 查依赖, 装 corresponding 包
#   - smoke 数据 (sift_base.dat 等) 在 runfiles 里, 不需要额外数据
#   - full 模式需要 SIFT-1M 数据集 (falcon/dataset/sift_*.fvecs)

set -e

# 1. 定位二进制和 runfiles (相对本脚本所在目录)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$SCRIPT_DIR/linux_falcon_zsq_test"
RUNFILES="$SCRIPT_DIR/linux_falcon_zsq_test.runfiles"

if [ ! -x "$BIN" ]; then
    echo "ERROR: binary not found or not executable: $BIN"
    echo "       make sure you extracted the tar.gz in this directory"
    echo "       tar xzf linux_falcon_zsq_test.tar.gz"
    exit 1
fi

if [ ! -d "$RUNFILES" ]; then
    echo "ERROR: runfiles directory not found: $RUNFILES"
    echo "       make sure you extracted the full tar.gz (not just the binary)"
    exit 1
fi

# 2. 设置 RUNFILES_DIR (bazel 二进制通过这个环境变量定位运行时依赖)
export RUNFILES_DIR="$RUNFILES"

MODE="${1:-smoke}"

case "$MODE" in
    smoke)
        # smoke: 数据 (sift_*.dat) 在 runfiles 里, 直接跑
        echo "[run] smoke test (1k base, data in runfiles)"
        "$BIN" --gtest_filter=FalconZSQCompare.SmokeSIFT1k
        ;;
    full)
        # full: 需要 SIFT-1M 数据集目录 (含 sift_base.fvecs 等)
        DATA_DIR="${2:-}"
        if [ -z "$DATA_DIR" ]; then
            echo "ERROR: data directory not specified"
            echo "Usage: $0 full <data_dir>"
            echo "  data_dir should contain sift_base.fvecs, sift_query.fvecs, sift_groundtruth.ivecs"
            echo "  e.g. ./run.sh full /path/to/falcon/dataset"
            exit 1
        fi
        if [ ! -f "$DATA_DIR/sift_base.fvecs" ]; then
            echo "ERROR: sift_base.fvecs not found in $DATA_DIR"
            exit 1
        fi
        echo "[run] full SIFT-1M test (data from $DATA_DIR)"
        export FALCON_SIFT_DIR="$DATA_DIR"
        export FALCON_RUN_FULL=1
        "$BIN" --gtest_filter=FalconZSQCompare.FullSIFT1M
        ;;
    list)
        # 列出所有测试 case
        "$BIN" --gtest_list_tests
        ;;
    *)
        echo "Usage: $0 [smoke|full <data_dir>|list]"
        echo "  smoke (default): 1k base, data in runfiles, 几秒跑完"
        echo "  full <dir>:     SIFT-1M, need data dir with sift_*.fvecs, 构图几分钟"
        echo "  list:           list all test cases"
        exit 1
        ;;
esac
