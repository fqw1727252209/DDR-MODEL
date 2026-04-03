# **1\. 文档介绍**

## **文档目的**

本文档主要介绍了 HJ 控制器支持哪些刷新特性，控制器如何实现这些功能。

## **参考文档**

* UVIP-DDR5-Controller-UG-002-00021-0B\_20250328.pdf  
* JESD79-5B\_V1-2(ddr5\_spec).pdf

## **术语与缩略语**

**Table 1 术语与缩略语**

| 术语/缩略语 | 全称 |
| :---- | :---- |
| aref | auto refresh |
| rfm | refresh management |
| pbr | per bank refresh |
| sbr | same bank refresh |
| tREFI | time for refresh interval |
| FGR | Fine Granularity Refresh |

# **2\. Device Manger(DEVMGR)**

DEVMGR 模块是整个控制器中实现刷新操作、MR 写操作和动态频率调节的模块。本文只描述协议要求的刷新操作如何实现。

**刷新功能：**

* 控制器按照配置好的平均间隔周期生成 auto-refresh 操作。  
* 能够将最多 8 次控制器生成的 all-bank 刷新（固定 1X 刷新模式）组合在一起连续发出。  
* 当刷新操作被分组时，控制器可以监测自身的空闲状态，当空闲时间达到配置好的预定时长时，就会主动“推测性”地发出部分刷新操作，以充分利用空闲时间，减少后续操作的延迟，这个操作由 CS 信号控制。

## **2.1 刷新模式**

DEVMGR 支持小粒度刷新（Fine Granularity Refresh）模式。

\*\*注意：\*\*不支持 on-the-fly（OTF）模式。

**表 2-1 刷新模式**

| FGR 刷新模式 | 配置 | 描述 |
| :---- | :---- | :---- |
| Fixed 1X | RefMode=0 | Normal 模式 |
| Fixed 2X | RefMode=1 | Fgr 模式 |

**表 2-2 “推测性”的刷新模式**

| 推测性的刷新模式 | 配置 | 描述 |
| :---- | :---- | :---- |
| 普通模式 | RefPostEn=0 | 刷新操作会以固定的间隔周期发出。 |
| 推迟模式 | RefPostEn=1 | 刷新操作会根据当前是否有报文选择是否延迟发出。 |

**表 2-3 All-Bank 刷新和 Same-Bank 刷新**

| ABR 模式 | 配置 | 描述 |
| :---- | :---- | :---- |
| All-bank 刷新 | RefabEn=1 | 在 1x/2x 模式下均可配置。 |
| Same-bank 刷新 | RefabEn=0 | 需要在 FGR 模式的配置序列中进行配置。 |

\*\*注意：\*\*如果 tREFI/(2\*每个 BG 的 bank 数量) \<= 每个物理 rank 包含的逻辑 rank 数量 \* tRFCsb\_dlr，禁止在 FGR 模式下将 csrRefabEn 配置为 0。

## **2.2 普通寄存器**

以下的 CSR 寄存器，不管刷新模式是哪一种都要配置。

**表 2-4 刷新相关的普通寄存器**

