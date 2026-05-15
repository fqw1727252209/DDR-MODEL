#ifndef __SDRAM_CONSTRAINT_HH__
#define __SDRAM_CONSTRAINT_HH__
#include <vector>
#include <deque>

#include "Configure/DDR5MemSpec3ds.hh"
#include "Controller/BankSlice.hh"

namespace dmu{
    namespace Controller{

using BankIndex = unsigned;
using BankGroupIndex= unsigned;
using LrankIndex = unsigned;
using PrankIndex = unsigned;
using ChannelIndex = unsigned;

class SdramConstraintIF
{
    protected:
        SdramConstraintIF(const SdramConstraintIF&) =default;
        SdramConstraintIF(SdramConstraintIF&&) = default;
        SdramConstraintIF& operator=(const SdramConstraintIF&) = default;
        SdramConstraintIF& operator=(SdramConstraintIF&&) = default;
    public:
        SdramConstraintIF() = default;
        virtual ~SdramConstraintIF() = default;
        virtual sc_core::sc_time TimeToSatisfyConstraints(Command command,BankAddress address) const = 0;
        virtual void InsertCommand(Command command,BankAddress address) = 0;
};


class SdramConstraintDDR5_3ds: public SdramConstraintIF
{

    public:
        explicit SdramConstraintDDR5_3ds(const DDR5MemSpec3ds& ddr5_memspec_3ds);
        ~SdramConstraintDDR5_3ds(){}
        sc_core::sc_time TimeToSatisfyConstraints(Command command,BankAddress address) const override;
        void InsertCommand(Command command,BankAddress address) override;
        std::vector<BankIndex> GetSameBgBankVec(BankAddress address) const;
        std::vector<BankGroupIndex> GetSameBgVec(BankAddress address) const;

    private:
        const DDR5MemSpec3ds& _ddr5_memspec_3ds;
        const bool is_x4_device;
        const sc_core::sc_time MaxTime = sc_core::sc_max_time();

        /*
        一个SdramConstraint 需要服务2个sub-channel，但是两个sub-channel之间暂时认为彼此相互独立
        每个sub-channels的pseudo-channel数量为1或者2，对应于（rdimm模式/mrdimm rank mode 和 mrdimm mux mode）
        同一sub-channel的pseudo-channel之间的timing彼此互不干扰，暂时认为相互独立
        每个pseudo-channel的timing 需要维护当前内部的 
        channel_timing -> sub-channel-timing -> pseudo-channel-timing
        
        因为要兼容rdimm的3ds模式
        所以内部需要定义清楚 p-rank和l-rank的时序交互的约束

        */
        struct PseudoChannelTiming{
            std::vector<std::vector<sc_core::sc_time>> previous_command_time4bank; // (real-banks,command)
            std::vector<std::vector<sc_core::sc_time>> previous_command_time4bg; // (real-bg, command)
            std::vector<std::vector<sc_core::sc_time>> previous_command_time4lrank; // (real-lranks, command)
            std::vector<std::vector<sc_core::sc_time>> previous_command_time4prank; // (real-pranks, command)
            // real xxx index indicates the real xxx in the pseudo-channel
            std::vector<std::vector<std::vector<sc_core::sc_time>>> previous_command_time4prank_lrank; //(pranks, lrank-index, command), record
            
            std::vector<sc_core::sc_time> previous_command_time4pseudo_channel; // (command)

            std::vector<std::deque<sc_core::sc_time>> last_4Activates4lrank; // (real-lranks)
            
            std::vector<std::deque<sc_core::sc_time>> last_4Activates4prank; // (real-pranks)

            sc_core::sc_time previous_command_time4pseudo_channel_onbus;

        };

        std::vector<std::vector<PseudoChannelTiming>> channel_timing; // 1-dimension: sub-channels,2-dimension: pseudo-channels, element: PseudoChannelTiming
// std::vector<std::deque<sc_core::sc_time>> last_Activates4dimm_subchannel; // 32 ACT that in 128 DDR Cycles;
// std::vector<std::vector<std::vector<sc_core::sc_time>>> previous_command_time4pseudo_channel_prank; //( pranks-index, command)
};

    } // Controller
} // dmu


#endif