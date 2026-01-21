# LPDDR5 频率比测试指南

## 概述

本测试验证 LPDDR5 AC Timing Checker 在不同控制器:DRAM 频率比下的正确性。

测试三种配置：
- **1:1** - 控制器和 DRAM 运行在相同频率（基准）
- **1:2** - 控制器运行在 DRAM 频率的一半
- **1:4** - 控制器运行在 DRAM 频率的四分之一

## 测试配置

### 1:1 频率比（基准）
- **配置文件**: `lib/DRAMsys/configs/lpddr5-example.json`
- **MemSpec**: `JEDEC_LPDDR5-6400.json`
- **DRAM 频率**: 1600 MHz (tCK = 0.625 ns)
- **控制器频率**: 1600 MHz (tCK = 0.625 ns)
- **频率比**: 1:1

### 1:2 频率比
- **配置文件**: `lib/DRAMsys/configs/lpddr5-1to2-example.json`
- **MemSpec**: `JEDEC_LPDDR5-6400_1to2.json`
- **DRAM 频率**: 1600 MHz (tCK = 0.625 ns)
- **控制器频率**: 800 MHz (tCK = 1.25 ns)
- **频率比**: 1:2
- **controllerClockRatio**: 2

### 1:4 频率比
- **配置文件**: `lib/DRAMsys/configs/lpddr5-1to4-example.json`
- **MemSpec**: `JEDEC_LPDDR5-6400_1to4.json`
- **DRAM 频率**: 1600 MHz (tCK = 0.625 ns)
- **控制器频率**: 400 MHz (tCK = 2.5 ns)
- **频率比**: 1:4
- **controllerClockRatio**: 4

## 测试内容

每个频率比配置都会运行完整的 AC Timing 测试套件，包括：

### 1. ACT 命令时序约束
- tRC (同 Bank ACT 到 ACT)
- tRRD (不同 Bank ACT 到 ACT)
- tRPpb (PREPB 到 ACT)
- tRPab (PREAB 到 ACT)
- tRFCab (REFAB 到 ACT)
- tRFCpb (REFPB 到 ACT)
- tFAW (四激活窗口)

### 2. RD 命令时序约束
- tRCD (ACT 到 RD)
- tCCD (RD 到 RD, 16 Bank 模式)
- tCCD_L (RD 到 RD, 同 Bank Group)
- tCCD_S (RD 到 RD, 不同 Bank Group)
- tWTR (WR 到 RD, 16 Bank 模式)
- tWTR_L (WR 到 RD, 同 Bank Group)
- tWTR_S (WR 到 RD, 不同 Bank Group)

### 3. WR 命令时序约束
- tRCD (ACT 到 WR)
- tCCD (WR 到 WR, 16 Bank 模式)
- tCCD_L (WR 到 WR, 同 Bank Group)
- tCCD_S (WR 到 WR, 不同 Bank Group)
- tRDWR (RD 到 WR)

### 4. PRE 命令时序约束
- tRAS (ACT 到 PRE)
- tRTP (RD 到 PRE)
- tWRPRE (WR 到 PRE)

### 5. REF 命令时序约束
- tRFCab (REFAB 到 REFAB)
- tRFCpb (REFPB 到 REFPB, 同 Bank)
- tPBR2PBR (REFPB 到 REFPB, 不同 Bank)

### 6. Power Down 和 Self Refresh 命令
- PDEA/PDXA/PDEP/PDXP 时序约束
- SREFEN/SREFEX 时序约束

## 运行测试

### 方法 1: 使用测试脚本（推荐）

```bash
./run_freq_ratio_test.sh
```

### 方法 2: 直接运行

```bash
./build/bin/dmutest FREQ_RATIO_TEST
```

或使用数字：

```bash
./build/bin/dmutest 7
```

### 方法 3: 单独测试每个频率比

```bash
# 测试 1:1 频率比
./build/bin/dmutest AC_TIMING_TEST lpddr5-example.json

# 测试 1:2 频率比
./build/bin/dmutest AC_TIMING_TEST lpddr5-1to2-example.json

# 测试 1:4 频率比
./build/bin/dmutest AC_TIMING_TEST lpddr5-1to4-example.json
```

