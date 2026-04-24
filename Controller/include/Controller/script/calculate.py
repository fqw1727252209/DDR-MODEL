# ======== 统计 ========
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Transaction Data Phase Analyzer
Author: ChatGPT (GPT-5)
Description:
    Parse transaction log and compute only Data Phase relationships
    (overlap and gap) between adjacent transactions.
"""

import re
import pandas as pd
import numpy as np
import sys
import os

# ======== 参数配置 ========
# 配置输入输出文件路径
# 配置输入输出文件路径
if len(sys.argv) < 2:
    print("Usage: python calculate.py <input_file>")
    print("Example: python calculate.py TransInfo.txt")
    sys.exit(1)

input_file = sys.argv[1]
# 获取输入文件的目录和文件名
input_dir = os.path.dirname(input_file)
# 在同一目录下生成输出文件
output_csv = os.path.join(input_dir, "transactions.csv") if input_dir else "transactions.csv"

# ======== 日志解析 ========
# 定义用于解析日志文件的正则表达式模式
block_pattern = re.compile(r"===== Transaction Statistics =====(.*?)=================================", re.S)
id_pattern = re.compile(r"Transaction ID:\s*(\d+)")
data_begin_pattern = re.compile(r"Data Begin Time:\s*(\d+)\s*ps")
data_end_pattern = re.compile(r"Data End Time:\s*(\d+)\s*ps")

# 存储解析后的事务数据
transactions = []

# 读取并解析输入文件
with open(input_file, "r") as f:
    text = f.read()

# 提取所有事务块
blocks = block_pattern.findall(text)

# 遍历每个事务块并提取关键信息
for block in blocks:
    tid_match = id_pattern.search(block)
    begin_match = data_begin_pattern.search(block)
    end_match = data_end_pattern.search(block)

    if not tid_match:
        continue

    tid = int(tid_match.group(1))
    data_begin = int(begin_match.group(1)) if begin_match else None
    data_end = int(end_match.group(1)) if end_match else None

    # 计算数据阶段延迟
    data_latency = (data_end - data_begin) if (data_begin and data_end) else None

    transactions.append({
        "TransactionID": tid,
        "Data_Begin(ps)": data_begin,
        "Data_End(ps)": data_end,
        "DataLatency(ps)": data_latency
    })

# ======== 转换为 DataFrame ========
# 将事务数据转换为pandas DataFrame格式
df = pd.DataFrame(transactions)
# df = df.sort_values(by="TransactionID").reset_index(drop=True)

# ======== 计算相邻事务的间隔 (gap) ========
# 计算相邻事务间的时间间隔和重叠情况
df["Next_Data_Begin(ps)"] = df["Data_Begin(ps)"].shift(-1)
df["Data_Gap(ps)"] = df["Next_Data_Begin(ps)"] - df["Data_End(ps)"]

# overlap = 负值
# 标记是否存在重叠（负间隔表示重叠）
df["Overlap?"] = df["Data_Gap(ps)"] < 0

# ======== 保存 CSV ========
# 将处理后的数据保存到csv文件
df.to_csv(output_csv, index=False)
print(f"✅ Parsed {len(df)} transactions -> {output_csv}")


# ======== 统计 ========
# 计算统计数据并输出结果摘要
valid_gaps = df["Data_Gap(ps)"].dropna()

avg_gap = valid_gaps.mean()
overlap_count = (valid_gaps < 0).sum()
positive_gap_count = (valid_gaps > 0).sum()
total_pairs = len(valid_gaps)

overlap_ratio = overlap_count / total_pairs * 100 if total_pairs else 0
positive_gap_ratio = positive_gap_count / total_pairs * 100 if total_pairs else 0


# 添加计算DataBegin最小值，DataEnd最大值和相关统计
min_data_begin = df["Data_Begin(ps)"].min()
max_data_end = df["Data_End(ps)"].max()
total_time = max_data_end - min_data_begin

# 计算IDLE时间 (所有正的Data_Gap之和)
idle_time = valid_gaps[valid_gaps > 0].sum()

# 计算有效数据传输时间 (总时间 - IDLE时间)
effective_data_time = total_time - idle_time

# 计算Data传输时间占总时间的比例
data_utilization_ratio = effective_data_time / total_time * 100 if total_time > 0 else 0


print("\n📊 Data Phase Relationship Summary:")
print(f"  Total Transactions         : {len(df)}")
print(f"  Avg Data Latency (ps)      : {df['DataLatency(ps)'].mean():.1f}")
print(f"  Avg Data Gap (ps)          : {avg_gap:.1f}")
print(f"  Overlap Count              : {overlap_count}")
print(f"  Overlap Ratio (%)          : {overlap_ratio:.2f}")
print(f"  Positive Gap Count         : {positive_gap_count}")
print(f"  Positive Gap Ratio (%)     : {positive_gap_ratio:.2f}")
valid_gaps = df["Data_Gap"].dropna()

# 输出新增的统计信息
print(f"\n⏱️ Data Transmission Statistics:")
print(f"  Min Data Begin (ps)        : {min_data_begin}")
print(f"  Max Data End (ps)          : {max_data_end}")
print(f"  Total Time (ps)            : {total_time}")
print(f"  IDLE Time (ps)             : {idle_time}")
print(f"  Effective Data Time (ps)   : {effective_data_time}")
print(f"  Data Utilization Ratio (%) : {data_utilization_ratio:.2f}")