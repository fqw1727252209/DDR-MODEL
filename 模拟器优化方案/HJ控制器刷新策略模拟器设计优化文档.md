# HJ控制器刷新策略模拟器设计优化文档 v0.2

**日期**：2026-03-16

---

## DDR5 刷新机制深度理解

### 为什么需要刷新

DRAM 核心存储单元是一个电容 + 晶体管，电容会随时间自然漏电，超过约 64ms（温度相关）数据就会丢失。**Refresh 就是周期性给电容重新充电**的过程。

DDR5 相比 DDR4 变化：
- 引入 Same-Bank Refresh（REFsb），减少刷新对带宽的影响
- 引入 Refresh Management（RFM），防止 Row Hammer 安全问题
- 支持 Fine Granularity Refresh（FGR），温度升高时加倍刷新频率

---

### 关键时序参数（DDR5-4800 典型值）

| 参数 | 含义 | 值 |
|---|---|---|
| tREFI1 | 1x 模式刷新间隔 | 3.904 μs |
| tREFI2 | 2x 模式刷新间隔 | 1.952 μs |
| tRFCab | REFab 命令后不可访问时间 | ~295 ns |
| tRFCsb | REFsb 命令后不可访问时间 | ~160 ns |
| MaxPostpone（1x） | 最大推迟次数 | 4 |
| MaxBurst（1x） | 1个tREFI内最多发 | 5条 |

---

### 刷新命令如何产生——HJ 控制器完整流程

- **第一步：寄存器配置启动刷新**

==**评审：**控制器上电后，软件写寄存器完成配置，关键寄存器==：

```
RefabEn = 1        → 使用 All-Bank 刷新模式
RefPostEn = 1      → 允许推迟（Postpone 模式）
RefMode = 0        → 固定 1x 刷新频率
MaxPostpone1x = 8  → 最大允许推迟 8 次（实际受协议约束最多 4 次）
RefiAb1x = 3904    → tREFI 对应的时钟周期数（按 tCK 换算）
Rank${i}TrefiStartValue → 各 Rank 首次刷新的偏移量（实现交错Stagger）
```

配置完成后，**DEVMGR 模块的 `inst_ref_top` 子模块开始独立计时**。

- **第二步：tREFI 计时器超期产生刷新请求**

每个逻辑 Rank 有独立的 `cnt_trefie` 计数器：

```
初始值 = Rank${i}TrefiStartValue（交错偏移）
每时钟周期 -1
减到 0 → 重载 RefiAb1x → cnt_postpone++（产生一个待发的刷新请求）
```

这个超期动作是**纯硬件计时**，不依赖任何软件干预。

- **第三步：Postpone 计数器决定是否推迟**

```
cnt_postpone 含义：已到期但还未发出的刷新请求数量

等效逻辑：
  - 每次 tREFI 超期 → cnt_postpone++
  - 每成功发出一次 REFab → cnt_postpone--

推迟条件：当前 Rank/Bank 被读写占用，无法立即关页，暂不发刷新
推迟上限：cnt_postpone >= MaxPostpone → 进入 Critical 状态
```

- **第四步：Critical 信号产生（强制刷新）先实现基本功能-->再实现强制刷新**

以下任一条件满足，`ref_critical` 拉高：

1. `cnt_postpone >= MaxPostpone`（推迟次数超限）
2. 两次刷新之间间隔超过 `5 × tREFI`
3. ctrlupd / phyupd / zq 操作期间有未完成的推迟请求

`ref_critical` 拉高后，**调度器必须优先处理刷新**，停止向该 Rank 发送新的 ACT 命令，等当前打开的 Page 关闭后立即发 REFab。

Critical 退出条件：`cnt_postpone <= CntPostponeLowTh`（配置的退出下限，通常为 0）

- **第五步：REFab 发出前提——Bank 必须关闭**

REFab 命令要求目标 Rank 所有 Bank 处于 Precharged 状态：

```
若有 Bank 处于 Active 状态：
  → 等待正在进行的读写完成
  → 发送 PRE（Precharge）命令关闭该 Bank
  → 确认全部 Bank Precharged
  → 发送 REFab
```

这就是刷新对带宽的主要影响来源——**强制中断读写、关闭所有 Bank、等待 tRFCab 后才能再次访问**。

- **第六步：tREFI 内 Burst 上限控制**

`inst_ref_burst_interval` 模块保证：**1 个 tREFI 窗口内最多发出 5 个 REFab**（包含正常 1 个和最多提前的 4 个）。超过后即使还有 pending 也不发，等到下一 tREFI 窗口。

---

### 刷新与 Bank 的交互阻塞关系

这是理解模拟器行为的核心：

