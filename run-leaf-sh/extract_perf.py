#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
extract_perf.py - 从 perf 日志中提取性能指标并生成两张 CSV

输出（与 .log 文件放在同一文件夹下）:
  Res_r.csv - 每个 .log 文件一行（原始数据），含 Run/Round/Thread 列
  Res_s.csv - 按 thread 聚合（方式 B 分段）:
              上段: 每个 thread 一行均值（mean）
              下段: 每个 thread 一行 std / min / max / count

用法:
  python extract_perf.py [LOG_DIR]
  - 不传参数: 使用脚本中 LOG_DIR 默认值
  - 传参数:   使用命令行传入的 log 文件夹路径

仅使用 Python 标准库，无需安装任何第三方包。
"""

import os
import re
import sys
import csv
import glob
import statistics

# ============ 配置区（按需修改）============
# 存放所有 .log 文件的文件夹路径。可改为你的实际路径，例如:
#   LOG_DIR = r"E:\Graduation\GPTproject\FS\falcon\TestResultDownLoad"
# 也可在运行时通过命令行参数覆盖: python extract_perf.py <文件夹路径>
LOG_DIR = r"."
# =========================================

# CSV 列定义（顺序即输出顺序）
METRIC_COLS = [
    'SucQPS', 'P99', 'P95', 'P90', 'P80', 'P70', 'P50',
    'Total Req', 'Suc Num', 'Fail Num',
    'Avg retrieve num', 'Avg result num',
    'no doc return num', 'score num',
]

# 各字段的值类型
COL_TYPE = {
    'SucQPS': 'float',
    'P99': 'float', 'P95': 'float', 'P90': 'float',
    'P80': 'float', 'P70': 'float', 'P50': 'float',
    'Total Req': 'int', 'Suc Num': 'int', 'Fail Num': 'int',
    'Avg retrieve num': 'float', 'Avg result num': 'float',
    'no doc return num': 'int', 'score num': 'int',
}

# 文件名格式: <run>_round<round>_thread<thread>.log
FILENAME_RE = re.compile(r'^(\d+)_round(\d+)_thread(\d+)\.log$')

# 各字段对应的 perf.cpp 行号正则（在文件内容中搜索，不按行）
PATTERNS = {
    'thread_line': re.compile(
        r'perf\.cpp:251\]\s*thread:\s*(\d+)\s+total_req:\s*(\d+)\s+suc_num:\s*(\d+)\s+fail_num:\s*(\d+)'
    ),
    'SucQPS':              re.compile(r'perf\.cpp:253\].*?suc_qps:\s*([\d.]+)'),
    'Avg retrieve num':    re.compile(r'perf\.cpp:254\].*?avg\s+retrieve_num:\s*([\d.]+)'),
    'Avg result num':      re.compile(r'perf\.cpp:256\].*?avg\s+result_num:\s*([\d.]+)'),
    'P99':                 re.compile(r'perf\.cpp:265\].*?p99:\s*([\d.]+)'),
    'P95':                 re.compile(r'perf\.cpp:268\].*?p95:\s*([\d.]+)'),
    'P90':                 re.compile(r'perf\.cpp:271\].*?p90:\s*([\d.]+)'),
    'P80':                 re.compile(r'perf\.cpp:274\].*?p80:\s*([\d.]+)'),
    'P70':                 re.compile(r'perf\.cpp:277\].*?p70:\s*([\d.]+)'),
    'P50':                 re.compile(r'perf\.cpp:280\].*?p50:\s*([\d.]+)'),
    'no doc return num':   re.compile(r'perf\.cpp:295\].*?no doc return num:\s*(\d+)'),
    'score num':           re.compile(r'perf\.cpp:296\].*?scored num:\s*(\d+)'),
}

# 读取文件末尾的字节数（perf 摘要段约 3KB，留足余量）
TAIL_BYTES = 16384


def convert(val, col):
    """把正则捕获到的字符串转为对应类型；空则返回 'miss'。"""
    if val is None:
        return 'miss'
    try:
        return float(val) if COL_TYPE[col] == 'float' else int(val)
    except (ValueError, TypeError):
        return 'miss'


def parse_log(filepath):
    """解析单个 log 文件，返回结果 dict；文件名不匹配返回 None。"""
    fname = os.path.basename(filepath)
    m = FILENAME_RE.match(fname)
    if not m:
        return None
    run, rnd, thr = m.group(1), m.group(2), int(m.group(3))

    # 只读末尾若干字节即可覆盖所有摘要指标
    try:
        with open(filepath, 'rb') as fb:
            fb.seek(0, 2)
            size = fb.tell()
            fb.seek(max(0, size - TAIL_BYTES), 0)
            chunk = fb.read().decode('utf-8', errors='replace')
    except Exception as e:
        print(f"  [错误] 读取 {fname} 失败: {e}")
        return None

    result = {'Run': run, 'Round': rnd, 'Thread': thr}

    # perf.cpp:251 行: thread / total_req / suc_num / fail_num
    m251 = PATTERNS['thread_line'].search(chunk)
    if m251:
        log_thread = int(m251.group(1))
        if log_thread != thr:
            print(f"  [警告] {fname}: 文件名 thread={thr} 与日志 thread={log_thread} 不一致，使用日志值")
            result['Thread'] = log_thread
        result['Total Req'] = convert(m251.group(2), 'Total Req')
        result['Suc Num']   = convert(m251.group(3), 'Suc Num')
        result['Fail Num']  = convert(m251.group(4), 'Fail Num')
    else:
        result['Total Req'] = result['Suc Num'] = result['Fail Num'] = 'miss'

    for col in ['SucQPS', 'Avg retrieve num', 'Avg result num',
                'P99', 'P95', 'P90', 'P80', 'P70', 'P50',
                'no doc return num', 'score num']:
        mobj = PATTERNS[col].search(chunk)
        result[col] = convert(mobj.group(1) if mobj else None, col)

    return result


def is_number(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool)


def stats_of(vals):
    """对一组值计算 (mean, std, min, max, count)；非数值忽略。"""
    nums = [v for v in vals if is_number(v)]
    if not nums:
        return 'miss', 'miss', 'miss', 'miss', 0
    mean_v = round(statistics.mean(nums), 4)
    std_v = round(statistics.stdev(nums), 4) if len(nums) >= 2 else 'miss'
    return mean_v, std_v, min(nums), max(nums), len(nums)


def main():
    log_dir = sys.argv[1] if len(sys.argv) > 1 else LOG_DIR
    if not os.path.isdir(log_dir):
        print(f"错误: 文件夹不存在 -> {log_dir}")
        print("用法: python extract_perf.py <log 文件夹路径>")
        sys.exit(1)

    files = [f for f in sorted(glob.glob(os.path.join(log_dir, '*.log')))
             if FILENAME_RE.match(os.path.basename(f))]

    if not files:
        print(f"在 {log_dir} 中没有找到 *_round*_thread*.log 文件")
        sys.exit(0)

    print(f"共找到 {len(files)} 个匹配的 log 文件，开始解析...\n")

    records = []
    for fp in files:
        r = parse_log(fp)
        if r is None:
            continue
        miss_cols = [c for c in METRIC_COLS if r[c] == 'miss']
        if miss_cols:
            print(f"  {os.path.basename(fp)}: 缺失 {', '.join(miss_cols)}")
        else:
            print(f"  {os.path.basename(fp)}: OK  Thread={r['Thread']} "
                  f"SucQPS={r['SucQPS']} P99={r['P99']}")
        records.append(r)

    if not records:
        print("\n没有可用的解析结果。")
        return

    # ---- 写 Res_r.csv ----
    r_csv = os.path.join(log_dir, 'Res_r.csv')
    header_r = ['Run', 'Round', 'Thread'] + METRIC_COLS
    records.sort(key=lambda x: (x['Thread'], int(x['Run']), int(x['Round'])))
    with open(r_csv, 'w', encoding='utf-8-sig', newline='') as f:
        w = csv.writer(f)
        w.writerow(header_r)
        for r in records:
            w.writerow([r['Run'], r['Round'], r['Thread']] + [r[c] for c in METRIC_COLS])
    print(f"\n[OK] Res_r.csv 已生成 -> {r_csv}  共 {len(records)} 行")

    # ---- 写 Res_s.csv（方式 B: 上段均值，下段 std/min/max，列数不同）----
    s_csv = os.path.join(log_dir, 'Res_s.csv')
    threads = sorted({r['Thread'] for r in records})

    # 预先算好每个 thread、每个指标的统计量
    cache = {}
    for t in threads:
        cache[t] = {}
        for col in METRIC_COLS:
            vals = [r[col] for r in records if r['Thread'] == t]
            cache[t][col] = stats_of(vals)  # (mean, std, min, max, count)

    # 上段表头: Thread + 各指标（均值）
    header_upper = ['Thread'] + METRIC_COLS
    # 下段表头: Thread + 各指标 std/min/max + 末尾 count
    header_lower = ['Thread']
    for col in METRIC_COLS:
        header_lower += [f'{col}_std', f'{col}_min', f'{col}_max']
    header_lower += ['count']

    with open(s_csv, 'w', encoding='utf-8-sig', newline='') as f:
        w = csv.writer(f)
        # 上段: 均值
        w.writerow(header_upper)
        for t in threads:
            w.writerow([t] + [cache[t][col][0] for col in METRIC_COLS])
        # 空行分隔
        w.writerow([])
        # 下段: 各指标 std / min / max，末尾 count
        w.writerow(header_lower)
        for t in threads:
            row = [t]
            for col in METRIC_COLS:
                _, std_v, min_v, max_v, _ = cache[t][col]
                row += [std_v, min_v, max_v]
            row.append(len([r for r in records if r['Thread'] == t]))
            w.writerow(row)
    print(f"[OK] Res_s.csv 已生成 -> {s_csv}  共 {len(threads)} 个 thread")


if __name__ == '__main__':
    main()
