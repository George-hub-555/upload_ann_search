可以。先确认该文件是真正的 ARM64 Bazel，而不是 133 字节的 Git LFS 指针。

## 1. 检查文件

在 FalconSearch 仓库根目录执行：

```bash
ls -lh devel/builder/bazel-7.4.1-linux-arm64

file devel/builder/bazel-7.4.1-linux-arm64
```

正确结果应该接近：

```text
56M devel/builder/bazel-7.4.1-linux-arm64
ELF 64-bit LSB executable, ARM aarch64
```

还可以核对 SHA-256：

```bash
sha256sum devel/builder/bazel-7.4.1-linux-arm64
```

预期为：

```text
d7aedc8565ed47b6231badb80b09f034e389c5f2b1c2ac2c55406f7c661d8b88
```

如果文件只有 133 字节并显示：

```text
version https://git-lfs.github.com/spec/v1
```

暂时不能安装，需要先取得完整的约 56 MB 文件。

## 2. 安装到当前用户

不需要 `sudo`：

```bash
mkdir -p "$HOME/.local/bin"

install -m 0755 \
  devel/builder/bazel-7.4.1-linux-arm64 \
  "$HOME/.local/bin/bazel"
```

将目录加入 `PATH`：

```bash
grep -qxF 'export PATH="$HOME/.local/bin:$PATH"' "$HOME/.bashrc" ||
  echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$HOME/.bashrc"
```

让配置立即生效：

```bash
source "$HOME/.bashrc"
```

验证：

```bash
command -v bazel
bazel --version
```

预期：

```text
/home/tysearch/.local/bin/bazel
bazel 7.4.1
```

## 3. 如果不想修改 PATH

也可以不安装，直接给仓库文件执行权限：

```bash
chmod +x devel/builder/bazel-7.4.1-linux-arm64
```

然后始终这样调用：

```bash
./devel/builder/bazel-7.4.1-linux-arm64 --version
```

## 4. 系统级安装

如果有 `sudo` 并希望所有用户使用：

```bash
sudo install -m 0755 \
  devel/builder/bazel-7.4.1-linux-arm64 \
  /usr/local/bin/bazel
```

验证：

```bash
/usr/local/bin/bazel --version
bazel --version
```

## 5. 安装后构建 checker

```bash
bazel build \
  --config=linux_arm64 \
  --copt=-march=armv8-a+fp+simd+crypto+crc \
  --cxxopt=-march=armv8-a+fp+simd+crypto+crc \
  //tools/index_factory:blink_graph_index_checker
```

成功后的程序位于：

```text
bazel-bin/tools/index_factory/blink_graph_index_checker
```
