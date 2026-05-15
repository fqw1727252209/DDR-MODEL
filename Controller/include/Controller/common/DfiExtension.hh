#ifndef __DFI_EXTENSION_HH__
#define __DFI_EXTENSION_HH__

#include "Controller/common/ControllerCommon.hh"
#include "Common/UifExtension.hh"
#include "sysc/utils/sc_report.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <cassert>
#include <cstdint>
#include <tlm>

#include "Controller/common/Command.hh"

namespace dmu{
    namespace Controller{

/*
========================================================================
DfiExtension 设计规格说明
========================================================================
一、模块描述
------------------------------------------------------------------------
DfiExtension 是一个 TLM Extension, 用于描述一个 DFI 时钟周期内DDR Command 的打包 (Command Packing) 信息。
该扩展对象用于在 ESL/SystemC 性能模型中模拟如下行为：
    1. 一个 DFI cycle 内可以传输多个 DDR Command
    2. Command 根据类型占用不同数量的 DFI slot
    3. Command 与 tlm_generic_payload 建立关联关系
    4. 支持 1N / 2N 模式下不同的 Command 展开宽度
    5. 支持 frequency_ratio = 8 时的奇偶 slot lane 分离
该扩展对象不拥有 transaction 生命周期，仅保存外部tlm_generic_payload 的指针。

二、类继承关系
------------------------------------------------------------------------
class DfiExtension : public tlm::tlm_extension<DfiExtension>
该类必须实现 TLM Extension 所要求的虚函数：
    clone()
    copy_from()
用于满足 TLM 扩展对象的复制语义要求。但是实现时禁止使用这两个函数，防止transaction指针的资源管理的问题

三、构造函数参数
------------------------------------------------------------------------
DfiExtension(unsigned frequency_ratio, bool is_2n_mode)
参数说明：
1. frequency_ratio
表示 DFI 时钟与 DDR 时钟之间的频率比例：
        frequency_ratio = DFI_CLK : DDR_CLK
常见取值：
        2
        4
        8
该参数同时也表示：
        一个 DFI cycle 内可使用的 Command Slot 数量
例如：
        frequency_ratio = 4
        slot index:
            0  1  2  3
2. is_2n_mode
表示 DFI 是否工作在 2N mode:
        false -> 1N 模式
        true  -> 2N 模式
该模式将影响 Command 占用的 slot 数量。

四、内部数据结构
------------------------------------------------------------------------
DfiExtension 内部维护一个固定长度的 slot 队列：
        trans_queue
队列长度：
        frequency_ratio
每一个元素表示一个 DFI Command Slot。
Slot 数据结构如下：
    struct Slot
    {
        bool is_cmd_end;
        Command cmd;
        tlm_generic_payload* trans;
    };
字段说明：
is_cmd_end
    标识该 slot 是否为某个 command 的结束 slot。
cmd
    DDR command 类型。
trans
    对应的 tlm_generic_payload 指针。
注意：
    tlm_generic_payload 只会存储在 command 的
    “最后一个 slot”中。

五、Command Slot 占用规则
------------------------------------------------------------------------
每一个 DDR command 会占用多个连续 slot。
slot 占用数量由以下两个因素决定：
    1. Command 类型
    2. DFI 模式 (1N / 2N)
1N 模式：
    Command     Slot 数量
    ------------------------
    ACT         2
    WR          2
    WRA         2
    RD          2
    RDA         2

    REFab       1
    REFSb       1
    PREab       1
    PRESb       1
    PRE         1
2N 模式：
    Command     Slot 数量
    ------------------------
    ACT         4
    WR          4
    WRA         4
    RD          4
    RDA         4

    REFab       2
    REFSb       2
    PREab       2
    PRESb       2
    PRE         2

六、Command 插入规则
------------------------------------------------------------------------
Command 通过以下接口加入：
    AddCommand(Command cmd, tlm_generic_payload* trans)
插入流程：
1. 根据 command 类型和 DFI mode 计算 slot 占用数量
2. 在 trans_queue 中分配连续 slot
3. 仅在最后一个 slot 中记录 transaction

示例：
    frequency_ratio = 4
    1N 模式
加入 WR command:
WR 占用 2 个 slot
slot 表示：
    slot0   slot1   slot2   slot3
    WR

实际存储：
    slot0 : empty
    slot1 : { is_cmd_end = true, trans = WR_payload }
------------------------------------------------------------------------
七、容量限制
------------------------------------------------------------------------
trans_queue 的最大 slot 数量为：
    frequency_ratio
所有 command 占用 slot 总数不能超过该值。
示例：
    frequency_ratio = 4
合法组合：
    1 + 1 + 1 + 1
    2 + 2
    2 + 1 + 1
    4
非法组合：
    2 + 2 + 1

八、跨周期 Command 处理
------------------------------------------------------------------------
如果一个 command 所需要的 slot 数量超过当前剩余 slot,
则该 command 不能加入当前 DfiExtension。
该 command 必须加入下一个 DFI cycle 的 DfiExtension。
示例：
    frequency_ratio = 4
当前状态：
    slot0 slot1 slot2 slot3
    CMD1  CMD1  CMD2  CMD2
剩余 slot:
    0
此时任何新 command 都必须进入下一周期。

九、frequency_ratio = 8 时的奇偶 Slot 约束
------------------------------------------------------------------------
当 frequency_ratio == 8 时，slot 被划分为两个独立 lane:
    even lane : 0 2 4 6
    odd lane  : 1 3 5 7
AddCommand 接口需要提供 lane 选择：
    AddCommand(Command cmd,
               tlm_generic_payload* trans,
               bool use_even_lane)
规则：
use_even_lane = true
    只允许使用 slot:
        0 2 4 6
use_even_lane = false
    只允许使用 slot:
        1 3 5 7

十、Lane 独立性
------------------------------------------------------------------------
奇数 lane 与偶数 lane 互不影响。
示例：
    slot index:
        0 1 2 3 4 5 6 7
当前状态：
        - X - X - X - X
表示：
    odd lane 已满
    even lane 仍可使用
此时仍然允许向 even lane 插入 command。

十一、Command 读取规则
------------------------------------------------------------------------
Command 按 slot 顺序读取。
接口：
    tlm_generic_payload* NextCommand()
读取流程：
1. 从 slot0 开始遍历
2. 遇到 is_cmd_end == true
3. 返回对应 transaction

十二、Reset 行为
------------------------------------------------------------------------
调用 reset() 时需要执行：
    1. 清空 trans_queue
    2. 重置 slot 状态
    3. 重置读取指针

十三、Transaction 生命周期
------------------------------------------------------------------------
DfiExtension 不负责管理 tlm_generic_payload 的生命周期。该类仅保存外部 transaction 的指针引用。
transaction 的创建与释放应由外部模块负责。
========================================================================
*/

struct DfiSlot
{
    bool is_cmd_end = false;
    tlm::tlm_generic_payload* trans = nullptr;
    DfiSlot():is_cmd_end(false),trans(nullptr){}
};

class DfiCmdExtension : public tlm::tlm_extension<DfiCmdExtension>
{
public:
    DfiCmdExtension(unsigned ratio, bool mode_2n)
    : frequency_ratio(ratio),
      is_2n_mode(mode_2n),
      MAX_EVEN_SLOT_NUM(ratio/2),
      MAX_ODD_SLOT_NUM(ratio/2),
      slots(ratio)
    {
        assert((frequency_ratio >= 4 && frequency_ratio%2==0) && "frequency_ratio must >= 4");
    }

