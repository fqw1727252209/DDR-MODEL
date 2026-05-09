# DDRCTL-T-T02-PRESB、PREAB-DESIGN-DETAIL

| 文件标识 | |
| :--- | :--- |
| 当前版本 | 1.2 |
| 作者 | 王笑乐 |
| 完成日期 | |

# 版本历史

| 版本 | 修订时间 | 修订人 | 修订章节 | 修订内容 |
| :--- | :--- | :--- | :--- | :--- |
| 1.0 | 2025.12.19 | 王笑乐 | | 初稿 |
| 1.1 | 2026.01.28 | 王笑乐 | | 补充寄存器说明 |
| 1.2 | 2026.02.04 | 王笑乐 | | 补充模块结构、cct update 说明 |

# 图目录

图 1 precharge 真值表 ............................................................................................ 6
图 2 普通 precharge 与 ref_critical precharge 流程 ................................................... 7
图 3 devmgr 相关操作 precharge 流程 ........................................................................ 7
图 4 理想情况下，ref_critical, 8 个 open 的 bank 执行 prepb 消耗时间 ....................... 8
图 5 prepb 插入 rd 命令间，ref_critical, 8 个 open 的 bank 执行 prepb 消耗时间 ........... 9
图 6 ref_critical, active 插入, 8 个 open 的 bank 执行 prepb 消耗时间 ...................... 9
图 7 在非 ap 配置下 page miss 的示例 ........................................................................ 10
图 8 ref_critical, 执行 presb 时的收益 ........................................................................ 10
图 9 在非 ap 配置下 page miss 的示例执行 presb 时的收益 ......................................... 11
图 10 presb、preab 设计模块结构 ............................................................................... 11
图 11 原设计 precharge、active 仲裁 ......................................................................... 14
图 12 增加 presb、preab 后的仲裁逻辑 ..................................................................... 14
图 13 precharge 命令编码 ........................................................................................... 16

# 表目录

表 1 presb、preab 请求来源 ........................................................................................ 12
表 2 高优先级和低优先级 presb、preab 请求描述 ...................................................... 13
表 3 prepb 操作需要检查的常见时序参数 .................................................................... 15

# 1. 文档介绍

## 1.1 文档目的

本文档主要介绍了控制器目前的 precharge 模式，请求产生、执行流程、存在的缺陷，另外介绍一下增加 presb、preab 的 precharge 模式实现方案的一些想法。

## 1.2 参考文档

UVIP-DDR5-Controller-UG-002-00021-0B_20250328.pdf
JESD79-5B_V1-2(ddr5_spec).pdf

## 1.3 术语与缩略语

Table 1 术语与缩略语

| 术语/缩略语 | 全称 |
| :--- | :--- |
| prepb | per bank precharge |
| presb | same bank precharge |
| preab | all bank precharge |
| ap | auto precharge |
| rr | round robin |
| bg | bank group |

# 2. precharge 背景介绍

在 DDR5 协议中有三种 precharge 模式，分别为 prepb、presb、preab，prepb 模式每次只 precharge 一个 bank，presb 模式类似于 refsb，precharge 的是所有 bg 相同 index 的 bank，而 preab precharge 的则是 1rank (3ds) 或 prank (非 3ds) 中所有的 bank。

| Function | Abbreviation | CS_n | CA0 | CA1 | CA2 | CA3 | CA4 | CA5 | CA6 | CA7 | CA8 | CA9 | CA10 | CA11 | CA12 | CA13 | NOTES |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Precharge All | PREab | L | H | H | L | H | L | CID3 | V | V | V | V | L | CID0 | CID1 | CID2 | 5, 20 |
| Precharge Same Bank | PREsb | L | H | H | L | H | L | CID3 | BA0 | BA1 | V | V | H | CID0 | CID1 | CID2 | 6, 20 |
| Precharge | PREpb | L | H | H | L | H | H | CID3 or DRFM-L | BA0 | BA1 | BG0 | BG1 | BG2 | CID0 | CID1 | CID2 | 7, 20 |

[图片描述：图 1 precharge 真值表。该图以表格形式展示了 PREab、PREsb 和 PREpb 三种命令在 CS_n 和 CA[0:13] 信号线上的逻辑电平编码要求，详细列出了不同模式下对 Bank Address (BA) 和 Bank Group (BG) 的选择逻辑。]