```
正常状态：
  Controller → ACT(Bank0) → RD/WR → PRE (自然流程)

Critical 刷新介入：
  RefreshMachine → [ref_critical=1]
        ↓
  BankSlice 收到 refresh_waiting 信号
        ↓
  当前 Bank 如果是 Active：下一个命令强制变为 PRE（不再接受 ACT/RD/WR）
  当前 Bank 如果是 Precharged：NOP（不开新页，等待）
        ↓
  所有 Bank 关闭后 → 发 REFab
  等待 tRFCab
        ↓
  RefreshMachine 收到 REFab 成功 → pending_count--
  若 pending_count 降到退出阈值 → ClearRefreshWaiting
        ↓
  BankSlice 恢复正常，可以接受 ACT 命令
```

---

==评审：【TBD】刷新的选择（REFab/REFsb）==

## 当前模拟器实现与 HJ 控制器对比

### 功能实现对照表

| 功能 | HJ 控制器 | 当前模拟器 | 差距说明 |
|---|---|---|---|
| tREFI 计时触发 | `cnt_trefie` 计数器 | `sc_time` 事件机制 | 机制不同但等效 ✅ |
| Postpone 计数 | `cnt_postpone` | `refresh_pending_count` | 等效 ✅ |
| Critical 进入 | `cnt_postpone >= MaxPostpone` | `pending_count >= threshold` | 等效 ✅ |
| Critical 退出下限 | `CntPostponeLowTh` | **未实现**，直接降到0退出 | ❌ 有差距 |
| BankSlice 阻塞协调 | `ref_critical` 信号线 | `is_refresh_waiting` 标志 | 等效 ✅ |
| PRE 强制关页 | 调度器自动处理 | BankSlice Evaluate 中处理 | 等效 ✅ |
| REFab 发出 | `u_cs_bsc_ref_gen` 时序判断 | `CmdSelect` 仲裁 | 等效 ✅ |
| **Rank 交错（Stagger）** | `Rank${i}TrefiStartValue` | **未实现**，所有Rank同时触发 | ❌ 缺失 |
| **tREFI 内 Burst 上限** | `inst_ref_burst_interval`（最多5次） | **未实现**，无上限 | ❌ 缺失 |
| **REFsb（Same-Bank）** | 支持 `RefabEn=0` | **未实现** | ❌ 缺失 |
| **FGR 2x 模式** | 支持 `RefMode=1` | **未实现**(目前应该有了，JSON配置，需要跑一下试试) | ❌ 缺失 |
| **RFM（RAA 计数器）** | `inst_ref_rfm` | **未实现** | ❌ 缺失 |
| 温度补偿（MR4） | `inst_ref_rm` 监控 | **未实现** | ❌ 缺失 |

---

## 正确性验证方案

> **核心要求：针对每一个已实现的刷新功能，做 RTL 波形 vs 模拟器 Log 的逐一比对**

### 验证框架设计

```
RTL 仿真（VCS/Questa）             模拟器（SystemC ESL）
         |                                   |
    波形文件(.vcd/.fsdb)           模拟器日志(DPRINT_INFO 输出)
         |                                   |
         └──────────── 对比脚本（Python） ───┘
                              |
                        对比结果报告
```

**模拟器侧日志增强点**（当前 DPRINT_INFO 已有基础，需补全验证所需的关键锚点）：
- 每次 tREFI 触发时打印：`[REFRESH] Rank=%d, pending_count=%d, time=%s`
- 每次 Critical 进入/退出：`[REFRESH] Critical=%s, Rank=%d`
- 每次 REFab 发出：`[REFRESH] REFab sent, Rank=%d, pending_after=%d`
- 每次 BankSlice 被阻塞：`[REFRESH] Bank=%d waiting for refresh`

---

### 功能 1：tREFI 周期性触发验证

**验证目标**：模拟器的刷新触发周期是否与 RTL 的 tREFI 一致

**RTL 观测信号**：
- `ref_req` 信号（每次到期拉高一拍），或者 `cnt_trefie` 置零的时刻

**模拟器观测**：
- `[REFRESH] Rank=%d, pending_count=%d` 出现的时间戳

**对比方法**：

```python
# 提取 RTL 中每次 ref_req 拉高的时间范围（周期）
# 提取模拟器中每次 pending++ 的时间差
# 期望：误差必须小于一个时钟周期
```

---

### 功能 2：Postpone 推迟与 Critical 验证

**验证目标**：推迟机制生效，Critical 在准确的待发数量限制下触发-->(==**评审：**一定要有请求才会推迟，AC Timing约束==)

**RTL 观测信号**：
- `cnt_postpone` 计数值
- `ref_critical` 信号变化边沿

**模拟器观测**：
- `refresh_pending_count` 值
- `IsRefreshCritical()` 导致的日志标志

