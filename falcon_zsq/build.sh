#!/bin/bash
# build.sh - A 机器 (编译机) 用: 编译测试二进制 + 打包 runfiles
#
# 前提: A 机器有 falcon 源码 + Opencode_done (平级目录)
#   某根目录/
#   ├── falcon/                          # falcon 源码 (含 WORKSPACE)
#   └── Opencode_done/
#       └── linux_falcon_zsq/            # 本目录
#
# 用法:
#   cd Opencode_done/linux_falcon_zsq
#   ./build.sh              # 编译 + 打包 -> dist/linux_falcon_zsq_test.tar.gz
#   ./build.sh --run-smoke  # 编译后直接在 A 机器跑 smoke (验证编译成功)
#
# 产物: dist/linux_falcon_zsq_test.tar.gz
#   拷到 B 机器, 解压后用 run.sh 运行

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# falcon 在本目录往上两级再进 falcon
FALCON_DIR="$(cd "$SCRIPT_DIR/../../falcon" && pwd)"

if [ ! -f "$FALCON_DIR/WORKSPACE" ]; then
    echo "ERROR: falcon WORKSPACE not found at $FALCON_DIR"
    echo "       expected: <root>/falcon/WORKSPACE"
    echo "       current:  <root>/Opencode_done/linux_falcon_zsq/build.sh"
    exit 1
fi

# 1. 在 falcon 树内创建符号链接 (不修改 falcon 源码)
LINK_PATH="$FALCON_DIR/linux_falcon_zsq"
if [ ! -L "$LINK_PATH" ]; then
    if [ -e "$LINK_PATH" ]; then
        echo "ERROR: $LINK_PATH exists but is not a symlink. remove it first."
        exit 1
    fi
    ln -s "$SCRIPT_DIR" "$LINK_PATH"
    echo "[setup] created symlink: $LINK_PATH -> $SCRIPT_DIR"
fi

# 2. 进入 falcon 根目录 (bazel 必须在 WORKSPACE 根执行)
cd "$FALCON_DIR"

# 3. 用 falcon 自带 bazel 二进制
BAZEL_BINARY="devel/builder/bazel-6.5.0-linux-arm64"
if [ ! -x "$BAZEL_BINARY" ]; then
    echo "WARN: falcon bazel not found at $BAZEL_BINARY, fallback to system bazel"
    BAZEL_BINARY="bazel"
fi

# 4. 编译 (bazel build, 不是 test, 只编译不运行)
echo "[build] compiling linux_falcon_zsq_test (this may take a few minutes)..."
$BAZEL_BINARY build --config=linux_arm64 \
    //linux_falcon_zsq:linux_falcon_zsq_test

# 5. 定位产物 (bazel-bin 是符号链接, 实际在 bazel 输出树)
BAZEL_BIN_DIR="$FALCON_DIR/bazel-bin/linux_falcon_zsq"
if [ ! -d "$BAZEL_BIN_DIR" ]; then
    # bazel 6.x 可能放在不同位置, 兜底用 bazel info
    BAZEL_BIN_DIR="$($BAZEL_BINARY info bazel-bin)/linux_falcon_zsq"
fi

BIN_PATH="$BAZEL_BIN_DIR/linux_falcon_zsq_test"
RUNFILES_PATH="$BAZEL_BIN_DIR/linux_falcon_zsq_test.runfiles"

if [ ! -x "$BIN_PATH" ] || [ ! -d "$RUNFILES_PATH" ]; then
    echo "ERROR: build output not found:"
    echo "  binary:  $BIN_PATH ($(ls -la "$BIN_PATH" 2>/dev/null))"
    echo "  runfiles: $RUNFILES_PATH ($(ls -d "$RUNFILES_PATH" 2>/dev/null))"
    exit 1
fi

echo "[build] binary: $BIN_PATH"
echo "[build] runfiles: $RUNFILES_PATH"

# 6. 检查动态库依赖 (B 机器需相同的 glibc/libstdc++ 等)
echo "[build] dynamic library dependencies:"
ldd "$BIN_PATH" || echo "  (ldd failed, may be static)"

# 7. 打包二进制 + runfiles 为 tar.gz
DIST_DIR="$SCRIPT_DIR/dist"
mkdir -p "$DIST_DIR"
TAR_NAME="linux_falcon_zsq_test.tar.gz"
TAR_PATH="$DIST_DIR/$TAR_NAME"

cd "$BAZEL_BIN_DIR"
# 用 -h 跟随符号链接, 确保实际文件被打包
tar czf "$TAR_PATH" -h linux_falcon_zsq_test linux_falcon_zsq_test.runfiles

TAR_SIZE=$(du -h "$TAR_PATH" | cut -f1)
echo "[build] packaged: $TAR_PATH ($TAR_SIZE)"
echo ""
echo "[build] DONE. Copy $TAR_PATH to B machine, then:"
echo "  tar xzf $TAR_NAME"
echo "  cd linux_falcon_zsq_test"
echo "  ./run.sh smoke          # 1k base smoke test"
echo "  ./run.sh full /path/to/sift/dataset   # SIFT-1M full test"

# 8. 可选: 在 A 机器直接跑 smoke 验证编译成功
if [ "$1" == "--run-smoke" ]; then
    echo ""
    echo "[run] smoke test on A machine..."
    cd "$RUNFILES_PATH"
    RUNFILES_DIR="$RUNFILES_PATH" "$BIN_PATH" --gtest_filter=FalconZSQCompare.SmokeSIFT1k
fi
