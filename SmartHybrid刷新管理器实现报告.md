# SmartHybrid 混合刷新管理器实现报告

## 一、任务目标

修改 DRAMSys 模拟器的刷新机制，创建新的 `RefreshManagerSmartHybrid`，实现：

1. **任务 a**：开启 postpone 后，根据推迟刷新的命令个数决定当前刷新方式
   - 低负载：使用 REFPB（Per-Bank Refresh）
   - 高负载：切换到 REFAB（All-Bank Refresh）

2. **任务 b**：当推迟刷新命令个数高于设定阈值时，强制对应的 bank 进行 Precharge

---

## 二、实现思路

### 核心逻辑

```
正常模式 (REFPB)
       ↓
flexibilityCounter >= panicThreshold ?
       ↓ 是
进入 Panic 模式
       ↓
activatedBanks > 0 ? → 发送 PREAB（任务 b）
       ↓ 否
发送 REFAB（任务 a）
       ↓
REFAB 完成 → 退出 Panic 模式 → 恢复 REFPB
```

### 关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| panicThreshold | 32 | 触发 REFAB 的阈值（maxPostponed/2） |
| maxPostponed | 64 | 最大推迟次数（8 banks × 8） |
| banksPerRank | 8 | 每个 Rank 的 Bank 数量 |

---

## 三、实现步骤

### 新建文件

| 文件 | 路径 |
|------|------|
| RefreshManagerSmartHybrid.h | `lib/DRAMsys/.../controller/refresh/` |
| RefreshManagerSmartHybrid.cpp | `lib/DRAMsys/.../controller/refresh/` |
| smart_hybrid.json | `lib/DRAMsys/configs/mcconfig/` |

### 修改文件

| 文件 | 修改内容 |
|------|----------|
| McConfig.h | 添加 SmartHybrid 枚举和 JSON 解析 |
| Configuration.h | 添加 SmartHybrid 枚举 |
| Configuration.cpp | 添加配置解析逻辑 |
| Controller.cpp | 添加 SmartHybrid 创建逻辑 |

---

## 四、仿真验证结果

### 测试配置
- 内存类型：LPDDR4-3200
- 流量模式：SAME_BANK_DIFF_ROW（高压力）
- 请求数量：20000
- 仿真时间：200 µs

### 统计数据

| 指标 | 数值 | 说明 |
|------|------|------|
| Panic 模式触发 | 3 次 | 刷新压力大时自动切换 |
| 强制 PREAB | 13 次 | 为 REFAB 做准备（任务 b） |
| REFAB 完成 | 3 次 | 快速清理积压刷新（任务 a） |
| REFPB 刷新 | 876 次 | 正常模式下的 Per-Bank 刷新 |

---

## 五、Log 关键信息

### 1. 初始化成功
```
[SmartHybrid] Initialized for Rank 0! PanicThreshold=32 MaxPostponed=64 BanksPerRank=8
```

### 2. 任务 a 验证 - 刷新方式切换
```
@19500 ns [SmartHybrid] Panic! Count=32 >= Threshold=32 -> Switching to REFAB strategy.
@19530625 ps [SmartHybrid] Issuing REFAB to clear debt (flexibilityCounter=32)
```

### 3. 任务 b 验证 - 强制 Precharge
```
@19500 ns [SmartHybrid] Forcing PREAB before REFAB (activatedBanks=1)
```

### 4. REFAB 完成并恢复 REFPB
```
@19551875 ps [SmartHybrid] REFAB completed! flexibilityCounter reduced by 8 to 24
@19551875 ps [SmartHybrid] Exiting Panic mode, returning to REFPB strategy.
```

---

## 六、完整工作流程

```
1. 正常 REFPB 模式
   [REFRESH] REFPB (Regular) Bank 1 at 487500 ps (flexibilityCounter: 0)
   
2. 刷新压力累积，触发 Panic（任务 a）
   @19500 ns [SmartHybrid] Panic! Count=32 >= Threshold=32 -> Switching to REFAB strategy.
   
3. 强制 Precharge（任务 b）
   @19500 ns [SmartHybrid] Forcing PREAB before REFAB (activatedBanks=1)
   
4. 发送 REFAB
   @19530625 ps [SmartHybrid] Issuing REFAB to clear debt (flexibilityCounter=32)
   
5. REFAB 完成，退出 Panic
   @19551875 ps [SmartHybrid] REFAB completed! flexibilityCounter reduced by 8 to 24
   @19551875 ps [SmartHybrid] Exiting Panic mode, returning to REFPB strategy.
   
6. 恢复 REFPB 模式
   [REFRESH] REFPB (Regular) Bank 1 at 23455625 ps (flexibilityCounter: 24)
```

---

## 七、结论

SmartHybrid 混合刷新管理器已成功实现并通过仿真验证：

- ✅ **任务 a**：根据推迟刷新命令个数（flexibilityCounter）动态切换刷新方式
- ✅ **任务 b**：高于阈值时强制执行 PREAB，为 REFAB 做准备
- ✅ 完整的状态机：REFPB → Panic → PREAB → REFAB → 恢复 REFPB