HJ 控制器支持 prepb、软件 presb、软件 preab 的 precharge 模式，不支持硬件 presb、preab 模式。读写访问出现 page miss、ref_critical、zq 等操作之前 bank 非 close 则会产生 prepb 请求，之后 prepb 请求和 active、rd、wr 等仲裁，获得仲裁后执行 prepb 操作。

## 2.1 prepb 请求产生及执行流程

在控制器中，prepb 请求的产生主要分为以下几种情况：

(1) 在非 AP 模式时，读写访问出现 pagemiss，需要执行 prepb 操作，再打开新行。

(2) 当有刷新处于 ref_critical 时，经过仲裁后的 bank 会禁止执行 rd、wr、act 等操作，等待执行 prepb。

(3) devmgr 相关的一些操作，如 zq、ctrlupd 等，产生的 lrank_pre_req_dev[31:0] 传递给 bsc，请求 prepb。

```verilog
assign rd_pre_wait_out = (pre_cur_state[index_SUB_BSC_BK] && rd_pre_wait_in) || (cur_bsc_pre_wait);
assign rd_wait_out = (rd_cur_state[index_SUB_BSC_BK] && rd_wait_in);
assign wr_act_wait_out = (act_cur_state[index_SUB_BSC_BK] && wr_act_wait_in);
assign wr_pre_wait_out = (pre_cur_state[index_SUB_BSC_BK] && wr_pre_wait_in) || (cur_bsc_pre_wait);

wr_pre_wait = cct_wr_cmd_vld && ~bsc_bank_close && ~wr_cmd_page_hit
rd_pre_wait = cct_rw_cmd_vld && ~bsc_bank_close && ~wr_cmd_page_hit
```

[图片描述：展示了关键刷新请求（Critical Refresh Request）的生成逻辑。由 `critical_refab_bank` 与 `allocated_lrank_bitmap`、`critical_refsb_wait` 与 `allocated_sbank_bitmap` 等四组信号分别进行与运算得到各自的 `req_vld` 信号。这些信号随后通过一个多输入或门，最后与 `~bsc_close` 进行与运算，生成 `ref_pre_gen_t` 信号，用于触发预充电生成。]

[图片描述：该架构图展示了 DDR 控制器中 Precharge 策略的整体硬件实现。包括左侧的 `devmgr`（设备管理）和 `ref_top`（刷新顶部模块），中间核心部分是 `bsc_top`，它包含多个针对 Bank 的控制切片 `bsc_bk`。每个切片通过组合逻辑处理 `ref_pre_gen` 信号，并生成 `pre_wait` 信号。生成的信号进入 `bsc_access_select` 进行仲裁，最终由 `gsc` 模块发出 `do_pre` 指令，并由 `fsc` 模块反馈执行状态 `pre_executed_onehot`。]

图 2 普通 precharge 与 ref critical precharge 流程

[图片描述：图 3 展示了 devmgr 相关操作的 precharge 流程架构图。图中包含 devmgr 模块（内部有 ref_top, ref_rank, hw_cmd_src, cmd_gen_top 等子模块）与 CS 模块（内部有 bsc_top, bsc_bk, bsc_ref_gen 等子模块）之间的信号交互。重点展示了 ref_critical 信号如何通过内部组合逻辑触发 pre_req_dev，最终通过 gsc 模块生成 pre_wait 并由 fsc 模块执行 do_pre 操作。]

图 3 devmgr 相关操作 precharge 流程

## 2.2 prepb 模式存在的缺陷

在某些场景下仅靠硬件 prepb 的 precharge 模式就显得效率很低下，比如 refsb 的刷新模式下刷新进入 critical、refab 的刷新模式下刷新进入 critical、devmgr 操作、或者多 bg 相同 index 的 bank page miss 等，在这些场景下就需要执行很多次 prepb 关闭涉及到的 bank，下面章节主要列举两种场景下的示例，其他场景类似。

### 2.2.1 ref critical 产生的 prepb

当某个 rank/lrank 累积的刷新进入到 critical 状态时，相应的 critical_refsb_wait_group、critical_*_lrank 信号也会传递给 bsc 模块。经过 RR 仲裁后，在 refsb 模式下，每个 rank/lrank 会仲裁出一个 bank (8 个 bg 相同 index 的 bank)，该 bank 会禁止任何读写请求的发出并立即发出 prepb 请求 (postpone 修复前)。

