#!/bin/bash

# LPDDR5 频率比测试脚本 - 测试所有三种频率比配置
# 测试 1:1, 1:2, 1:4 三种频率比配置下的 AC Timing 约束

echo "========================================"
echo "LPDDR5 频率比测试 - 完整测试套件"
echo "========================================"
echo ""

# 设置颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
NC='\033[0m' # No Color

# 检查可执行文件是否存在
if [ ! -f "build/bin/dmutest" ]; then
    echo -e "${RED}错误: build/bin/dmutest 不存在${NC}"
    echo "请先编译项目: cd build && make -j8"
    exit 1
fi

# 检查配置文件是否存在
RESOURCE_DIR="lib/DRAMsys/configs"

declare -a configs=(
    "lpddr5-example.json:1:1 (基准)"
    "lpddr5-1to2-example.json:1:2 (半频)"
    "lpddr5-1to4-example.json:1:4 (四分频)"
)

for config_info in "${configs[@]}"; do
    IFS=':' read -r config_file config_name <<< "$config_info"
    if [ ! -f "$RESOURCE_DIR/$config_file" ]; then
        echo -e "${RED}错误: 找不到 $config_file${NC}"
        exit 1
    fi
done

echo -e "${GREEN}所有配置文件检查通过${NC}"
echo ""

# 运行测试计数器
total_tests=0
passed_tests=0
failed_tests=0

# 测试结果数组
declare -a test_results=()

echo "========================================"
echo "开始运行频率比测试"
echo "========================================"
echo ""

# 运行每个配置的测试
for config_info in "${configs[@]}"; do
    IFS=':' read -r config_file config_name <<< "$config_info"
    
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}测试配置: $config_name${NC}"
    echo -e "${BLUE}配置文件: $config_file${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""
    
    total_tests=$((total_tests + 1))
    
    # 运行测试（在build目录下运行，确保临时文件生成在build目录）
    cd build
    ./bin/dmutest FREQ_RATIO_TEST "../$RESOURCE_DIR/$config_file" "../$RESOURCE_DIR"
    test_result=$?
    cd ..
    test_result=$?
    cd ..
    
    # 检查测试结果
    if [ $test_result -eq 0 ]; then
        echo -e "${GREEN}✅ $config_name 测试通过${NC}"
        passed_tests=$((passed_tests + 1))
        test_results+=("$config_name:PASS")
    else
        echo -e "${RED}❌ $config_name 测试失败${NC}"
        failed_tests=$((failed_tests + 1))
        test_results+=("$config_name:FAIL")
    fi
    
    echo ""
done

# 打印测试总结
echo "========================================"
echo "测试总结"
echo "========================================"
echo ""

echo "测试结果:"
for result in "${test_results[@]}"; do
    IFS=':' read -r name status <<< "$result"
    if [ "$status" = "PASS" ]; then
        echo -e "  ${GREEN}✅ $name${NC}"
    else
        echo -e "  ${RED}❌ $name${NC}"
    fi
done

echo ""
echo "统计:"
echo "  总测试数: $total_tests"
echo "  通过: $passed_tests"
echo "  失败: $failed_tests"
echo ""

if [ $failed_tests -eq 0 ]; then
    echo -e "${GREEN}========================================"
    echo -e "✅ 所有频率比配置测试全部通过！"
    echo -e "========================================${NC}"
    exit 0
else
    echo -e "${RED}========================================"
    echo -e "❌ 部分频率比配置测试失败！"
    echo -e "========================================${NC}"
    exit 1
fi