| 域段名 | 类型 | 缺省值 | 描述 |
| :---- | :---- | :---- | :---- |
| RefabEn | RW | 'h1 | 0: same-bank 刷新 1: all-bank 刷新 |
| RefPostEn | RW | 'h1 | 使能或不使能刷新延迟。 |
| RefMode | RW | 'h0 | 刷新模式。 DDR5: • 0: 固定 1X • 1: 固定 2X • 其他值: 保留。 |
| MaxPostpone1x | RW | 'h8 | 小粒度 1X 模式下能够延迟发出的最大刷新数量。 |
| MaxPostpone2x | RW | 'h8 | 小粒度 2X 模式下能够延迟发出的最大刷新数量。 |
| MaxPostpone4x | RW | 'h8 | 保留，未使用。 |
| RefiAb1x | RW | 'h0 | All-bank 刷新间隔：1xtREFI。 向下取整(1*tREFIab/(tCK*freq\_ratio))。 |
| Rank${i}TrefiStartValue | RW | 'h0 | 在多 rank 模式下，Rank${i}首次刷新计时器开始时的偏移值。 csrRefabEn=1。 csrRank${i}TrefiStartValue \< 向下取整(1*tREFIab/(tCK*freq\_ratio))。 |
| LRankTrefiOffset | RW | 'h0 | 不同逻辑 rank 的 tREFI 计数器之间的偏移。 |
| DisRank | RW | 'h0 | 使能或禁用每个 rank 的刷新功能，每一位对应一个 rank。 静态值，配置完后不发生变化。 |
| DisPrank | RW | 'h0 | 使能或者禁用每个物理 rank 的刷新功能，每一位对应一个物理 rank。 静态值，配置完后不发生变化。 |
| tRfcab1x | RW | 'h0 | 小粒度 1X 模式下对相同逻辑 rank 产生的 REFab 命令之间的时间间隔。 |
| tRfcab2x | RW | 'h0 | 小粒度 2X 模式下对相同逻辑 rank 产生的 REFab 命令之间的时间间隔。 |
| tRfcab4x | RW | 'h0 | 保留，未使用。 |
| tRfcabDlr1x | RW | 'h0 | 小粒度 1X 模式下对不同逻辑 rank 产生的 REFab 命令之间的时间间隔。 |
| tRfcabDlr2x | RW | 'h0 | 小粒度 2X 模式下对不同逻辑 rank 产生的 REFab 命令之间的时间间隔。 |
| tRfcabDlr4x | RW | 'h0 | 保留，未使用。 |
| tRfcabDpr1x | RW | 'h0 | 小粒度 1X 模式下对不同物理 rank 产生的 REFab 命令之间的时间间隔。 |
| tRfcabDpr2x | RW | 'h0 | 小粒度 2X 模式下对不同物理 rank 产生的 REFab 命令之间的时间间隔。 |
| tRfcabDpr4x | RW | 'h0 | 保留，未使用。 |
| tRfcsbSlr | RW | 'h0 | 对相同逻辑 rank 产生的 REFsb 命令之间的时间间隔。 |
| tRfcsbDlr | RW | 'h0 | 对不同逻辑 rank 产生的 REFsb 命令之间的时间间隔。 |
| RmPb2AbTh | RW | 'h2 | 当 csrRefabEn=0 时，从 refsb 模式切换到 refab 模式的刷新速率阈值。保持在默认值。 |
| RefWideRangeEn | RW | 'h0 | 是否支持 DDR5 协议下的更细粒度的温度控制，来实现刷新宽范围功能。 |
| CntPostponeLowThAb | RW | 'h0 | 在 refab 模式中，当已经发送的刷新命令数量等于该值时，ref\_critical 状态信号无效。 如果配置为 0 时，说明当推迟的刷新命令数量为 0 时，ref\_critical 状态信号无效。 |
| CntPostponeLowThPb | RW | 'h0 | 在 refsb 模式中，当已经发送的刷新命令数量等于该值时，ref\_critical 状态信号无效。 如果配置为 0 时，说明当推迟的刷新命令数量为 0 时，ref\_critical 状态信号无效。 |

对于 4 个物理 rank，每个物理 rank 包含 4 个逻辑 rank 共 16 个 rank 的配置，csrDisRank 和 csrDisPrank 提供按位粒度的刷新功能使能或关闭。具体如下表所示。

**表 2-5 配置表**

|  |  | 逻辑 rank 索引 |  |  |  |
| :---- | :---- | :---- | :---- | :---- | :---- |
|  |  | 0 | 1 | 2 | 3 |
| **物理 Rank 索引** | 0 | DisRank\[0\] | DisRank\[1\] | DisRank\[2\] | DisRank\[3\] |
|  | 1 | DisRank\[4\] | DisRank\[5\] | DisRank\[6\] | DisRank\[7\] |
|  | 2 | DisRank\[8\] | DisRank\[9\] | DisRank\[10\] | DisRank\[11\] |
|  | 3 | DisRank\[12\] | DisRank\[13\] | DisRank\[14\] | DisRank\[15\] |

## **2.3 温度补偿刷新**

通过使能 csrMrr4En 来启用这个功能。

**表 2-6 温度补偿刷新寄存器**

