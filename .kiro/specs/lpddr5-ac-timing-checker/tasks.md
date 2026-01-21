# Implementation Plan: LPDDR5 AC Timing Checker

## Overview

本实现计划将LPDDR5 AC Timing Checker分解为可执行的编码任务。实现基于现有的LPDDR4 Checker架构，扩展支持LPDDR5特有的时序参数和Bank结构。

## Tasks

- [x] 1. 创建MemSpecLPDDR5类
  - [x] 1.1 创建MemSpecLPDDR5头文件
    - 在 `lib/DRAMsys/src/libdramsys/DRAMSys/configuration/memspec/` 目录下创建 `MemSpecLPDDR5.h`
    - 继承自MemSpec基类
    - 定义所有LPDDR5时序参数成员变量（tRCD, tRAS, tRPpb, tRPab, tRC, tRRD, tCCD, tCCD_L, tCCD_S, tWTR, tWTR_L, tWTR_S, tRTP, tWR, tFAW, tRFCab, tRFCpb, tPBR2PBR, tPBR2ACT, tDQSCK等）
    - 定义Bank结构常量（banksPerRank=16, bankGroupsPerRank=8, banksPerBankGroup=2）
    - 定义bankGroupMode配置参数
    - 声明getRefreshIntervalAB(), getRefreshIntervalPB(), getExecutionTime()等方法
    - _Requirements: 1.1, 1.2, 1.5_

  - [x] 1.2 实现MemSpecLPDDR5构造函数和方法
    - 在 `lib/DRAMsys/src/libdramsys/DRAMSys/configuration/memspec/` 目录下创建 `MemSpecLPDDR5.cpp`
    - 实现从Config::MemSpec加载时序参数的构造函数
    - 实现getRefreshIntervalAB()返回tREFI
    - 实现getRefreshIntervalPB()返回Per-Bank刷新间隔
    - 实现getExecutionTime()计算各命令执行时间
    - 实现getIntervalOnDataStrobe()计算数据选通间隔
    - _Requirements: 1.1, 1.3, 1.4_

  - [x] 1.3 编写MemSpecLPDDR5单元测试
    - 测试参数加载正确性
    - 测试默认值设置
    - 测试Bank结构配置
    - _Requirements: 1.1, 1.2, 1.4_

- [x] 2. 创建LPDDR5 JSON配置文件
  - [x] 2.1 创建LPDDR5 memspec JSON配置
    - 在 `lib/DRAMsys/configs/memspec/` 目录下创建 `JEDEC_LPDDR5-6400.json`
    - 包含LPDDR5-6400速度等级的时序参数
    - 包含bankGroupMode配置选项
    - _Requirements: 1.3_

  - [x] 2.2 创建LPDDR5示例配置文件
    - 在 `lib/DRAMsys/configs/` 目录下创建 `lpddr5-example.json`
    - 引用LPDDR5 memspec配置
    - 配置地址映射和控制器参数
    - _Requirements: 1.3_