当刷新处于 refsb 模式时，极限情况下，某个 rank/lrank 的 8 个 bg 相同 index 的 bank 都处于 open 状态时，需要执行 8 个 prepb 命令。在刷新 postpone 问题修复之前，处于 critical 的 bank 会很快执行 precharge，8 个 prepb 命令可能消耗在 trfcsb 的时间内，没有多余的时序消耗。在刷新 postpone 问题修复之后，处于 critical 的 bank precharge 的时机已接近 trfcsb 边界，那么 8 个 bank 执行 8 个 prepb 命令就会导致刷新间隔增大。

如图 4 所示，理想情况下，刷新处于 refsb 模式，在无 rd、wr、act 等其他操作时，prepb 每个 dfi_clk 都能获得仲裁，需要 8 个 dfi_clk 才能关闭所有 bank。rd、wr 属于 col 类型的命令，优先级高于 prepb，tccd 最优的时序是 2 个 dfi_clk，假设 rd 命令之间可以按照这个最优的时序执行，如图 5 所示，prepb 命令插入 rd 与 rd 之间，这样就需要 16 个 dfi_clk 才能执行完 8 个 prepb 命令。row 类型的命令除了 prepb 还有 act，所以中间可能 act 命令或其他 bank 的 prepb 命令插入，如图 6 所示，这样 8 个 prepb 命令的执行时间就大于 16 个 dfi_clk。

此处举的是 refsb 模式下刷新处于 critical 时的例子，refab 或 devmgr 产生的 precharge 请求存在的局限性和该例子类似。

[图片描述：图 4 展示了在理想情况下，ref critical 状态下 8 个处于 open 状态的 bank 执行 prepb 命令的消耗时间。时序图显示了从 BG0 BANK0 到 BG7 BANK0 的 8 个 bank 组。在 refsb_critical 信号触发后，每个 bank 依次发出 1 个 dfi_clk 宽度的 prepb 命令，呈阶梯状排列。总执行时间正好为 8 个 dfi_clk。]

图 4 理想情况下，ref critical，8 个 open 的 bank 执行 prepb 消耗时间

[图片描述：图 5 展示了在 ref critical（刷新紧急）状态下，prepb 命令被插入到 rd 命令流中的时序。图中 8 个不同 Bank Group (BG0-BG7) 的 BANK0 依次执行 prepb 操作，总耗时约为 8*2 dfi_clk。下方 original bank 轴显示了连续的 read 命令流及其间隔。]

图 5 prepb 插入 rd 命令间, ref critical, 8 个 open 的 bank 执行 prepb 消耗时间

[图片描述：图 6 展示了在 ref critical 状态下，包含 active 和 read 命令插入时的时序图。图中 8 个不同 Bank Group (BG0-BG7) 的 BANK0 依次执行 prepb，由于增加了 active 命令，总耗时超过了 8*2 dfi_clk。]

图 6 ref critical, active 插入, 8 个 open 的 bank 执行 prepb 消耗时间

### 2.2.2 非 ap 配置下的 page miss

在某些场景下，比如地址连续的场景，控制器关闭 ap 功能，性能收益会更大。在控制器关闭 ap 功能时，存在不同 bg 相同 index 的 bank 同时在等待 precharge 的情况，如果为这些 bank 分别执行 prepb，效率就很低下。图 7 是在非 ap 配置下的示例，假设某个 lrank 的 8 个 bg 都在使用，由于 prepb 的时序或仲裁原因，在某个时刻可能存在 8 个 bg 的 bank0 在 page miss 后等待 precharge 的情况，如果只支持 prepb 的 precharge 模式就需要 8 个 dfi clk 甚至更多的时间完成 8 个 bank 的 precharge，此时可以执行一个 presb 命令就完成 8 个 bank 的 precharge。

[图片描述：图 7 展示了在非 ap 配置下 page miss 的示例时序。图中 BANK0 在 8 个不同 Bank Group 中均处于 open 状态并等待 precharge。若采用 prepb 方式，8 个 bank 依次执行会导致较长的累积延迟，总计约 8 dfi_clk。]

图 7 在非 ap 配置下 page miss 的示例

## 3. presb、preab 实现方案

### 3.1 presb、preab 模式性能收益

