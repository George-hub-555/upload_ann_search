#!/bin/bash
# run.sh - 在 falcon 树内创建符号链接并跑 bazel test
# 用法:
#   ./run.sh           # smoke (1k base, 几秒)
#   ./run.sh full      # 完整 SIFT-1M (1M base + 10k query, 构图几分钟)
#   ./run.sh smoke     # 显式 smoke
#
# 目录结构假设 (相对路径):
#   某根目录/
#   ├── falcon/                        # falcon 源码 (含 WORKSPACE)
#   └── Opencode_done/
#       └── linux_falcon_zsq/          # 本脚本所在目录
#           ├── run.sh                 # 本脚本
#           ├── BUILD
#           ├── linux_falcon_zsq_test.cpp
#           └── README.md
#
# 跨机器: 整个 "某根目录" 拷到不同 Linux aarch64 机器, 路径都相对, 不用改.

set -e

# 1. 定位路径 (全部相对, 不用绝对路径)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# falcon 在本目录往上两级再进 falcon: Opencode_done/linux_falcon_zsq -> ../.. -> falcon
FALCON_DIR="$(cd "$SCRIPT_DIR/../../falcon" && pwd)"

if [ ! -f "$FALCON_DIR/WORKSPACE" ]; then
    echo "ERROR: falcon WORKSPACE not found at $FALCON_DIR"
    echo "       expected: <root>/falcon/WORKSPACE"
    echo "       current:  <root>/Opencode_done/linux_falcon_zsq/run.sh"
    exit 1
fi

# 2. 在 falcon 树内创建符号链接 (不修改 falcon 源码, 只加一个 link)
#    falcon/linux_falcon_zsq -> Opencode_done/linux_falcon_zsq
LINK_PATH="$FALCON_DIR/linux_falcon_zsq"
if [ ! -L "$LINK_PATH" ]; then
    if [ -e "$LINK_PATH" ]; then
        echo "ERROR: $LINK_PATH exists but is not a symlink. remove it first."
        exit 1
    fi
    ln -s "$SCRIPT_DIR" "$LINK_PATH"
    echo "[setup] created symlink: $LINK_PATH -> $SCRIPT_DIR"
fi

# 3. 进入 falcon 根目录跑 bazel (bazel 必须在 WORKSPACE 根执行)
cd "$FALCON_DIR"

# 4. 用 falcon 自带的 bazel 二进制 (devel/builder/bazel-6.5.0-linux-arm64)
BAZEL_BINARY="devel/builder/bazel-6.5.0-linux-arm64"
if [ ! -x "$BAZEL_BINARY" ]; then
    echo "ERROR: bazel binary not found: $FALCON_DIR/$BAZEL_BINARY"
    echo "       check falcon/devel/builder/ for the correct version"
    # 兜底: 用系统 bazel (版本可能不匹配)
    BAZEL_BINARY="bazel"
    echo "       fallback to system bazel: $BAZEL_BINARY"
fi

MODE="${1:-smoke}"

case "$MODE" in
    smoke)
        echo "[run] smoke test (1k base, falcon/resource data)"
        $BAZEL_BINARY test --config=linux_arm64 --test_output=all \
            //linux_falcon_zsq:linux_falcon_zsq_test
        ;;
    full)
        echo "[run] full SIFT-1M test (1M base + 10k query)"
        # --spawn_strategy=local: 绕过沙箱, 允许读 falcon/dataset/ (不在 bazel data 中)
        # FALCON_SIFT_DIR: 用 $(pwd) 动态生成绝对路径 (bazel test_env 需要绝对路径)
        $BAZEL_BINARY test --config=linux_arm64 --test_output=all --spawn_strategy=local \
            --test_env=FALCON_SIFT_DIR="$(pwd)/falcon/dataset" \
            --test_env=FALCON_RUN_FULL=1 \
            --test_filter=FalconZSQCompare.FullSIFT1M \
            //linux_falcon_zsq:linux_falcon_zsq_test
        ;;
    *)
        echo "Usage: $0 [smoke|full]"
        echo "  smoke (default): 1k base, 几秒跑完"
        echo "  full:            SIFT-1M, 构图几分钟"
        exit 1
        ;;
esac