- [x] 3. 创建CheckerLPDDR5类
  - [x] 3.1 创建CheckerLPDDR5头文件
    - 在 `lib/DRAMsys/src/libdramsys/DRAMSys/controller/checker/` 目录下创建 `CheckerLPDDR5.h`
    - 继承自CheckerIF接口
    - 声明timeToSatisfyConstraints()和insert()方法
    - 声明isBankGroupMode()辅助方法
    - 定义命令历史记录数据结构（lastScheduledByCommandAndBank, lastScheduledByCommandAndBankGroup等）
    - 定义tFAW窗口跟踪队列
    - 定义预计算的复合时序参数（tBURST, tRDWR, tWRRD等）
    - _Requirements: 7.1, 7.3, 7.4_

  - [x] 3.2 实现CheckerLPDDR5构造函数
    - 在 `lib/DRAMsys/src/libdramsys/DRAMSys/controller/checker/` 目录下创建 `CheckerLPDDR5.cpp`
    - 初始化MemSpecLPDDR5指针
    - 初始化命令历史记录向量
    - 预计算复合时序参数（tBURST, tBURST32, tRDWR, tWRRD, tRDPRE, tWRPRE等）
    - _Requirements: 7.1_

  - [x] 3.3 实现ACT命令时序约束检查
    - 实现timeToSatisfyConstraints对ACT命令的处理
    - 检查同Bank的tRC约束
    - 检查不同Bank的tRRD约束
    - 检查PREPB后的tRPpb约束
    - 检查PREAB后的tRPab约束
    - 检查REFAB后的tRFCab约束
    - 检查REFPB后的tRFCpb约束
    - 检查tFAW窗口约束
    - _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7_

  - [x] 3.4 编写ACT命令时序约束属性测试
    - **Property 1: ACT命令时序约束正确性**
    - **Validates: Requirements 2.1, 2.2, 2.3, 2.4, 2.5, 2.6, 2.7**
    - Implemented in `DMU/lp5_ac_timing_test.cpp` as `runProperty1_ACTTimingConstraints()`

  - [x] 3.5 实现RD命令时序约束检查
    - 实现timeToSatisfyConstraints对RD/RDA命令的处理
    - 检查ACT后的tRCD约束
    - 16 Bank模式：检查tCCD约束
    - 8 BG模式：检查同Bank Group的tCCD_L约束
    - 8 BG模式：检查不同Bank Group的tCCD_S约束
    - 16 Bank模式：检查WR后的tWRRD约束
    - 8 BG模式：检查同Bank Group的tWTR_L约束
    - 8 BG模式：检查不同Bank Group的tWTR_S约束
    - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7_

  - [x] 3.6 编写RD命令时序约束属性测试
    - **Property 2: RD命令时序约束正确性**
    - **Validates: Requirements 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7**
    - Implemented in `DMU/lp5_ac_timing_test.cpp` as `runProperty2_RDTimingConstraints()`

  - [x] 3.7 实现WR命令时序约束检查
    - 实现timeToSatisfyConstraints对WR/WRA/MWR/MWRA命令的处理
    - 检查ACT后的tRCD约束
    - 16 Bank模式：检查tCCD约束
    - 8 BG模式：检查同Bank Group的tCCD_L约束
    - 8 BG模式：检查不同Bank Group的tCCD_S约束
    - 检查RD后的tRDWR约束
    - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5_

  - [x] 3.8 编写WR命令时序约束属性测试
    - **Property 3: WR命令时序约束正确性**
    - **Validates: Requirements 4.1, 4.2, 4.3, 4.4, 4.5**
    - Implemented in `DMU/lp5_ac_timing_test.cpp` as `runProperty3_WRTimingConstraints()`

  - [x] 3.9 实现PRE命令时序约束检查
    - 实现timeToSatisfyConstraints对PREPB/PREAB命令的处理
    - 检查ACT后的tRAS约束
    - 检查RD后的tRTP约束
    - 检查WR后的tWRPRE约束
    - PREAB需检查所有Bank的约束
    - _Requirements: 5.1, 5.2, 5.3, 5.4_

  - [x] 3.10 编写PRE命令时序约束属性测试
    - **Property 4: PRE命令时序约束正确性**
    - **Validates: Requirements 5.1, 5.2, 5.3, 5.4**
    - Implemented in `DMU/lp5_ac_timing_test.cpp` as `runProperty4_PRETimingConstraints()`

  - [x] 3.11 实现REF命令时序约束检查
    - 实现timeToSatisfyConstraints对REFAB/REFPB命令的处理
    - REFAB：检查所有Bank已预充电（tRPab约束）
    - REFAB：检查连续REFAB的tRFCab约束
    - REFPB：检查目标Bank已预充电（tRPpb约束）
    - REFPB：检查同Bank连续REFPB的tRFCpb约束
    - REFPB：检查不同Bank连续REFPB的tPBR2PBR约束
    - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_

  - [x] 3.12 编写REF命令时序约束属性测试
    - **Property 5: REF命令时序约束正确性**
    - **Validates: Requirements 6.1, 6.2, 6.3, 6.4, 6.5, 6.6**
    - Implemented in `DMU/lp5_ac_timing_test.cpp` as `runProperty5_REFTimingConstraints()`

  - [x] 3.13 实现insert方法
    - 记录命令时间戳到相应的历史记录向量
    - 更新tFAW窗口队列（对于ACT命令）
    - 记录突发长度（对于RD/WR命令）
    - _Requirements: 7.3_

  - [x] 3.14 编写timeToSatisfyConstraints返回最大值属性测试
    - **Property 6: timeToSatisfyConstraints返回最大约束值**
    - **Validates: Requirements 7.2**
    - Implemented in `DMU/lp5_ac_timing_test.cpp` as `runProperty6_MaxConstraintValue()`

- [x] 4. Checkpoint - 确保核心功能测试通过
  - 确保所有测试通过，如有问题请询问用户
  - ✅ 6个Property-Based Tests全部通过 (每个100次迭代)
  - ✅ 27个Specific Constraint Tests全部通过
  - 修复: REFAB->ACT测试期望值需要考虑LPDDR5的2*tCK命令延迟调整