| 域段名 | 类型 | 缺省值 | 描述 |
| :---- | :---- | :---- | :---- |
| Mrr4En | RW | 'h1 | 0: 停止检查 TEMP 是否发生变化。 |
| Mrr4Rank | RW | 'h1 | 表明该 rank 是否需要读取 MR4 寄存器。 |
| Mr4RdInter | RW | 'h80\_0000 | TEMP 检查间隔。 |

## **2.4 刷新管理（RFM）**

为了启用 DEVMGR 模块的刷新管理功能，一定要将 csrRfmEn 配置为 'h1，同时配置 csrRaadec（MR59 OP\[7:6\]）、csrRaamult（MR58 OP\[7:5\]）和 csrRaaimt（MR58 OP\[4:1\]）。

**表 2-7 刷新管理和自适应刷新管理寄存器**

| 域段名 | 类型 | 缺省值 | 描述 |
| :---- | :---- | :---- | :---- |
| RfmEn | RW | 'h0 | • 1'b0=禁用 RFM。 • 1'b1=使能 RFM。 静态配置。 |
| RfmabEn | RW | 'h0 | • 1'b0=RFMsb。 • 1'b1=RFMab。 RFMsb 只能在 FGR 模式下启用。 |
| Raadec | RW | 'h0 | 每发一个 RFM 命令，RAA 计数器减少的值。 |
| Raamult | RW | 'h0 | 每发送一个 ACT 命令，RAA 计数器增加的值。 |

## **2.5 小粒度刷新模式（FGR）切换序列**

FGR 模式切换是一个完整且无法分割的操作，不能被其他的软件操作打断。

### **DDR5 的 FGR 序列：refab \-\> refsb**

1. **阻塞数据传输。**  
   a. 将所有通道的 csrXmuHold 置为 1 来阻止新的报文到来。  
   b. 轮询所有通道的 csrXmuIdle 状态信号寄存器直到全部拉高，然后将所有通道的 csrUifHold 置为 1。  
   c. 轮询所有通道的 csrMcIdle 状态信号寄存器直到全部拉高来确认所有的命令都已完成并且 DDRCTL 处于 idle 状态。  
2. **禁用低功耗模式。**  
   a. 将所有通道的 csrPdnEn 和 csrSrEn 置为 0。  
   b. 读取通道 0 的 csrSwSr 寄存器，如果为 1，执行软件自刷新退出流程并且不清除通道 0 的 csrUifHold 和 csrXmuHold 状态位。读取通道 1 的 csrSwSr 寄存器，如果为 1，执行软件自刷新退出流程并且不清除通道 1 的 csrUifHold 和 csrXmuHold 状态位。  
   c. 读取通道 0 的 csrSwMpsm 寄存器，如果为 1，退出软件控制的 MPSM 状态并且不清除通道 0 的 csrUifHold 和 csrXmuHold 状态位。读取通道 1 的 csrSwMpsm 寄存器，如果为 1，退出软件控制的 MPSM 状态并且不清除通道 1 的 csrUifHold 和 csrXmuHold 状态位。  
   d. 轮询所有通道的 csrDdrLpState 状态信号寄存器直到全部变为 0。  
3. **轮询软件触发事件的状态。**  
   a. 轮询所有通道的 csrMpcTrig 和 csrMpcBusy 状态信号寄存器直到全部变为 0。  
   b. 轮询所有通道的 csrCtrlupdTrig 和 csrCtrlupdBusy 状态信号寄存器直到全部变为 0。  
4. **配置 FGR 切换流程的标志位。**  
   a. 将所有通道的 csrFgrStart 置为 1。  
   b. 轮询所有通道的 csrArbState 状态信号寄存器直到两次读出的值都为 'h1。  
5. **执行 FGR 模式切换操作。**  
   a. 如果 PHY 不支持 FGR 模式，把所有通道的 csrPhymstrEn 置为 0。  
   b. 将所有通道的 csrDfiLpEnSr 和 csrSdramCgEnSrDeep 置为 0。  
   c. 对所有通道执行进入自刷新的操作序列。  
   d. 将所有通道的 csrRefMode 置为 'h1。  
   e. 根据当前工作频率下的 csrRefMode 为所有通道的 csrMr4Value 设置合适的值。  
   f. 执行退出自刷新的操作序列。  