**测试场景构建**：
- 施加高压连续读写操作（使得 Bank 持续繁忙），观察刷新是否被推迟（待发计数上升）
- 观察当计数达到设定的 `MaxPostpone`（例如 4）时，双方是否同时上报 Critical

---

### 功能 3：Bank 阻塞与关页流程验证

**验证目标**：判定 Critical 发生后，Bank 层是否正确阻断 ACT，并优先完成 PRE 关页并发出 REFab

**RTL 观测信号**：
- `ref_critical` 拉高后，总线上的 `cs_act`（ACT 指令）
- `cs_pre`（PRE 指令）
- `cs_ref`（REFab 指令）
- 以及时序：`PRE` 到 `REFab` 满足 `tRP`

**模拟器观测**：
- BankSlice 中 `Evaluate()` 的 next_command 被设为 `PRE`
- `CheckRankBankIsClosed()` 条件满足
- REFab 的发出时间

**对比方法**：
- 重点对齐 Critical 触发的绝对时间，逐一比对此后的命令下发轨迹：预期序列必须为 `强制PRE -> 等待 tRP -> 发出 REFab`

---

### 功能 4：REFab 发出后的通道恢复验证

**验证目标**：验证 REFab 发出后，严格遵守 tRFCab 锁定时间，之后通道才恢复读写

**RTL 观测信号**：
- `cs_ref` 发出后到下一个 `cs_act` 之间的时间间隔

**模拟器观测**：
- `ClearRefreshWaiting()` 被调用的时间
- 锁定解除后下一次 ACT 的发送时间

**对比方法**：
- 判断 `T(下一次ACT) - T(REFab)` 是否严格 $\ge$ `tRFCab`

---

## 改进实现计划

### Phase 1（对齐真实控制器语义）

| 任务 | 涉及代码位置 | 工作量 | 验证指标 |
|---|---|---|---|
| **Rank 交错（Stagger）** | `RefreshMachine` 的构造及初值设定 | 0.5天 | 各 Rank 不在同一周期触发刷新 |
| **Critical 退出下限** | `RefreshMachine`、`McConfig` 添加低阈值判断 | 0.5天 | 退出 Critical 的时机由目前直接消退改为依配置项 |
| **Burst 上限(单tREFI)** | `RefreshMachineManager` 中增加滑动窗口/计数器 | 1天 | 保证 T=tREFI 时间段内发出的 Refresh 数量不超过5。 |

### Phase 2（功能扩展）

| 任务 | 涉及代码位置 | 工作量 | 说明 |
|---|---|---|---|
| **REFsb 支持** | `RefreshMachine`, `BankSlice`, 时序基类 | 3天 | 减小阻塞粒度至每个 Bank Group 相同序号的 Bank |
| **RFM 管理机制** | `BankSlice`, 新增模块 | 2天 | 累积 RAA 发送 RFM，解决行锤击（RowHammer）问题 |

---

## Phase 1 核心功能 C++ 实现思路简述

为了应对技术评审中对具体代码实现的提问，特补充 Phase 1 中三个核心优化任务的代码实现架构思路：

### Rank 交错（Stagger）实现思路

**问题背景**：目前模拟器所有 Rank 的 `tREFI` 计时器初始值均相同，导致多 Rank 系统会在同一时刻爆发刷新请求，瞬间锁死整个通道带宽（刷新风暴）。
**代码修改点**：
1. **配置层 (`ConfigureFile` / `McConfig.hh`)**：在 JSON 配置文件及 `McConfig` 结构体中，为每个 Rank 新增初始化偏移量配置字段（如 `uint32_t trefi_start_offset`）。
2. **初始化层 (`RefreshMachine.cpp` 构造函数)**：在构建 `RefreshMachine` 时获取对应 Rank 的偏移值。
3. **计时逻辑 (`RefreshMachine` timer)**：SystemC 中管理刷新的 primary event 第一回触发时间由 `start_time = tREFI` 改为 `start_time = tREFI + (offset * tCK)`，将多个 Rank 的刷新触发在时间轴上错开。

### Critical 退出下限（Threshold）实现思路

**问题背景**：当前模拟器逻辑是待发刷新必须清零（`pending_count == 0`）才退出 Critical 恢复读写。真实控制器允许配置退出下限来兼顾延时指标。
**代码修改点**：
1. **配置层 (`McConfig.hh`)**：新增 `int ref_postpone_low_th` 作为退出下限阈值（通常默认为0）。
2. **状态仲裁层 (`RefreshMachine.cpp`)**：
   修改更新 `critical` 标志位的判定逻辑。
   *原代码类似*：`if (pending_count == 0) { is_critical = false; }`
   *修改为*：`if (pending_count <= config->ref_postpone_low_th) { is_critical = false; ClearRefreshWaiting(); }`
