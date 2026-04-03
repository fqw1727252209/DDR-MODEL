#!/bin/bash

# 定义 traffic type 名称数组
traffic_names=("Stream_Rd" "Stream_Wr" "Random_Rd" "Random_Wr" "Stream_Copy" "Stream_Add" "Random_Copy" "Random_Add")



# 并行运行不同的 traffic type (0-7)
for i in {2..7}
do
    echo "Starting traffic type $i: ${traffic_names[$i]} Analysis"

    # 并行执行 controller_test 并重定向输出
    python3 calculate.py Output_dir/no3ds_map5/${traffic_names[$i]}/TransInfo.txt

    echo "Traffic type $i: ${traffic_names[$i]} Get Result"
done