6. **重新使能被禁用的配置。**  
   a. 轮询所有通道的 csrSwOpBusy 状态信号寄存器直到全部变为 0。  
   b. 如果有要求的话，把所有通道的 csrPdnEn/csrSrEn 配置为 1。  
   c. 如果有要求的话，把所有通道的 csrDfiLpEnSr 和 csrSdramCgEnSrDeep 置为 1。  
   d. 将所有通道的 csrUifHold 置为 0。  
   e. 将所有通道的 csrXmuHold 置为 0 来重新接收新的报文。  
7. **将所有通道的 csrFgrStart 置为 0 来清除 FGR 切换标志位。**

### **DDR5 的 FGR 序列：refsb \-\> refab**

1. **阻塞数据传输。**  
   a. 将所有通道的 csrXmuHold 置为 1 来阻止新的报文到来。  
   b. 轮询所有通道的 csrXmuIdle 状态信号寄存器直到全部拉高，然后将所有通道的 csrUifHold 置为 1 来禁用 ecc scrubber 功能。  
   c. 轮询所有通道的 csrMcIdle 状态信号寄存器直到全部拉高来确认所有的命令都已完成并且 DDRCTL 处于 idle 状态。  
   *(步骤 2-4 同上)*  
2. **执行 FGR 模式切换操作。**  
   a. 如果 PHY 不支持 FGR 模式并且 csrRefMode 要改为 'h1，把所有通道的 csrPhymstrEn 置为 0 如果先前已禁用的话。对于其他配置，把所有通道的 csrPhymstrEn 保持在 1。  
   b. 将所有通道的 csrDfiLpEnSr 和 csrSdramCgEnSrDeep 置为 0。  
   c. 执行进入自刷新的操作序列。  
   d. 将所有通道的 csrRefMode 置为 'h1 来进入 FGR 模式或者配置为 'h0 来进入普通模式。  
   e. 将所有通道的 csrRefabEn 置为 'h1。  
   f. 根据当前工作频率下的 csrRefMode 为所有通道的 csrMr4Value 设置合适的值。  
   g. 执行退出自刷新的操作序列。  
   *(步骤 6-7 同上)*

### **DDR5 的 FGR 序列：普通模式下的 refab \-\> FGR 模式下的 refab**

*(步骤 1-4 同上)*

5\. **执行 FGR 模式切换操作。**

a. 如果 PHY 不支持 FGR 模式并且 csrRefMode 要改为 'h1，把所有通道的 csrPhymstrEn 置为 0。对于其他配置，把所有通道的 csrPhymstrEn 置为 1。

b. 将所有通道的 csrDfiLpEnSr 和 csrSdramCgEnSrDeep 置为 0。

c. 执行进入自刷新的操作序列。

d. 将所有通道的 csrRefMode 置为 'h1 来进入 FGR 模式或者配置为 'h0 来进入普通模式。

e. 将所有通道的 csrRefabEn 保持在 'h1。

f. 根据当前工作频率下的 csrRefMode 为所有通道的 csrMr4Value 设置合适的值。

g. 执行退出自刷新的操作序列。

*(步骤 6-7 同上)*

## **2.6 进入与退出自刷新**

如果 csrSdramCgEnSrDeep 为 0，两个通道的进入或者退出自刷新操作可以各自进行。单通道序列如下：

**进入自刷新：**

1. 轮询 csrSrTrig 和 csrSrBusy 寄存器直到都变为 0。  
2. 读取 csrSwSr。  
   * 如果是 0，跳转到步骤 3。  
   * 如果是 1，表明系统已经触发了一次自刷新进入流程，不需要其他操作并且结束当前的自刷新流程。  
3. 将 csrXmuHold 置为 1，通知 XMU 停止接收新的请求。  
4. 轮询 csrXmuIdle。如果为高，将 csrUifHold 置为 1。  
5. 轮询 csrMcIdle。如果为高，表明所有命令都完成了，DDRCTL 处于 idle 状态。  
6. 轮询 csrDdrLpState 直到变为 0。  
7. 将 csrSrType 和 csrSrTrig 置为 1，发送 SRE 命令。  
8. 轮询 csrSrTrig 和 csrSrBusy 直到全部变为 0，证明命令已经发送。

