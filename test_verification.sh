#!/bin/bash

# LPDDR5 AC Timing Checker 验证脚本
# 用于验证三种频率比配置的正确性

echo "========================================"
echo "LPDDR5 AC Timing Checker 验证测试"
echo "========================================"
echo ""

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[1;34m'
NC='\033[0m' # No Color

# 检查可执行文件
if [ ! -f "build/bin/dmutest" ]; then
    echo -e "${RED}错误: build/bin/dmutest 不存在${NC}"
    echo "请先编译项目"
    exit 1
fi

# 配置文件路径
RESOURCE_DIR="lib/DRAMsys/configs"

# 测试配置数组
declare -a configs=(
    "lpddr5-example.json:1:1 (基准配置)"
    "lpddr5-1to2-example.json:1:2 (半频配置)"
    "lpddr5-1to4-example.json:1:4 (四分频配置)"
)

# 测试计数器
total_tests=0
passed_tests=0

echo -e "${BLUE}开始运行验证测试...${NC}"
echo ""

# 运行每个配置的测试
for config_info in "${configs[@]}"; do
    IFS=':' read -r config_file config_name <<< "$config_info"
    
    echo "========================================"
    echo "测试配置: $config_name"
    echo "配置文件: $config_file"
    echo "========================================"
    
    total_tests=$((total_tests + 1))
    
    # 运行测试（在build目录下运行，确保临时文件生成在build目录）
    cd build
    ./bin/dmutest FREQ_RATIO_TEST "../$RESOURCE_DIR/$config_file" "../$RESOURCE_DIR" 2>&1 | tee "../logs/test_${config_file%.json}.log"
    cd ..
    
    # 检查测试结果
    if [ ${PIPESTATUS[0]} -eq 0 ]; then
        echo -e "${GREEN}✅ $config_name 测试通过${NC}"
        passed_tests=$((passed_tests + 1))
    else
        echo -e "${RED}❌ $config_name 测试失败${NC}"
    fi
    
    echo ""
done

# 打印总结
echo "========================================"
echo "测试总结"
echo "========================================"
echo "总测试数: $total_tests"
echo "通过: $passed_tests"
echo "失败: $((total_tests - passed_tests))"
echo ""

if [ $passed_tests -eq $total_tests ]; then
    echo -e "${GREEN}✅ 所有测试通过！${NC}"
    exit 0
else
    echo -e "${RED}❌ 部分测试失败！${NC}"
    exit 1
fi
