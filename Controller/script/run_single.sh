#!/bin/bash

traffic_names=("Stream_Rd" "Stream_Wr" "Random_Rd" "Random_Wr" "Stream_Copy" "Stream_Add" "Random_Copy" "Random_Add")

traffic_type=7
# no3ds_map5 3ds_map7 3ds_map2 no3ds_map1
Type=3ds_map7

echo Output_Dir/${Type}/${traffic_names[$traffic_type]}
Result_Dir=Output_Dir/${Type}/${traffic_names[$traffic_type]}
mkdir -p ${Result_Dir}


./controller_test ../config ${Type}.json ${traffic_type} >${Result_Dir}/log 2>&1
# python3 calculate.py Output_Dir/${Type}/${traffic_names[$traffic_type]}/TransInfo.txt

# ./controller_test ../config $no3ds_map5.json 4 >Output_Dir/no3ds_map5/Stream_Copy/log 2>&1