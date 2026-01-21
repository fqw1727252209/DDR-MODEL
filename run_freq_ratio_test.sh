#!/bin/bash

# LPDDR5 频率比测试脚本
# 测试 1:1, 1:2, 1:4 三种频率比配置下的 AC Timing 约束

echo "========================================"
echo "LPDDR5 频率比测试"
echo "========================================"
echo ""

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 检查可执行文件是否存在
if [ ! -f "build/bin/dmutest" ]; then
    echo -e "${RED}错误: build/bin/dmutest 不存在${NC}"
    echo "请先编译项目: cd build && make -j8"
    exit 1
fi

# 检查配置文件是否存在
RESOURCE_DIR="lib/DRAMsys/configs"

if [ ! -f "$RESOURCE_DIR/lpddr5-example.json" ]; then
    echo -e "${RED}错误: 找不到 lpddr5-example.json${NC}"
    exit 1
fi

if [ ! -f "$RESOURCE_DIR/lpddr5-1to2-example.json" ]; then
    echo -e "${RED}错误: 找不到 lpddr5-1to2-example.json${NC}"
    exit 1
fi

if [ ! -f "$RESOURCE_DIR/lpddr5-1to4-example.json" ]; then
    echo -e "${RED}错误: 找不到 lpddr5-1to4-example.json${NC}"
    exit 1
fi

echo -e "${GREEN}所有配置文件检查通过${NC}"
echo ""

# 运行频率比测试（在build目录下运行）
echo "开始运行频率比测试..."
echo ""

cd build
./bin/dmutest FREQ_RATIO_TEST
cd ..

# 检查测试结果
if [ $? -eq 0 ]; then
    echo ""
    echo -e "${GREEN}========================================"
    echo -e "✅ 频率比测试全部通过！"
    echo -e "========================================${NC}"
    exit 0
else
    echo ""
    echo -e "${RED}========================================"
    echo -e "❌ 频率比测试失败！"
    echo -e "========================================${NC}"
    exit 1
fi
