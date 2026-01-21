# Requirements Document

## Introduction

本文档定义了LPDDR5 AC Timing Checker的需求规格。AC Timing（Access Timing）约束是DRAM控制器必须遵守的时序规则，确保内存命令之间保持正确的时间间隔，以保证数据完整性和内存设备的正常工作。

LPDDR5（Low Power DDR5）是面向移动设备和低功耗应用的高性能内存标准，相比DDR5有以下特点：
- 采用16n预取架构（DDR5为8n）
- 支持更高的数据速率（最高6400 MT/s）
- 采用不同的Bank结构（16个Bank，可配置为8 Bank Group模式或16 Bank模式）
- 支持Write-X命令和Data-Copy功能
- 支持多种刷新模式（All-Bank Refresh, Per-Bank Refresh）

本项目的目标是：
1. 整理LPDDR5的五类核心命令（RD/WR/ACT/PRE/REF）的AC Timing参数
2. 参考现有DDR4 Checker实现，开发LPDDR5 AC Timing Checker
3. 在main.cc中编写测试激励验证AC Timing约束的正确性

## Glossary

- **AC_Timing_Checker**: 负责检查和计算命令时序约束的模块，确保发出的命令满足DRAM规格要求
- **tCK**: 时钟周期时间（Clock Cycle Time），LPDDR5使用WCK（Write Clock）和CK（Command Clock），WCK频率为CK的2倍或4倍
- **tRCD**: Row to Column Delay - ACT到RD/WR命令的最小间隔
- **tRAS**: Row Active Time - ACT到PRE命令的最小间隔
- **tRP**: Row Precharge Time - PRE到下一个ACT命令的最小间隔
- **tRPpb**: Per-Bank Precharge Time - 单Bank预充电时间
- **tRPab**: All-Bank Precharge Time - 全Bank预充电时间
- **tRC**: Row Cycle Time - 同一Bank连续ACT命令的最小间隔（tRAS + tRP）
- **tRRD**: Row to Row Delay - 不同Bank的ACT到ACT最小间隔
- **tCCD**: Column to Column Delay - 连续RD/WR命令的最小间隔
- **tCCD_L**: Column to Column Delay (Long) - 同一Bank Group的RD/WR到RD/WR最小间隔（8 Bank Group模式）
- **tCCD_S**: Column to Column Delay (Short) - 不同Bank Group的RD/WR到RD/WR最小间隔（8 Bank Group模式）
- **tWTR**: Write to Read Turnaround - WR到RD命令的最小间隔
- **tWTR_L**: Write to Read Turnaround (Long) - 同一Bank Group的WR到RD最小间隔
- **tWTR_S**: Write to Read Turnaround (Short) - 不同Bank Group的WR到RD最小间隔
- **tRTP**: Read to Precharge - RD到PRE命令的最小间隔
- **tWR**: Write Recovery Time - WR到PRE命令的最小间隔
- **tFAW**: Four Activate Window - 任意4个ACT命令的最小时间窗口（16 Bank模式）
- **tRFCab**: All-Bank Refresh Cycle Time - 全Bank刷新命令完成所需时间
- **tRFCpb**: Per-Bank Refresh Cycle Time - 单Bank刷新命令完成所需时间
- **tREFI**: Refresh Interval - 两次REF命令之间的最大间隔
- **tPBR2PBR**: Per-Bank Refresh to Per-Bank Refresh - 连续Per-Bank刷新的最小间隔
- **tPBR2ACT**: Per-Bank Refresh to Activate - Per-Bank刷新到ACT的最小间隔
- **Bank**: DRAM中的存储单元组，LPDDR5支持16个Bank
- **Bank_Group**: LPDDR5在8 Bank Group模式下，每组2个Bank
- **Channel**: LPDDR5支持独立的双通道操作
- **MemSpec**: 内存规格配置，包含所有时序参数
- **BL16**: Burst Length 16，LPDDR5的默认突发长度
- **BL32**: Burst Length 32，LPDDR5支持的扩展突发长度
- **WCK**: Write Clock，LPDDR5的数据时钟，频率为CK的2倍或4倍
- **CK**: Command Clock，LPDDR5的命令时钟

## Requirements

### Requirement 1: LPDDR5 MemSpec时序参数定义

**User Story:** 作为开发者，我需要定义LPDDR5特有的时序参数结构，以便Checker能够获取正确的约束值。