**退出自刷新：**

1. 轮询 csrSrTrig 和 csrSrBusy 直到全变为 0。  
2. 读取 csrSwSr 寄存器。  
   * 如果为 'h1，跳转至步骤 3。  
   * 如果为 'h0，表明系统进入 SRX 状态，不需要执行自刷新退出流程并且结束当前的自刷新流程。  
3. 将 csrSrType 置为 'h0，csrSrTrig 置为 1，发送 SRX 命令。  
4. 轮询 csrSrTrig 和 csrSrBusy 直到全变为 0，证明 SRX 已经发出。  
5. 将 csrUifHold 置为 'h0。  
6. 将 csrXmuHold 置为 'h0，重新接收请求。

如果 csrSdramCgEnSrDeep 为 1，两个通道的进入或者退出自刷新操作必须同时进行。双通道序列与单通道相同，需要注意的是每次判断和操作都要确保两个通道都已完成。

# **3\. 控制器如何发出刷新操作**

刷新操作分为三个部分，命令的产生、命令发出的时机以及命令最终在 dfi 接口上发出。其中命令的产生由 inst\_devmgr 模块中的 inst\_ref\_top 模块实现，命令发出的时机即刷新命令与其他命令之间的优先级仲裁、时序等在 u\_cs\_bsc\_ref\_gen 中实现，命令最终在 dfi 接口上发出，则由 inst\_dfi 模块实现。本章节主要对刷新命令的产生进行说明，命令发出的时机也有涉及。

## **3.1 刷新命令的产生**

一个刷新请求可以正常地从 dfi 接口发出，首先要按照协议在与颗粒处于相同的刷新模式下，以 rank 为单位按时间间隔产生相应的刷新请求，根据配置的寄存器 csrRefabEn 可以确定产生的刷新请求类型。

协议允许刷新请求暂缓被发出，即 postpone 功能。通过该功能控制器可以灵活选择发出刷新请求的时机，对于 refab 类型的刷新请求，进行刷新前要使整个目标 rank 处于关闭状态，这也就说明整个刷新命令的执行过程中（也包括因执行刷新命令而无法发出 active 命令的时序要求）的整个时间段内，目标 rank 都处于一个无法被读写命令访问的状态，这在高带宽的访问压力中无疑是对带宽的一种浪费，而 postpone 功能初步缓解了这种情况。

**inst\_ref\_top 模块架构说明：**

![1_inst_ref_top架构图](\\wsl.localhost\Ubuntu-20.04\home\feng\0_Proj\ddr_esl\模拟器优化方案\现有HJ控制器文档\assets\1_inst_ref_top架构图.png)
* 核心主体为 32 个 inst\_ref\_rank\_\* 模块，分别对 3ds 模式下的 4 个物理 rank（每个物理 rank 包含 8 个逻辑 rank）同时进行刷新管理。其产生的请求最终传递给 u\_cs\_bsc\_ref\_gen 进行时序判断。  
* **inst\_ref\_rm**：监控 MR4 寄存器状态，处理因温度升高产生的 tREFI 参数调整触发。  
* **inst\_ref\_fgr\_change**：执行刷新模式切换（支持 1x/2x）。  
* **inst\_ref\_sce\_ctrl**：实现刷新 flush 功能，启用时会尽快执行延期的刷新命令。  
* **inst\_ref\_rank\_\* 内部主体**：  
  * inst\_ref\_timer：实现 postpone 暂缓功能。  
  * inst\_ref\_before\_sre：计算进入自刷新前的请求数量。  
  * inst\_ref\_burst\_interval：计算每个 tREFI 期间最多可发的刷新数量。  
  * inst\_ref\_mode\_switch：确认模式转换的相关配置是否完成。  
  * inst\_ref\_critical：处理延期请求超限进入/退出 critical 状态的逻辑。  
  * inst\_ref\_rfm：处理针对 act 攻击额外需要的 rfm 刷新请求。  
  * inst\_ref\_sw\_ctrl：处理软件接管的刷新逻辑。  
  * inst\_ref\_req\_gen：汇聚所有请求并传递给后续模块。