根据在 2.2 章节给出的场景描述，刷新处于 critical 时，在 8 个 bg 相同 index 的 bank 都处于 open 的状态下，presb 模式的增加，可使 precharge 时间节省至少 7 个 dfi_clk 以上，如图 8 所示。在非 ap 模式下的 page miss，在 8 个 bg 相同 index 的 bank 都处于 open 的状态等待 precharge 的情况下，presb 模式的增加，在某些场景下可使 precharge 时间节省至少 7 个 dfi_clk 以上，如图 9 所示。presb 模式的具体收益和 bank 使用情况以及控制器的命令执行状态有关，preab 模式的收益与 presb 类似。增加 presb/preab 模式，可以减少刷新延迟，也可以使 bank 尽快 open 别的 row。

[图片描述：图 8 展示了在 ref_critical 场景下执行 presb 的收益。图表对比了 8 个 Bank Group (BG0-BG7) 的 BANK0 在 open 状态下执行刷新操作的时序，重点标注了 trfcsb - trp 的跨度，以此显现 presb 模式带来的时间收益。]

图 8 ref_critical，执行 presb 时的收益

[图片描述：图 9 展示了在非 ap 配置下 page miss 的示例执行 presb 时的收益。这是一个详细的指令周期时序图，展示了不同 Bank Group 在执行 rd/wr 后，如何通过 presb 机制减少预充电等待时间 (prepb wait)。]

图 9 在非 ap 配置下 page miss 的示例执行 presb 时的收益

### 3.2 presb、preab 设计模块结构

[图片描述：图 10 展示了 presb、preab 的设计模块结构。图中包含 iproc、cq (包含 cq_rd_cam, cq_wr_cam) 和 cs (包含 bsc_top, pre_top, gsc, fsc_row) 等主要模块。pre_top 模块内部细分为 presb_and_preab_req, pre_arbiter, presb_timing_check, pre_req_match_bsc 等子模块。]

图 10 presb、preab 设计模块结构

presb、preab 设计的模块结构如上图所示，增加了四个子模块 presb and preab req、pre arbiter、presb timing check、pre req match bsc，另外涉及到一些模块的修改，具体为：

(1) presb and preab req 模块的作用是 presb、preab 请求的产生，将请求按照 bank、lrank 排序。

(2) pre arbiter 模块的作用是对高优先级、低优先级 的 presb、preab 请求进行仲裁。

(3) presb timing check 模块的作用是对 presb 进行时序检查。

(4) pre req match bsc 模块的作用是将 presb、preab 请求映射为 bsc。

(5) gsc 模块需要输出 presb、preab 执行时的指示信号。

(6) fsc 模块需要输出 presb、preab 执行时的地址信息。

(7) presb、preab 执行时涉及到多个 bsc，所以 cam 中的 cct update 需要修改，由多个 trigger 触发。

### 3.3 presb、preab 请求产生

控制器为支持 presb、preab 的 precharge 模式，需要先考虑在什么场景下使用 presb/preab。目前考虑的是 presb 模式使用在 refsb 处于 critical、非 ap page miss 的场景，preab 模式使用在 refab 处于 critical、devmgr 相关操作、非 ap page miss 等场景，如表 1。

在非 appagemiss 的场景下，lrank/rank 的所有 bg、bank 不一定都在使用，极限情况下，只有一个 bg 在使用，此时就没必要产生 presb 请求，所以设想设置阈值，当所有在使用中的 bg 相同 index 的 bank 都在等待 prepb 并且满足时序，超过阈值后，就可以产生 presb 请求，preab 类似。

**表 1 presb、preab 请求来源**

| pre 模式 | 来源 | 信号 | 位宽 | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| presb | ref_critical、page miss | presb_req | 128 | 在 3ds 配置下，每 4bit 对应一个 lrank 的 4 个 bank。在非 3ds 配置下，bit0~3、32~35、64~67、96~99 对应 4 个 prank。 |
| preab | ref_critical、page miss、devmgr | preab_req | 32 | 在 3ds 配置下，每个 bit 对应一个 lrank。在非 3ds 配置下，bit0、8、16、24 对应 4 个 prank。 |

### 3.3.1 presb、preab 请求优先级划分

precharge、active 均输入 col 类型的命令，两种类型的命令经过仲裁后才能执行，增加 presb、preab 的 precharge 模式后，需要考虑其与 active 之间的优先级。根据前面介绍 presb、preab 请求来源均有多种情况，其中有比较紧急的场景如 ref critical，也有不紧急的场景如非 ap 的 pagemiss、devmgr 操作。因此设想把 presb、preab 请求划分为两个优先级，比较紧急的场景下使用高优先级的 presb h req、preab h req，不紧急的场景下使用低优先级的 presb l req、preab l req。