    // 禁止clone
    virtual tlm_extension_base* clone() const override{
        SC_REPORT_FATAL("DFI_EXTENSION", "Clone not supported");
        return nullptr;
    }

    // 禁止copy
    virtual void copy_from(const tlm_extension_base &) override{
        SC_REPORT_FATAL("DFI_EXTENSION", "Copy not supported");
    }

public:

    void reset()
    {
        used_units_even = 0;
        used_units_odd = 0;
        read_index = 0;
        for(auto &s : slots)
        {
            s.is_cmd_end = false;
            s.trans = nullptr;
        }
    }

    bool AddTrans(tlm::tlm_generic_payload* trans, unsigned cmd_occupied_phase_num){
        unsigned unit = cmd_occupied_phase_num;
        if(this->GetUsedUnits() + unit > frequency_ratio)
            return false;
        unsigned end = this->GetUsedUnits() + unit - 1;
        slots[end].is_cmd_end = true;
        slots[end].trans = trans;
        
        used_units_even += (unit - unit % 2)/2 + unit % 2;
        used_units_odd += (unit - unit % 2)/2;
        return true;
    }

    bool AddTrans(tlm::tlm_generic_payload* trans, unsigned cmd_occupied_phase_num, bool use_even_lane){
        unsigned unit = cmd_occupied_phase_num;
        if(use_even_lane){
            if(used_units_even + unit > MAX_EVEN_SLOT_NUM)
                return false;
            unsigned units_base_even = 0;
            unsigned end_even = units_base_even + (used_units_even + unit -1) * 2;
            
            slots[end_even].is_cmd_end = true;
            slots[end_even].trans = trans;
            used_units_even += unit; // 添加这行
            return true;
        }
        else{
            if(used_units_odd + unit > MAX_ODD_SLOT_NUM)
                return false;
            unsigned units_base_odd = 1;
            unsigned end_odd = units_base_odd + (used_units_odd + unit -1) * 2;
            
            slots[end_odd].is_cmd_end = true;
            slots[end_odd].trans = trans;
            used_units_odd += unit; // 添加这行
            return true;
        }
    }

