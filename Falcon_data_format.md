tar -tzvf /实际路径/data.tar.gz | head -100

tar -xOf /实际路径/data.tar.gz "压缩包内/base文件路径" \
  | head -c 64 | xxd -g 1