- [x] 5. 实现Power Down和Self Refresh命令支持
  - [x] 5.1 实现PDEA/PDXA/PDEP/PDXP命令时序约束
    - 实现Power Down进入和退出的时序检查
    - 检查tCKE, tXP, tACTPDEN, tRDPDEN, tWRPDEN, tPRPDEN, tREFPDEN等约束
    - 已在 `CheckerLPDDR5.cpp` 中实现
    - 已在 `lp5_ac_timing_test.cpp` 中添加12个测试用例
    - _Requirements: 7.1_

  - [x] 5.2 实现SREFEN/SREFEX命令时序约束
    - 实现Self Refresh进入和退出的时序检查
    - 检查tXSR, tSR等约束
    - 已在 `CheckerLPDDR5.cpp` 中实现
    - 已在 `lp5_ac_timing_test.cpp` 中添加11个测试用例
    - _Requirements: 7.1_

- [x] 6. 集成到DRAMSys框架
  - [x] 6.1 更新Configuration类支持LPDDR5
    - 修改 `lib/DRAMsys/src/libdramsys/DRAMSys/configuration/Configuration.cpp`
    - 添加LPDDR5 MemSpec类型识别
    - 创建MemSpecLPDDR5实例
    - _Requirements: 1.3_

  - [x] 6.2 更新Controller类支持LPDDR5 Checker
    - 修改Controller类以支持CheckerLPDDR5
    - 根据MemSpec类型选择正确的Checker实现
    - _Requirements: 7.1_

  - [x] 6.3 更新CMakeLists.txt
    - 添加MemSpecLPDDR5.cpp和CheckerLPDDR5.cpp到编译目标
    - _Requirements: 1.1_

- [x] 7. 编写测试激励
  - [x] 7.1 创建LPDDR5 AC Timing测试模块
    - 在DMU目录下创建 `lp5_ac_timing_test.h` 和 `lp5_ac_timing_test.cpp`
    - 参考现有的DDR4测试模块结构
    - 实现LPDDR5ACTimingTester类
    - _Requirements: 8.1, 8.2, 8.3, 8.4, 8.5_

  - [x] 7.2 实现16 Bank模式测试用例
    - 测试ACT命令约束（tRC, tRRD, tFAW）
    - 测试RD命令约束（tRCD, tCCD）
    - 测试WR命令约束（tRCD, tCCD, tRDWR）
    - 测试PRE命令约束（tRAS, tRTP, tWR）
    - 测试REF命令约束（tRFCab, tRFCpb, tPBR2PBR）
    - _Requirements: 8.4, 8.6_

  - [x] 7.3 实现8 Bank Group模式测试用例
    - 测试同Bank Group的tCCD_L约束
    - 测试不同Bank Group的tCCD_S约束
    - 测试同Bank Group的tWTR_L约束
    - 测试不同Bank Group的tWTR_S约束
    - _Requirements: 8.6, 9.1_

  - [x] 7.4 更新main.cc集成LPDDR5测试
    - 添加LPDDR5测试入口
    - 支持通过命令行参数选择测试模式
    - _Requirements: 8.5_

- [x] 8. Checkpoint - 确保所有测试通过
  - 确保所有测试通过，如有问题请询问用户

- [x] 9. 编写额外的属性测试
  - [x] 9.1 编写JSON序列化round-trip属性测试
    - **Property 7: MemSpec参数JSON序列化round-trip**
    - **Validates: Requirements 1.3**

  - [x] 9.2 编写Bank Group模式切换属性测试
    - **Property 8: Bank Group模式时序参数正确应用**
    - **Validates: Requirements 1.5, 9.1**

  - [x] 9.3 编写突发长度影响属性测试
    - **Property 9: 突发长度对时序的影响**
    - **Validates: Requirements 9.3**

  - [x] 9.4 编写预充电时序区分属性测试
    - **Property 10: 预充电时序区分**
    - **Validates: Requirements 9.6**

- [x] 10. Final Checkpoint - 确保所有测试通过
  - 确保所有测试通过，如有问题请询问用户

## Notes

- 所有任务均为必需任务，确保全面的测试覆盖
- 每个任务都引用了具体的需求以便追溯
- Checkpoint任务用于确保增量验证
- 属性测试验证通用的正确性属性
- 单元测试验证具体的示例和边界情况
- 实现顺序：MemSpec → Checker核心 → 集成 → 测试激励