3. **通知机制 (`BankSliceManager.cpp`)**：确保下限触发时，能及时下发信号唤醒被挂起的正在排队的读写命令。

### 单 tREFI 窗口 Burst 上限控制实现思路

**问题背景**：为防止累积的推迟刷新一次性清库释放，DDR5 限制在一个 tREFI 窗口内最多发出 5 次刷新（通常是 1x模式的协议约束）。当前模拟器没有该上限，可能连续发出数十个 REFab。
**代码修改点**：
1. **数据结构 (`RefreshMachine.hh`)**：在类内新增一个定长滑动窗口队列，如 `std::deque<uint64_t> recent_ref_timestamps`，用于记录最近发送 REFab 的绝对时钟戳。
2. **发射记录 (`RefreshMachine::UpdateOnRefAb()`)**：每当调度器成功发射一笔 REFab，将 `sc_time_stamp().value()` push 进队列。若队列长度超过 5，则 pop 掉最早的一项。
3. **发射限制逻辑 (`RefreshMachine::CanIssueRefresh()`)**：-->（==**评审：这种可以不考虑**==）
   在向 `CmdSelect` 模块请求仲裁前，增加断言：
   `if (recent_ref_timestamps.size() == 5)`
   检查 `current_time - recent_ref_timestamps.front()` 是否 `< tREFI_duration`。
   如果小于，说明一个 `tREFI` 窗口内已发满 5 次。此时返回 `false` 阻塞此次发出请求；必须等时间流逝、滑动窗口移出最早一次记录后才放行。

---

## Phase 2 核心功能 C++ 实现思路简述 (扩展特性)

针对后续的高级功能，初步架构构思如下：

### REFsb（Same-Bank Refresh）实现思路

**问题背景**：DDR5 引入了 REFsb，允许只刷新各个 Bank Group 中具有相同 Bank Index 的 Bank，在此期间其他 Bank 依然可以响应读写，大大减少了刷新带来的阻塞。
**代码修改点**：
1. **状态细化 (`BankSliceManager.hh`)**：
   目前的 `is_refresh_waiting` 是 Rank 级别的。需要将其细化，改为维护一个 `bool refsb_waiting[BANK_GROUP_NUM][BANK_NUM]` 或按 Bank Index 锁定。
2. **命令调度 (`CmdSelect.cpp` / `Scheduler.cpp`)**：
   - 增加对 `REFsb` 命令的支持及仲裁权重。
   - 当发出 `REFsb` 后，时序约束模型 `SdramConstraint` 需分别处理同 Bank（受 `tRFCsb` 约束）和不同 Bank（不受约束）的允许访问时间。
3. **策略选择 (`RefreshMachine.cpp`)**：
   根据配置 `RefabEn == 0`（即不强制全 Bank 刷新），在生成刷新请求时，循环生成一组目标 Bank 序列（例如第一次刷 Bank0，下次刷 Bank1...），并通知 `BankSlice` 仅阻塞对应 Bank。

### RFM（Refresh Management / 防 RowHammer）实现思路

**问题背景**：DDR5 为防范 RowHammer 攻击，引入了 RAA（Rolling Accumulated Act）计数器。当一个 Bank 内的 Activate 次数超过阈值时，需要硬件自动发出 RFM 命令来刷新相邻的被害行。
**代码修改点**：
1. **统计层 (`BankSlice.hh/cpp`)**：
   每个 Bank 新增一个计数器 `uint32_t act_counter`（即 RAA 计数）。每次 `Evaluate()` 中成功发出 ACT 命令时，`act_counter++`。

   【评审】：单独的通道，RFM主动产生

2. **触发机制 (`BankSlice.cpp` / 新增 `RfmManager`)**：
   - 配置项中增加 `RAA_Threshold` (如 MR24 配置的乘积等)。
   - 当 `act_counter >= RAA_Threshold` 时，当前 Bank 拉高 `rfm_req` 标志位，并上报给 `Scheduler` 仲裁。
   - RFM 请求被受理后，向该 Bank（或整个 Rank）插入 `RFM` 命令。
   - 【评审】命令冲突的时候，如何仲裁<RFM和正常的REF？>  相当于分开记录RFM和正常REF，同时产生。如果满足RFM时序，并且同时有REF，优先发送RFM刷新---总结为：有RFM选择RFM，没RFM再考虑是否需要正常的REF（见控制器bsc_ref部分）

3. **时序约束 (`SdramConstraint.cpp`)**：
   RFM 的执行时间类似于一个稍微缩短的刷新命令，需要加入 `tRFM`（基于 `tRFCab` 或相似值）的死区时间控制，发完后将 `act_counter` 清零。