#### Acceptance Criteria

1. THE MemSpecLPDDR5 SHALL 包含所有LPDDR5核心时序参数（tCK, tRCD, tRAS, tRPpb, tRPab, tRC, tRRD, tCCD, tWTR, tRTP, tWR, tFAW, tRFCab, tRFCpb, tREFI）
2. THE MemSpecLPDDR5 SHALL 支持LPDDR5的Bank结构（16个Bank，可配置为8 Bank Group模式或16 Bank模式）
3. THE MemSpecLPDDR5 SHALL 支持从JSON配置文件加载时序参数
4. WHEN 时序参数未在配置中指定 THEN THE MemSpecLPDDR5 SHALL 使用JEDEC LPDDR5规格的默认值
5. THE MemSpecLPDDR5 SHALL 支持Bank Group模式相关的时序参数（tCCD_L, tCCD_S, tWTR_L, tWTR_S）

### Requirement 2: ACT命令时序约束检查

**User Story:** 作为内存控制器，我需要在发出ACT（Activate）命令前检查时序约束，以确保不违反DRAM规格。

#### Acceptance Criteria

1. WHEN 对同一Bank发出ACT命令 THEN THE AC_Timing_Checker SHALL 确保距离上一次该Bank的ACT命令间隔不小于tRC
2. WHEN 对不同Bank发出ACT命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tRRD
3. WHEN 在8 Bank Group模式下对同一Bank Group的不同Bank发出ACT命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tRRD（LPDDR5的tRRD不区分Long/Short）
4. WHEN 在同一Channel发出ACT命令 THEN THE AC_Timing_Checker SHALL 确保任意4个ACT命令的时间窗口不小于tFAW
5. WHEN 对已预充电的Bank发出ACT命令 THEN THE AC_Timing_Checker SHALL 确保距离PRE命令间隔不小于tRPpb（单Bank）或tRPab（全Bank）
6. WHEN 在All-Bank REF命令后发出ACT命令 THEN THE AC_Timing_Checker SHALL 确保距离REF命令间隔不小于tRFCab
7. WHEN 在Per-Bank REF命令后对同一Bank发出ACT命令 THEN THE AC_Timing_Checker SHALL 确保距离REF命令间隔不小于tRFCpb

### Requirement 3: RD命令时序约束检查

**User Story:** 作为内存控制器，我需要在发出RD（Read）命令前检查时序约束，以确保数据能正确读取。

#### Acceptance Criteria

1. WHEN 对已激活的Bank发出RD命令 THEN THE AC_Timing_Checker SHALL 确保距离ACT命令间隔不小于tRCD
2. WHEN 在16 Bank模式下发出连续RD命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tCCD
3. WHEN 在8 Bank Group模式下对同一Bank Group发出连续RD命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tCCD_L
4. WHEN 在8 Bank Group模式下对不同Bank Group发出连续RD命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tCCD_S
5. WHEN 在WR命令后发出RD命令（16 Bank模式）THEN THE AC_Timing_Checker SHALL 确保间隔不小于tWTR加上写入延迟
6. WHEN 在WR命令后对同一Bank Group发出RD命令（8 BG模式）THEN THE AC_Timing_Checker SHALL 确保间隔不小于tWTR_L加上写入延迟
7. WHEN 在WR命令后对不同Bank Group发出RD命令（8 BG模式）THEN THE AC_Timing_Checker SHALL 确保间隔不小于tWTR_S加上写入延迟

### Requirement 4: WR命令时序约束检查

**User Story:** 作为内存控制器，我需要在发出WR（Write）命令前检查时序约束，以确保数据能正确写入。

#### Acceptance Criteria

1. WHEN 对已激活的Bank发出WR命令 THEN THE AC_Timing_Checker SHALL 确保距离ACT命令间隔不小于tRCD
2. WHEN 在16 Bank模式下发出连续WR命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tCCD
3. WHEN 在8 Bank Group模式下对同一Bank Group发出连续WR命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tCCD_L
4. WHEN 在8 Bank Group模式下对不同Bank Group发出连续WR命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tCCD_S
5. WHEN 在RD命令后发出WR命令 THEN THE AC_Timing_Checker SHALL 确保间隔满足读写切换时序（tRL + tDQSCK + tBURST - tWL + tWPRE）

### Requirement 5: PRE命令时序约束检查

