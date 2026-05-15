#!/bin/bash

# 定义 traffic type 名称数组
traffic_names=("Stream_Rd" "Stream_Wr" "Random_Rd" "Random_Wr" "Stream_Copy" "Stream_Add" "Random_Copy" "Random_Add")

# 定义 map 名称数组
map_names=("mrdimm_map5" "mrdimm_map5_no_refresh")
thread_num=1
trans_num=10240

for map_name in "${map_names[@]}"
do
    echo "------ Processing map: $map_name ------"
    # 并行运行不同的 traffic type (0-7)
    for i in {4..7}
    do
        echo "Starting traffic type $i: ${traffic_names[$i]} Analysis"

        output_subdir="${traffic_names[$i]}_t${thread_num}_n${trans_num}"
        log_file="Output_Dir/${map_name}/${output_subdir}/Statisic.log"
        if [ ! -f "$log_file" ]; then
            echo "No such log file: $log_file, skipping."
            continue
        fi

        # 并行执行 controller_test 并重定向输出
        python3 ../script/parse_statisic.py "$log_file"

        echo "Traffic type $i: ${traffic_names[$i]} Get Result"
    done
done