## 预期输出

测试会输出一个表格，显示每个频率比配置的测试结果：

```
========================================================================================================================
LPDDR5 频率比 AC Timing 测试
========================================================================================================================
频率比          比率      DRAM频率(MHz)     控制器频率(MHz)      DRAM tCK(ns)   控制器tCK(ns)     测试结果
------------------------------------------------------------------------------------------------------------------------
1:1 (基准)      1:1       1600.00           1600.00              0.625          0.625             ✅ PASS (100/100)
1:2 (半频)      1:2       1600.00           800.00               0.625          1.250             ✅ PASS (100/100)
1:4 (四分频)    1:4       1600.00           400.00               0.625          2.500             ✅ PASS (100/100)
========================================================================================================================

测试总结:
  配置测试: 3/3 通过
  总测试数: 300/300 通过

✅ 所有频率比配置的AC Timing测试全部通过！
========================================================================================================================
```

## 验证原理

### 时序约束转换示例

以 tRC = 60 nCK (37.5 ns) 为例：

#### 1:1 频率比
```
DRAM tCK = 0.625 ns
Controller tCK = 0.625 ns
DRAM cycles = 60
Controller cycles = ceil(60 / 1) = 60
实际等待时间 = 60 × 0.625 ns = 37.5 ns ✅
```

#### 1:2 频率比
```
DRAM tCK = 0.625 ns
Controller tCK = 1.25 ns
DRAM cycles = 60
Controller cycles = ceil(60 / 2) = 30
实际等待时间 = 30 × 1.25 ns = 37.5 ns ✅
```

#### 1:4 频率比
```
DRAM tCK = 0.625 ns
Controller tCK = 2.5 ns
DRAM cycles = 60
Controller cycles = ceil(60 / 4) = 15
实际等待时间 = 15 × 2.5 ns = 37.5 ns ✅
```

### 关键验证点

1. **时钟周期整数倍对齐**: 所有约束都向上取整到整数个控制器周期
2. **时序约束满足**: 实际等待时间 ≥ DRAM 要求的最小时间
3. **频率比正确应用**: 控制器周期 = DRAM 周期 × 频率比
4. **所有命令类型**: 验证 ACT, RD, WR, PRE, REF 等所有命令

## 故障排查

### 编译错误

如果遇到编译错误：

```bash
cd build
make clean
cmake ..
make -j8
```

### 配置文件缺失

确保以下文件存在：
- `lib/DRAMsys/configs/lpddr5-example.json`
- `lib/DRAMsys/configs/lpddr5-1to2-example.json`
- `lib/DRAMsys/configs/lpddr5-1to4-example.json`
- `lib/DRAMsys/configs/memspec/JEDEC_LPDDR5-6400.json`
- `lib/DRAMsys/configs/memspec/JEDEC_LPDDR5-6400_1to2.json`
- `lib/DRAMsys/configs/memspec/JEDEC_LPDDR5-6400_1to4.json`

### 测试失败

如果测试失败，检查：

1. **频率比配置**: 确认 JSON 文件中的 `controllerClockRatio` 正确
2. **时序参数**: 确认所有 LPDDR5 时序参数已正确配置
3. **转换逻辑**: 检查 `CheckerLPDDR5::convertToControllerTime()` 函数
4. **控制器时钟**: 确认 `Controller::controllerMethod()` 使用正确的时钟

## 性能分析

测试还可以用于分析不同频率比对性能的影响：

- **1:1**: 最高性能，最低延迟
- **1:2**: 中等性能，控制器功耗降低
- **1:4**: 最低性能，最低功耗

量化误差：
- **1:1**: 无量化误差
- **1:2**: 最多 1 个控制器周期 (1.25 ns)
- **1:4**: 最多 1 个控制器周期 (2.5 ns)

## 总结

本测试全面验证了 LPDDR5 AC Timing Checker 在不同频率比下的正确性，确保：

✅ 所有 DRAM 时序约束得到满足
✅ 控制器可以运行在较低频率以节省功耗
✅ 时钟域转换逻辑正确
✅ 向上取整机制保证安全性