**User Story:** 作为内存控制器，我需要在发出PRE（Precharge）命令前检查时序约束，以确保行数据已正确保存。

#### Acceptance Criteria

1. WHEN 对已激活的Bank发出PRE命令 THEN THE AC_Timing_Checker SHALL 确保距离ACT命令间隔不小于tRAS
2. WHEN 在RD命令后发出PRE命令 THEN THE AC_Timing_Checker SHALL 确保距离RD命令间隔不小于tRTP
3. WHEN 在WR命令后发出PRE命令 THEN THE AC_Timing_Checker SHALL 确保距离WR命令间隔不小于tWR加上写入延迟和突发时间
4. WHEN 发出PREAB（全Bank预充电）命令 THEN THE AC_Timing_Checker SHALL 确保所有Bank满足上述约束

### Requirement 6: REF命令时序约束检查

**User Story:** 作为内存控制器，我需要在发出REF（Refresh）命令前检查时序约束，以确保刷新操作正确执行。

#### Acceptance Criteria

1. WHEN 发出All-Bank REF命令 THEN THE AC_Timing_Checker SHALL 确保所有Bank已预充电（距离最后的PRE命令间隔不小于tRPab）
2. WHEN 发出连续All-Bank REF命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tRFCab
3. WHEN 在All-Bank REF命令后发出其他命令 THEN THE AC_Timing_Checker SHALL 确保间隔不小于tRFCab
4. WHEN 发出Per-Bank REF命令 THEN THE AC_Timing_Checker SHALL 确保目标Bank已预充电（距离PRE命令间隔不小于tRPpb）
5. WHEN 发出连续Per-Bank REF命令到同一Bank THEN THE AC_Timing_Checker SHALL 确保间隔不小于tRFCpb
6. WHEN 发出连续Per-Bank REF命令到不同Bank THEN THE AC_Timing_Checker SHALL 确保间隔不小于tPBR2PBR

### Requirement 7: 时序约束计算接口

**User Story:** 作为调度器，我需要查询某个命令最早可以发出的时间，以便进行命令调度优化。

#### Acceptance Criteria

1. THE AC_Timing_Checker SHALL 提供timeToSatisfyConstraints接口，返回给定命令满足所有约束的最早时间
2. WHEN 调用timeToSatisfyConstraints THEN THE AC_Timing_Checker SHALL 考虑所有相关的时序约束并返回最大值
3. THE AC_Timing_Checker SHALL 提供insert接口，记录已发出命令的时间戳用于后续约束检查
4. THE AC_Timing_Checker SHALL 支持查询Bank Group模式（8 BG或16 Bank模式）

### Requirement 8: 测试激励验证

**User Story:** 作为验证工程师，我需要编写测试激励来验证AC Timing Checker的正确性。

#### Acceptance Criteria

1. THE Test_Stimulus SHALL 能够生成违反各类时序约束的命令序列
2. THE Test_Stimulus SHALL 能够验证Checker正确检测到时序违规
3. THE Test_Stimulus SHALL 能够验证合法命令序列被正确接受
4. THE Test_Stimulus SHALL 覆盖五类核心命令（RD/WR/ACT/PRE/REF）的主要时序约束
5. THE Test_Stimulus SHALL 输出清晰的测试结果报告
6. THE Test_Stimulus SHALL 分别测试8 Bank Group模式和16 Bank模式

### Requirement 9: LPDDR5特有时序支持

**User Story:** 作为开发者，我需要支持LPDDR5相比DDR4/DDR5的特有时序特性。

#### Acceptance Criteria

1. THE AC_Timing_Checker SHALL 支持LPDDR5的16 Bank结构和可选的8 Bank Group模式
2. THE AC_Timing_Checker SHALL 支持LPDDR5的Per-Bank Refresh模式（tRFCpb, tPBR2PBR, tPBR2ACT）
3. THE AC_Timing_Checker SHALL 支持LPDDR5的BL16和BL32突发长度
4. THE AC_Timing_Checker SHALL 支持LPDDR5的WCK/CK时钟比率（2:1或4:1）
5. THE AC_Timing_Checker SHALL 支持LPDDR5的tDQSCK参数（DQS到CK的延迟）
6. THE AC_Timing_Checker SHALL 区分tRPpb（单Bank预充电）和tRPab（全Bank预充电）时序

