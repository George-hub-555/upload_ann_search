## A 机器(编译机)

```bash
# 1. 把 linux_falcon_zsq + falcon 源码放到 A 机器(平级)
#    某根目录/falcon/  +  某根目录/Opencode_done/linux_falcon_zsq/

# 2. 编译
cd 某根目录/Opencode_done/linux_falcon_zsq
chmod +x build.sh
./build.sh

# 产物: 某根目录/Opencode_done/linux_falcon_zsq/dist/linux_falcon_zsq_test.tar.gz
# 可选验证: ./build.sh --run-smoke
```

## B 机器(测试机)

```bash
# 1. 把 A 机器的 dist/linux_falcon_zsq_test.tar.gz 拷到 B 机器
#    (full 模式还需拷 falcon/dataset/ 到 B 机器)

# 2. 解压
cd ~
tar xzf linux_falcon_zsq_test.tar.gz
cd linux_falcon_zsq_test
chmod +x run.sh

# 3. 运行
./run.sh smoke                       # smoke: 1k base, 数据在 tar 包里
./run.sh full /path/to/dataset       # full: SIFT-1M, 需指定数据目录(含 sift_*.fvecs)
./run.sh list                        # 列出所有测试 case
```

## 产物位置

| 阶段 | 产物 | 路径 |
|---|---|---|
| A 机器编译后 | tar 包 | `Opencode_done/linux_falcon_zsq/dist/linux_falcon_zsq_test.tar.gz` |
| B 机器解压后 | 二进制 + runfiles + run.sh | `~/linux_falcon_zsq_test/` 目录下 |

## 拷贝清单

```bash
# A -> B (smoke 只需 tar 包)
scp A:Opencode_done/linux_falcon_zsq/dist/linux_falcon_zsq_test.tar.gz B:~/

# A -> B (full 还需 SIFT-1M 数据, 512MB)
scp -r A:falcon/dataset B:~/
# 然后 B 机器: ./run.sh full ~/dataset
```