presb h req、preab h req 是刷新处于 critical 产生的，为避免 ref 被推迟，需要尽快执行 pre，所以认为 presb h req、preab h req 优先级可以高于 active 或 prepb。低优先级的 presb l req、preab l req 请求则认为和 active、prepb 处于相同的优先级。

**表 2 高优先级和低优先级 presb、preab 请求描述**

| pre 模式 | 来源 | 信号 | 位宽 | 描述 |
| :--- | :--- | :--- | :--- | :--- |
| prepb | /* | /* | /* | /* |
| presb | refsb_critical | presb_h_req | 128 | 高优先级的 presb 请求。在 3ds 配置下，每 4bit 对应一个 lrank 的 4 个 bank，在非 3ds 配置下，bit0~3、32~35、64~67、96~99 对应 4 个 prank。 |
| | pagemiss | presb_l_req | 128 | 低优先级的 presb 请求。在 3ds 配置下，每 4bit 对应一个 lrank 的 4 个 bank，在非 3ds 配置下，bit0~3、32~35、64~67、96~99 对应 4 个 prank。 |
| preab | refab_critical | preab_h_req | 32 | 高优先级的 preab 请求。在 3ds 配置下，每个 bit 对应一个 lrank，在非 3ds 配置下，bit0、8、16、24 对应 4 个 prank。 |
| | pagemiss、devmgr | preab_l_req | 32 | 低优先级的 preab 请求。在 3ds 配置下，每个 bit 对应一个 lrank，在非 3ds 配置下，bit0、8、16、24 对应 4 个 prank。 |

### 3.4 presb、preab 请求仲裁

[图片描述：图 11 原设计 precharge、active 仲裁。展示了一个逻辑框图，包含输入信号 access_sel_bks[63:0]、element_ac_rr[511:0] 等进入 RR 仲裁器，输出 do_pre 和 do_act 等信号。]

图 11 原设计 precharge、active 仲裁

precharge 和 active 原设计 rr 仲裁逻辑如图 11 所示，等待 precharge 和 active 的 bank 一起经过 rr 仲裁，所以 precharge、active 的优先级是相同的。在增加 presb、preab 的 precharge 模式后，需要增加新的仲裁逻辑，如图 12 所示，具体描述为：

(1) 当 presb_h_req/preab_h_req 存在，并且满足时序时，不选择 prepb、active、presb_l、preab_l，需要禁用图 11 中 precharge 和 active 的原设计仲裁逻辑。

因为 presb_h_req/preab_h_req 可能存在多组 bank 或 lrank 请求执行 presb 或 preab，所以需要增加两组 rr 仲裁逻辑。仲裁后根据仲裁结果的 bank 匹配 bsc，输出 do_pre，并把执行 precharge 的结果返回给 bsc。

(2) 当 presb_h_req/preab_h_req 不存在或不满足时序时，此时可以选择 prepb、active、presb_l、preab_l，图 11 中 precharge 和 active 的原设计仲裁逻辑可以使用。

presb_l_req、preab_l_req 执行的需要满足条件 a、b，然后根据仲裁的 bank 匹配 bsc，输出 do_pre，并把执行 precharge 的结果返回给 bsc。

a) prepb、active 的 rr 仲裁结果的 bank 是准备执行 prepb 的。

b) 要执行 prepb 的 bank 在 presb_l_req、preab_l_req 的范围内。

(3) 如果 1 和 2 条件都不满足，则继续执行原设计 prepb 和 active 的仲裁和执行逻辑。

[图片描述：图 12 增加 presb、preab 后的仲裁逻辑。这是一个详细的逻辑电路图，展示了从 pre_wait、act_wait、presb_h_req[127:0] 和 preab_h_req[31:0] 信号出发，经过多个 RR（Round Robin）仲裁模块和选择逻辑，最终汇总产生 do_act[63:0] 和 do_pre[63:0] 信号的过程。中间包含一个名为“presb_l/preab_l 筛选条件”的逻辑块，列出了四个判断条件：1. 是否无 preab_h/presb_h；2. 是否有 preab_l_req/presb_l_req；3. 获得仲裁的 bank 是否是等待 prepb 的 bank；4. 获得仲裁的 bank 是否在 presb_l_req/preab_l_req 范围内。]

图 12 增加 presb、preab 后的仲裁逻辑

### 3.5 presb、preab 请求时序检查

在目前的控制器设计中，包含 prepb、preab 的时序检查，只需要增加关于 presb 模式的时序检查。在 presb 的时序管理中，可以使用 prepb 的时序，根据 presb 请求匹配到对应的 bsc，如果这些 bsc 的 prepb 时序都是满足的，那么可以认为 presb 的时序是满足的。prepb 操作需要检查的常见时序参数如表 3 所示，除了表中提及的参数外，还有 zq、mpc 等 device 相关的时序检查。

**表 3 prepb 操作需要检查的常见时序参数**

| To cmd | From cmd | prank | lrank | bg | 时序参数 | 描述 |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| pre | pre | same | /* | /* | tppd | Minimum core_clk cycles between precharge cmd and precharge cmd |
| | act | same | same | same | csrRasMin | Minimum core_clk cycles between active and precharge cmd for same bank |
| | rd | same | same | same | csrRd2pre | RD cmd to PRE cmd delay<br>注：协议是 tRTP |
| | wr/mwr | same | same | same | csrWr2pre | WR cmd to PRE cmd delay<br>注：协议是 CWL+BL/2+tWR |
| | mrw/mrr | same | /* | /* | tMR | MRW/MRR to valid command<br>协议是 tMRD |

### 3.6 presb、preab 命令执行信息打包和编码

[图片描述：图 13 precharge 命令编码。该图展示了 JEDEC DDR5 规范中关于 PREab、PREsb 组和 PREpb 的具体命令编码规则，表明 PREab 与 PREsb 在前五位指令编码上相同，仅通过 CA10 标识符作功能区分（CA10=1 为 PREsb，CA10=0 为 PREab）。]

图 13 precharge 命令编码

在 DDR5 协议中，preab、presb 的编码前五位是相同的，都是 5'b01011，两种模式是通过 CA10 进行区分的，当 CA10=1'b1 时，对应 presb 模式，当 CA10=1'b0 时，对应 preab 模式。

cs 模块将要执行的命令信息输出给 dfi 模块，在 dfi 的子模块 dfi_cmd_encoder 中根据真值表编码后给 PHY，因此在执行 presb、preab 时需要修改 cs 模块输出的执行命令信息打包后的信号 cmd_body 和 cmd_type。

### 3.7 cct update 逻辑

cam 中的 cct 在执行 precharge、active 时需要 update，原逻辑每次只涉及到一个 bsc，在增加 presb、preab 设计后，每次执行时涉及到多个 bsc，因此在增加 presb、preab 设计后，cct 需要由多个 trigger 触发同时触发。

### 3.8 寄存器描述

| 寄存器信号名 | 位宽 | 描述 |
| :--- | :--- | :--- |
| param_presb_or_preab_bypass | 1bit | 增加的控制器 presb、preab 模式的设计 bypass<br>1: bypass 增加的控制器 presb、preab 模式的设计<br>0: 使能增加的控制器 presb、preab 模式的设计 |
| param_high_presb_en | 1bit | 使能高优先级请求 presb 模式<br>1：使能高优先级请求 presb 模式<br>0：使能高优先级请求 presb 模式 |
| param_high_preab_en | 1bit | 使能高优先级请求 preab 模式<br>1：使能高优先级请求 preab 模式<br>0：使能高优先级请求 preab 模式 |
| param_low_presb_en | 1bit | 使能低优先级请求 presb 模式<br>1：使能低优先级请求 presb 模式<br>0：使能低优先级请求 presb 模式 |
| param_low_preab_en | 1bit | 使能低优先级请求 preab 模式<br>1：使能低优先级请求 preab 模式<br>0：使能低优先级请求 preab 模式 |
| param_low_presb_thre | 4bit | 某 lrank 相同 index 的 bank 的 precharge 请求，个数达到该阈值时产生低优先级 presb 请求。 |
| param_low_preab_thre | 6bit | 某 lrank 的 precharge 请求，个数达到该阈值时产生低优先级 preab 请求。 |
| param_presb_interval_counter | 5bit | presb 和 presb 命令之间的时序间隔，单位是 dfi_clk |