### **3.1.1 postpone 的实现**

代码中实现了一个整体的 cnt\_postpone 计数器来对 rank 进行延期数量的刷新请求进行计算，每当 ddr 的状态发生了变化，只要涉及到 postpone 数量的重新计算，相应的计数值都会赋值到这个整体 cnt\_postpone 中。
 ![2_postpone的实现](\\wsl.localhost\Ubuntu-20.04\home\feng\0_Proj\ddr_esl\模拟器优化方案\现有HJ控制器文档\assets\2_postpone的实现.png)

举例在 1x 模式下的 refab 刷新中，该计数器理论最大为 4，而在 2x 模式下的 refsb 中，该计数器理论最大为 32。

### **3.1.2 trefie 超期的计算**

每个 rank 都实现了一个 cnt\_trefie 计数器来确定本 rank 依照协议应该何时产生一个刷新请求。

![3_trefie超期的计算](\\wsl.localhost\Ubuntu-20.04\home\feng\0_Proj\ddr_esl\模拟器优化方案\现有HJ控制器文档\assets\3_trefie超期的计算.png)

由图可知，当计数器进行模式切换或者进入了 DFS 操作时，会将一个固定配置的 REF\_EXPIRE\_VAL 赋给当前计数器（通常配置为 1）。这是因为控制器的设计导致在进行刷新模式切换时需要先进入 sr 状态，并且 dfs 操作也只能在 1x 模式下进行。这样在退出自刷新后会立即产生一次 cnt 的超期，从而使计算 postpone 刷新请求的数量加一。

### **3.1.3 SRX 状态与 refab 请求的发送**

![4_SRX状态与refab请求发送](\\wsl.localhost\Ubuntu-20.04\home\feng\0_Proj\ddr_esl\模拟器优化方案\现有HJ控制器文档\assets\4_SRX状态与refab请求发送.png)

每当控制器发出一个 SRX（即退出自刷新状态）命令，按照协议规定都要对未被禁用的 rank 发送一个 refab 命令。控制器通过 ref\_critical\_srx 信号线表明需要立即产生刷新请求。结合 postpone 逻辑，该 refab 不会使 cnt\_postpone 减一；退出自刷新时 cnt\_postpone 会加一，且很快又因 cnt\_trefie 到期再次加一。

### **3.1.4 一个 tREFI 中刷新数量的限制**

![5_tREFI刷新数量限制](\\wsl.localhost\Ubuntu-20.04\home\feng\0_Proj\ddr_esl\模拟器优化方案\现有HJ控制器文档\assets\5_tREFI刷新数量限制.png)

inst\_ref\_burst\_interval 模块用于实现协议中对每个 tREFI 时间间隔中能发出的命令数量进行限制。模块为其分配空闲计数器，在 tREFI 时间后自动清零。若记录满载（例如 ddr5 1x 模式下一个 tREFI 期间已发出 5 个 refab），则不能再产生新的刷新请求。

### **3.1.5 critical 信号的产生**

![6_critical信号产生](\\wsl.localhost\Ubuntu-20.04\home\feng\0_Proj\ddr_esl\模拟器优化方案\现有HJ控制器文档\assets\6_critical信号产生.png)

ref\_critical 信号分为 refab、refsb 两种。除了超出 cnt\_postpone 设置外，在 ctrlupd、phyupd、zq 操作时若有未完成的 postpone 请求，或连续两个刷新请求间隔超过最大配置值（如 1x 模式 5tREFI1），都会拉高 critical 信号。当计数值低于下限时拉低。

## **3.2 刷新命令时序的计算**

![7_刷新命令时序计算](\\wsl.localhost\Ubuntu-20.04\home\feng\0_Proj\ddr_esl\模拟器优化方案\现有HJ控制器文档\assets\7_刷新命令时序计算.png)

控制器能发出的所有请求都会经由 inst\_schd 模块决定选择（内部由 inst\_bsc\_top 进行时序计算）。当满足发出条件时，选中的目标 rank/bank 信息会以 ref\_body 形式传回 inst\_ref\_top。

## **3.3 刷新命令的发出**

