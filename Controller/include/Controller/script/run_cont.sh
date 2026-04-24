#!/bin/bash

# 创建输出目录
mkdir -p Output_dir/3ds_map2
mkdir -p Output_dir/no3ds_map1

# 定义 traffic type 名称数组
traffic_names=("Stream_Rd" "Stream_Wr" "Random_Rd" "Random_Wr" "Stream_Copy" "Stream_Add" "Random_Copy" "Random_Add")

# 检查 controller_test 是否存在
if [ ! -f "./controller_test" ]; then
    echo "Error: ./controller_test executable not found!"
    exit 1
fi

# 并行运行不同的 traffic type (0-7)
for i in {0..1}
do
    echo "Starting traffic type $i: ${traffic_names[$i]}"

    mkdir -p Output_dir/3ds_map2/${traffic_names[$i]}
    mkdir -p Output_dir/no3ds_map1/${traffic_names[$i]}

    # 并行执行 controller_test 并重定向输出
    ./controller_test ../config 3ds_map2.json $i 2>&1 >Output_dir/3ds_map2/${traffic_names[$i]}/log &
    ./controller_test ../config no3ds_map1.json $i 2>&1 >Output_dir/no3ds_map1/${traffic_names[$i]}/log &

    echo "Traffic type $i: ${traffic_names[$i]} started in background"
done

echo "All traffic types started in parallel. Waiting for completion..."

# 等待所有后台任务完成
wait

echo "All traffic types completed."