    bool AddLastCycleTrans(tlm::tlm_generic_payload* trans, unsigned remaining_phase_num){
        return AddTrans(trans, remaining_phase_num);
    }

    bool AddLastCycleTrans(tlm::tlm_generic_payload* trans, unsigned remaining_phase_num, bool use_even_lane){
        return AddTrans(trans, remaining_phase_num, use_even_lane);
    }

    bool HasNextCommand() const
    {
        for(unsigned i = read_index; i < frequency_ratio; i++)
        {
            if(slots[i].is_cmd_end)
                return true;
        }
        return false;
    }

    bool HasCommand() const
    {
        for(unsigned i = 0; i < frequency_ratio; i++)
        {
            if(slots[i].is_cmd_end || slots[i].trans != nullptr)
                return true;
        }
        return false;
    }

    tlm::tlm_generic_payload* GetNextCommand()
    {
        while(read_index < frequency_ratio)
        {
            if(slots[read_index].is_cmd_end)
            {
                auto *t = slots[read_index].trans;
                read_index++;
                return t;
            }
            read_index++;
        }
        return nullptr;
    }

    bool IsFull() const
    {
        return this->GetUsedUnits() == frequency_ratio;
    }

    bool IsOddFull() const{
        return used_units_odd == MAX_ODD_SLOT_NUM;
    }

    bool IsEvenFull() const{
        return used_units_even == MAX_EVEN_SLOT_NUM;
    }

    unsigned RemainingUnits() const
    {
        return frequency_ratio - this->GetUsedUnits();
    }

    unsigned RemainingUnitsOdd() const{
        return MAX_ODD_SLOT_NUM - used_units_odd;
    }

    unsigned RemainingUnitsEven() const{
        return MAX_EVEN_SLOT_NUM - used_units_even;
    }

    unsigned GetUsedUnits() const
    {
        return used_units_odd + used_units_even;
    }

private:

    const unsigned frequency_ratio;
    const bool is_2n_mode;
    
    const unsigned MAX_ODD_SLOT_NUM;
    const unsigned MAX_EVEN_SLOT_NUM;

    // unsigned used_units = 0;
    unsigned used_units_odd = 0;
    unsigned used_units_even = 0;
    // unsigned used_units_total = 0;

    unsigned read_index = 0;

    std::vector<DfiSlot> slots;

};
    }
}

#endif