接口上发出的刷新命令符合 DFI 协议要求，包含 REFab, RFMab, REFsb, RFMsb 等指令及对应的管脚电平定义组合。

# **4\. 控制器有关刷新的其他逻辑实现**

## **4.1 刷新操作的时序灵活性**

**4.13.6 Refresh Operation Scheduling Flexibility**

```
In general, a Refresh command needs to be issued to the DDR5 SDRAM regularly every tREFI interval. To allow for improved
efficiency in scheduling and switching between tasks, some flexibility in the absolute refresh interval is provided.
In Normal Refresh mode, a maximum of 4 REFab commands can be postponed, meaning that at no point in time more than a total of
4 Refresh commands are allowed to be postponed. In case that 4 REFab commands are postponed in a row, the resulting maximum
interval between the surrounding REFab commands is limited to 5 × tREFI1 (see Figure 69). At any given time, a maximum of 5
REFab commands can be issued within 1 x tREFI1 window. Self-refresh mode may be entered with a maximum of 4 REFab
commands being postponed. After exiting Self-Refresh mode with one or more REFab commands postponed, additional REFab
commands may be postponed to the extent that the total number of postponed REFab commands (before and after the Self-Refresh)
will never exceed 4. During Self-Refresh Mode, the number of postponed REFab commands does not change. An additional REFab
command is required after Self-Refresh exit (refer to Clause 4.9 for more information)
```



在 JESD-5, DDR5 的协议中，关于刷新操作的时序灵活性规定：Normal Refresh 模式下最多允许 4 个 REFab 命令被 postponed（推迟）。在任何给定时间，1 x tREFI1 窗口内最多可以发出 5 个 REFab 命令。进入 Self-refresh 模式时最多允许携带 4 个 postponed 的 REFab 命令。退出后，可以继续 postponed，但前后的总数量永远不会超过 4 个。

协议允许进入自刷新状态前将被延缓发送的 refab 指令继续延缓至退出自刷新状态后进行，但并未明确说明采用何种方式。基于自刷新前后 1x、2x 模式改变的场景，可以有以下两种方式实现该功能：

1. postpone 的计算，1x 模式和 2x 模式共用同一个计数器。当进行模式切换时，将 1x 模式下延缓的 refab 数量通过算法变换为 2x 模式下应刷出的 refsb 数量。**目前控制器采用此种方式。**  
2. 针对 1x 模式下的 refab 命令和 2x 模式下的 refsb 命令，分别实现两个计数器 a、b 记录其延缓的刷新命令数量，再实现一个 mix 计数器，用于计算 1x 和 2x 下的命令总和。

**混用风险说明：**

控制器和颗粒（或者说 vip 模型）应使用相同的计数方式，否则可能会出现以下场景（例如：控制器使用第一种，颗粒使用第二种）：

* a. 处于 1x 模式时累计了 1 个 refab 未发出（被 postpone）；此时颗粒同样记录计数器 a 缺少 1 个 refab，计数器 b 未缺少。  
* b. 切换至 2x 模式，控制器将这些 refab 转换为 refsb 的数量并发出；此时颗粒记录计数器 a 仍缺少 1 个 refab，计数器 b 多收到（pullin） 1 个 refab 或 4 个 refsb。  
* c. 切换回 1x 模式，控制器此时认为不少发任何刷新请求。但经过 4 个 tREFI 后，颗粒认为累计了 5 个 refab 未发送，违反了协议。

这样的场景会导致模型在使用时报错。对于极端场景（如频繁在 1x 下推迟、2x 下补刷），会导致模型记录的计数器远超最大值。又由于模型中有 MaxPullIn 的设定，超出的刷新请求会被丢弃，最终导致长时间后总体的 mix 计数器与控制器发出的总体数量不符。

## **4.2 rfm 操作**

协议要求 tRFM 和 tRFC 的大小要保持一致。

控制器使用 inst\_ref\_rfm 模块来产生 rfm 请求。实现方式按照协议要求，当某个在一段时间内多次被 act 且没有刷新过，其中的 cnt\_raa 会不断累计，直到产生 rfm 类请求。每当执行一个刷新操作，对应的 cnt\_raa 也会下降相应的数目，具体数目由寄存器配置。