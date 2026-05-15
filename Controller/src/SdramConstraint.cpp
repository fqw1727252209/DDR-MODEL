#include <algorithm>

#include <cassert>
#include <systemc>
#include <vector>

#include "Controller/SdramConstraint.hh"
#include "Common/CommonDefine.hh"
#include "Configure/DDR5MemSpec3ds.hh"
#include "Controller/common/Command.hh"
#include "sysc/kernel/sc_time.h"

namespace dmu{
    namespace Controller{

using namespace sc_core;

SdramConstraintDDR5_3ds::SdramConstraintDDR5_3ds(const DDR5MemSpec3ds& ddr5_memspec_3ds)
: _ddr5_memspec_3ds(ddr5_memspec_3ds)
, is_x4_device(_ddr5_memspec_3ds.DeviceWidth == 4)
{
    // 初始化包含各个子通道中伪通道的时序相关向量
    channel_timing = std::vector<std::vector<PseudoChannelTiming>>(_ddr5_memspec_3ds.NumOfSubChannelsPerChannel,
                    std::vector<PseudoChannelTiming>(_ddr5_memspec_3ds.NumOfPseudoChannelsPerSubChannel));
    for(auto& sub_channel_timing : channel_timing) {
        for(auto& pch_timing : sub_channel_timing) {
            // 初始化各个时间记录向量
            pch_timing.previous_command_time4bank = std::vector<std::vector<sc_time>>(Command::NumOfCommands(),
                                                    std::vector<sc_time>(_ddr5_memspec_3ds.NumOfBanksPerPseudoChannel, MaxTime));

            pch_timing.previous_command_time4bg = std::vector<std::vector<sc_time>>(Command::NumOfCommands(),
                                                    std::vector<sc_time>(_ddr5_memspec_3ds.NumOfBankGroupsPerPseudoChannel, MaxTime));

            pch_timing.previous_command_time4lrank = std::vector<std::vector<sc_time>>(Command::NumOfCommands(),
                                                    std::vector<sc_time>(_ddr5_memspec_3ds.NumOfLogicalRanksPerPseudoChannel, MaxTime));

            pch_timing.previous_command_time4prank = std::vector<std::vector<sc_time>>(Command::NumOfCommands(),
                                                    std::vector<sc_time>(_ddr5_memspec_3ds.NumOfPhysicalRanksPerPseudoChannel, MaxTime));

            pch_timing.previous_command_time4prank_lrank = std::vector<std::vector<std::vector<sc_time>>>(Command::NumOfCommands(),
                                                    std::vector<std::vector<sc_time>>(_ddr5_memspec_3ds.NumOfPhysicalRanksPerPseudoChannel,
                                                    std::vector<sc_time>(_ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank, MaxTime)
                                                    )
            );

            pch_timing.previous_command_time4pseudo_channel = std::vector<sc_time>(Command::NumOfCommands());

            // 初始化激活队列
            pch_timing.last_4Activates4lrank = std::vector<std::deque<sc_time>>(_ddr5_memspec_3ds.NumOfLogicalRanksPerPseudoChannel, std::deque<sc_time>(4, MaxTime));
            pch_timing.last_4Activates4prank = std::vector<std::deque<sc_time>>(_ddr5_memspec_3ds.NumOfPhysicalRanksPerPseudoChannel, std::deque<sc_time>(4, MaxTime));

            // 初始化其他成员
            pch_timing.previous_command_time4pseudo_channel_onbus = MaxTime;
            // pch_timing.last_Activates4dimm_channel = std::vector<std::deque<sc_time>>(_ddr5_memspec_3ds.NumOfDimmsPerChannel, std::deque<sc_time>());
        }
    }
}

sc_core::sc_time
SdramConstraintDDR5_3ds::TimeToSatisfyConstraints(Command command, BankAddress address) const
{

    BankIndex command_bank = address.real_ba;
    BankGroupIndex command_bg = address.real_bg;
    LrankIndex command_lrank = address.real_cid;

    PrankIndex command_prank = address.cs;
    ChannelIndex command_p_ch = address.pseudo_ch;
    ChannelIndex command_s_ch = address.sub_ch;
    ChannelIndex command_ch = address.ch;

    LrankIndex command_lrank_on_pranks = address.cid;
    PrankIndex command_prank_on_pseudo_channel = address.cs;

    // 先确定获得哪个Pseudo channel的timing, 根据sub-channel和pseudo-channel的index 进行明确

    DPRINT_ASSERT(command_bank < _ddr5_memspec_3ds.NumOfBanksPerPseudoChannel, "SDRAMConstraint", "command_bank: %d", command_bank);
    DPRINT_ASSERT(command_bg < _ddr5_memspec_3ds.NumOfBankGroupsPerPseudoChannel, "SDRAMConstraint", "command_bg: %d", command_bg);
    DPRINT_ASSERT(command_lrank < _ddr5_memspec_3ds.NumOfLogicalRanksPerPseudoChannel, "SDRAMConstraint", "command_lrank: %d", command_lrank);
    DPRINT_ASSERT(command_prank < _ddr5_memspec_3ds.NumOfPhysicalRanksPerPseudoChannel, "SDRAMConstraint", "command_prank: %d", command_prank);
    DPRINT_ASSERT(command_p_ch < _ddr5_memspec_3ds.NumOfPseudoChannelsPerSubChannel, "SDRAMConstraint", "command_p_ch: %d", command_p_ch);
    DPRINT_ASSERT(command_s_ch < _ddr5_memspec_3ds.NumOfSubChannelsPerChannel, "SDRAMConstraint", "command_sub_ch: %d", command_s_ch);

    auto& pseudo_channel_timing = channel_timing[command_s_ch][command_p_ch];

    auto& previous_command_time4bank = pseudo_channel_timing.previous_command_time4bank;
    auto& previous_command_time4bg = pseudo_channel_timing.previous_command_time4bg;
    auto& previous_command_time4lrank = pseudo_channel_timing.previous_command_time4lrank;
    auto& previous_command_time4prank = pseudo_channel_timing.previous_command_time4prank;
    auto& previous_command_time4prank_lrank = pseudo_channel_timing.previous_command_time4prank_lrank;
    auto& previous_command_time4pseudo_channel = pseudo_channel_timing.previous_command_time4pseudo_channel;
    auto& last_4Activates4lrank = pseudo_channel_timing.last_4Activates4lrank;
    auto& last_4Activates4prank = pseudo_channel_timing.last_4Activates4prank;
    auto& previous_command_time4pseudo_channel_onbus = pseudo_channel_timing.previous_command_time4pseudo_channel_onbus;

    sc_time previous_cmd_record_time;
    sc_time command_avail_time = sc_time_stamp();

    if(command == Command::RFMab) ;
    if(command == Command::REFab) ;
    if(command == Command::RFMsb) ;
    if(command == Command::REFsb) ;

    if (command == Command::ACT)
    {
        previous_cmd_record_time = previous_command_time4bank[Command::PRE][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRP_mc);
        }

        previous_cmd_record_time = previous_command_time4bank[Command::PREsb][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRP_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::PREab][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRP_mc);
        }

        previous_cmd_record_time = previous_command_time4bank[Command::ACT][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time +
                                        _ddr5_memspec_3ds.tRCD_mc + _ddr5_memspec_3ds.tRASmin_mc);
        }

        previous_cmd_record_time = previous_command_time4bg[Command::ACT][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_L_slr_mc);
        //  assert((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::ACT][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_S_slr_mc);
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
        {
            if(i == command_lrank_on_pranks)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::ACT][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_dlr_mc);
            }
        }

        if(last_4Activates4lrank[command_lrank].size()>=4)
        {
            previous_cmd_record_time = last_4Activates4lrank[command_lrank].front();
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tFAW_slr_mc);
            }
            assert((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
        }

        if(last_4Activates4prank[command_prank].size()>=4)
        {
            previous_cmd_record_time = last_4Activates4prank[command_prank].front();
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, last_4Activates4prank[command_prank].front()
                                            + _ddr5_memspec_3ds.tFAW_dlr_mc);
            }
            assert((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::REFab][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_slr_mc);
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
        {
            if(i == command_lrank_on_pranks)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::REFab][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dlr_mc);
            }
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfPhysicalRanksPerPseudoChannel; i++)
        {
            if(i == command_prank_on_pseudo_channel)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank[Command::REFab][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dpr_mc);
            }
        }

        previous_cmd_record_time = previous_command_time4bank[Command::REFsb][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFCsb_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4bg[Command::REFsb][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tREFSBRD_slr_mc);
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
        {
            if(i == command_lrank_on_pranks)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::REFsb][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tREFSBRD_dlr_mc);
            }
        }

        previous_cmd_record_time = previous_command_time4bank[Command::RDA][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time +
                                        _ddr5_memspec_3ds.tRTP_slr_mc + _ddr5_memspec_3ds.tRP_mc);
        //  assert((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
        }

        previous_cmd_record_time = previous_command_time4bank[Command::WRA][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time +
                                        _ddr5_memspec_3ds.tCWL_mc + _ddr5_memspec_3ds.tBurst_mc + _ddr5_memspec_3ds.tWR_slr_mc + _ddr5_memspec_3ds.tRP_mc);
        }
    }

    else if(command == Command::PRE)
    {
        previous_cmd_record_time = previous_command_time4bg[Command::PRE][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::PRE][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4prank[Command::PRE][command_prank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_dlr_mc);
        }

        previous_cmd_record_time = previous_command_time4bg[Command::PREsb][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4prank[Command::PREsb][command_prank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_dlr_mc);
        }

        previous_cmd_record_time = previous_command_time4prank[Command::PREab][command_prank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_dlr_mc);
        }

        previous_cmd_record_time = previous_command_time4bank[Command::ACT][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRASmin_mc);
        }

        previous_cmd_record_time = previous_command_time4bank[Command::RD][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRTP_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4bank[Command::WR][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tWR_slr_mc);
        }
    }
    else if(command == Command::PREsb)
    {
        previous_cmd_record_time = previous_command_time4lrank[Command::PRE][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4prank[Command::PRE][command_prank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_dlr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::PREsb][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4prank[Command::PREsb][command_prank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_dlr_mc);
        }

        previous_cmd_record_time = previous_command_time4prank[Command::PREab][command_prank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_dlr_mc);
        }

        for(const auto& bank_sbg: this->GetSameBgBankVec(address))
        {
            previous_cmd_record_time = previous_command_time4bank[Command::ACT][bank_sbg];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRASmin_mc);
            }

            previous_cmd_record_time = previous_command_time4bank[Command::RD][bank_sbg];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRTP_slr_mc);
            }

            previous_cmd_record_time = previous_command_time4bank[Command::WR][bank_sbg];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tWR_slr_mc);
            }
        }
    }
    else if(command == Command::PREab)
    {
        previous_cmd_record_time = previous_command_time4lrank[Command::PRE][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4prank[Command::PRE][command_prank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_dlr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::PREsb][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4prank[Command::PREsb][command_prank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_dlr_mc);
        }

        previous_cmd_record_time = previous_command_time4prank[Command::PREab][command_prank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tPPD_dlr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::ACT][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRASmin_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::RD][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRTP_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::WR][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tWR_slr_mc);
        }
    }
    else if(command == Command::REFab)
    {
        previous_cmd_record_time = previous_command_time4lrank[Command::PRE][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRP_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::PREab][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRP_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::PREsb][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRP_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::REFsb][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFCsb_slr_mc);
        }

        for (int i=0; i<_ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
        {
            if(i == command_lrank_on_pranks)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::REFsb][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFCsb_dlr_mc);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::REFab][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dlr_mc);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::ACT][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_dlr_mc);
            }
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::REFab][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_slr_mc);
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfPhysicalRanksPerPseudoChannel; i++)
        {
            if(i == command_prank_on_pseudo_channel)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank[Command::REFab][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dpr_mc);
            }
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::RDA][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRTP_slr_mc
                                        + _ddr5_memspec_3ds.tRP_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::WRA][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCWL_mc
                                        + _ddr5_memspec_3ds.tBurst_mc + _ddr5_memspec_3ds.tWR_slr_mc + _ddr5_memspec_3ds.tRP_mc);
        }

        if(last_4Activates4prank[command_prank].size()>=4)
        {
            previous_cmd_record_time = last_4Activates4prank[command_prank].front();
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, last_4Activates4prank[command_prank].front()
                                            + _ddr5_memspec_3ds.tFAW_dlr_mc);
            }
        }
    }
    else if(command == Command::REFsb)
    {
        previous_cmd_record_time = previous_command_time4lrank[Command::REFab][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_slr_mc);
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
        {
            if(i == command_lrank_on_pranks)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::REFsb][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFCsb_dlr_mc);
            }

            previous_cmd_record_time = previous_command_time4prank_lrank[Command::REFab][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dlr_mc);
            }

            previous_cmd_record_time = previous_command_time4prank_lrank[Command::ACT][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_dlr_mc);
            }
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfPhysicalRanksPerPseudoChannel; i++)
        {
            if(i == command_prank_on_pseudo_channel)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank[Command::REFab][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dpr_mc);
            }
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::REFsb][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFCsb_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::ACT][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_L_slr_mc);
        }

        for(const auto& ba: this->GetSameBgBankVec(address))
        {
            previous_cmd_record_time = previous_command_time4bank[Command::RDA][ba];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRTP_slr_mc
                                            + _ddr5_memspec_3ds.tRP_mc);
            }

            previous_cmd_record_time = previous_command_time4bank[Command::WRA][ba];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCWL_mc
                                            + _ddr5_memspec_3ds.tBurst_mc + _ddr5_memspec_3ds.tWR_slr_mc + _ddr5_memspec_3ds.tRP_mc);
            }
        }

        if(last_4Activates4lrank[command_lrank].size()>=4)
        {
            previous_cmd_record_time = last_4Activates4lrank[command_lrank].front();
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, last_4Activates4lrank[command_lrank].front()
                                            + _ddr5_memspec_3ds.tFAW_slr_mc);
            }
        }

        if(last_4Activates4prank[command_prank].size()>=4)
        {
            previous_cmd_record_time = last_4Activates4prank[command_prank].front();
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, last_4Activates4prank[command_prank].front()
                                            + _ddr5_memspec_3ds.tFAW_dlr_mc);
            }
        }
    }
    else if(command == Command::RD || command == Command::RDA)
    {
        previous_cmd_record_time = previous_command_time4bank[Command::ACT][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRCD_mc);
        }

        previous_cmd_record_time = previous_command_time4bg[Command::RD][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_L_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::RD][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_S_slr_mc);
        }

        // same sub-channel, though has no time constraint, should also limited to tburst length
        previous_cmd_record_time = previous_command_time4pseudo_channel[Command::RD];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
        }
        previous_cmd_record_time = previous_command_time4pseudo_channel[Command::RDA];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
        }
        previous_cmd_record_time = previous_command_time4pseudo_channel[Command::WR];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
        }
        previous_cmd_record_time = previous_command_time4pseudo_channel[Command::WRA];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
        }

        previous_cmd_record_time = previous_command_time4bg[Command::WR][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_L_WTR_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::WR][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_S_WTR_slr_mc);
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfPhysicalRanksPerPseudoChannel; i++)
        {
            if(i == command_prank_on_pseudo_channel)
            {
                continue;
            }
            // physical rank delay switch
            previous_cmd_record_time = previous_command_time4prank[Command::WR][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Wr2RdPrank);
            }
            previous_cmd_record_time = previous_command_time4prank[Command::WRA][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Wr2RdPrank);
            }
            previous_cmd_record_time = previous_command_time4prank[Command::RD][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Rd2RdPrank);
            }
            previous_cmd_record_time = previous_command_time4prank[Command::RDA][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Rd2RdPrank);
            }
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
        {
            if(i == command_lrank_on_pranks)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::RD][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_dlr_mc);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::WR][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WTR_dlr_mc);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::RDA][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_dlr_mc);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::WRA][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WTR_dlr_mc);
            }

            // logical rank switch delay
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::RD][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Rd2WrLrank);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::WR][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Wr2WrLrank);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::RDA][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Rd2WrLrank);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::WRA][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Wr2WrLrank);
            }
        }

        previous_cmd_record_time = previous_command_time4bg[Command::RD][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_L_RTW_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::RD][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_S_RTW_slr_mc);
        }
    }
    else if(command == Command::WR || command == Command::WRA)
    {
        previous_cmd_record_time = previous_command_time4bank[Command::ACT][command_bank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRCD_mc);
        }

        previous_cmd_record_time = previous_command_time4bg[Command::WR][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time +
                                        (is_x4_device ? _ddr5_memspec_3ds.tCCD_L_WR_slr_mc : _ddr5_memspec_3ds.tCCD_L_WR2_slr_mc));
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::WR][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_S_WR_slr_mc);
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfPhysicalRanksPerPseudoChannel; i++)
        {
            if(i == command_prank_on_pseudo_channel)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank[Command::WR][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dpr_mc);
            }
            previous_cmd_record_time = previous_command_time4prank[Command::WRA][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dpr_mc);
            }
            // physical rank switch delay
            previous_cmd_record_time = previous_command_time4prank[Command::WR][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Wr2WrPrank);
            }
            previous_cmd_record_time = previous_command_time4prank[Command::WRA][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Wr2WrPrank);
            }
            previous_cmd_record_time = previous_command_time4prank[Command::RD][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Rd2WrPrank);
            }
            previous_cmd_record_time = previous_command_time4prank[Command::RDA][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Rd2WrPrank);
            }
        }

        for(int i=0; i<_ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
        {
            if(i == command_lrank_on_pranks)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::RD][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_RTW_dlr_mc);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::WR][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dlr_mc);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::RDA][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_RTW_dlr_mc);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::WRA][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dlr_mc);
            }

            // logical rank switch delay
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::RD][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Rd2WrLrank);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::WR][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Wr2WrLrank);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::RDA][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Rd2WrLrank);
            }
            previous_cmd_record_time = previous_command_time4prank_lrank[Command::WRA][command_prank][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.Wr2WrLrank);
            }
        }

        previous_cmd_record_time = previous_command_time4bg[Command::RD][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_L_RTW_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::RD][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_S_RTW_slr_mc);
        }

        // same sub-channel, though has no time constraint, should also limited to tburst length
        previous_cmd_record_time = previous_command_time4pseudo_channel[Command::RD];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
        }
        previous_cmd_record_time = previous_command_time4pseudo_channel[Command::RDA];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
        }
        previous_cmd_record_time = previous_command_time4pseudo_channel[Command::WR];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
        }
        previous_cmd_record_time = previous_command_time4pseudo_channel[Command::WRA];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
        }

        previous_cmd_record_time = previous_command_time4bg[Command::WRA][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time +
                                        (is_x4_device ? _ddr5_memspec_3ds.tCCD_L_WR_slr_mc : _ddr5_memspec_3ds.tCCD_L_WR2_slr_mc));
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::WRA][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_S_WR_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4bg[Command::RDA][command_bg];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_L_RTW_slr_mc);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::RDA][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_S_RTW_slr_mc);
        }
    }
    else
    {
        SC_REPORT_ERROR("SdramConstraint", "Unknown command!");
    }

    if(previous_command_time4pseudo_channel_onbus != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_command_time4pseudo_channel_onbus + _ddr5_memspec_3ds.tCK_mc);
    }
    if(command_avail_time != MaxTime)
    {
        // assert((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
        DPRINT_ASSERT((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME, "SDRAM constraint", "%s command_avail_time:%s",command.to_string().c_str(),command_avail_time.to_string().c_str());
    }
    return command_avail_time;
}

void
SdramConstraintDDR5_3ds::InsertCommand(Command command, BankAddress address)
{
    BankIndex command_bank = address.real_ba;
    BankGroupIndex command_bg = address.real_bg;
    LrankIndex command_lrank = address.real_cid;

    PrankIndex command_prank = address.cs;
    ChannelIndex command_p_ch = address.pseudo_ch;
    ChannelIndex command_s_ch = address.sub_ch;
    ChannelIndex command_ch = address.ch;

    LrankIndex command_lrank_on_pranks = address.cid;
    PrankIndex command_prank_on_pseudo_channel = address.cs;

    // 先确定获得哪个Pseudo channel的timing, 根据sub-channel和pseudo-channel的index 进行明确
    auto& pseudo_channel_timing = channel_timing[command_s_ch][command_p_ch];

    auto& previous_command_time4bank = pseudo_channel_timing.previous_command_time4bank;
    auto& previous_command_time4bg = pseudo_channel_timing.previous_command_time4bg;
    auto& previous_command_time4lrank = pseudo_channel_timing.previous_command_time4lrank;
    auto& previous_command_time4prank = pseudo_channel_timing.previous_command_time4prank;
    auto& previous_command_time4prank_lrank = pseudo_channel_timing.previous_command_time4prank_lrank;
    auto& previous_command_time4pseudo_channel = pseudo_channel_timing.previous_command_time4pseudo_channel;
    auto& last_4Activates4lrank = pseudo_channel_timing.last_4Activates4lrank;
    auto& last_4Activates4prank = pseudo_channel_timing.last_4Activates4prank;
    auto& previous_command_time4pseudo_channel_onbus = pseudo_channel_timing.previous_command_time4pseudo_channel_onbus;

    if(command == Command::RFMab)
        command = Command::REFab;
    if(command == Command::RFMsb)
        command = Command::REFsb;

    sc_time record_time = sc_time_stamp();
    assert((record_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
    previous_command_time4lrank[command][command_lrank] = record_time;
    previous_command_time4prank[command][command_prank] = record_time;
    previous_command_time4pseudo_channel[command] = record_time;

    previous_command_time4prank_lrank[command][command_prank][command_lrank_on_pranks] = record_time;

    previous_command_time4pseudo_channel_onbus = record_time;

    if(command == Command::REFsb || command == Command::PREsb || command == Command::RFMsb)
    {
        for(const auto& ba: this->GetSameBgBankVec(address))
        {
            previous_command_time4bank[command][ba] = record_time;
        }
        for(const auto& bg: this->GetSameBgVec(address))
        {
            previous_command_time4bg[command][bg] = record_time;
        }
    }
    else
    {
        previous_command_time4bank[command][command_bank] = record_time;
        previous_command_time4bg[command][command_bg] = record_time;
    }

    if (command == Command::ACT || command == Command::REFsb || command == Command::RFMsb)
    {
        if (last_4Activates4lrank[command_lrank].size() == 4)
        {
            last_4Activates4lrank[command_lrank].pop_front();
        }
        last_4Activates4lrank[command_lrank].push_back(record_time);

        if (last_4Activates4prank[command_prank].size() == 4)
        {
            last_4Activates4prank[command_prank].pop_front();
        }
        last_4Activates4prank[command_prank].push_back(record_time);
    }
    if (command == Command::REFab || command == Command::RFMab)
    {
        if (last_4Activates4prank[command_prank].size() == 4)
        {
            last_4Activates4prank[command_prank].pop_front();
        }
        last_4Activates4prank[command_prank].push_back(record_time);
    }
    assert(last_4Activates4prank[command_prank].size() <= 4 && last_4Activates4lrank[command_lrank].size() <= 4);
}

std::vector<BankIndex>
SdramConstraintDDR5_3ds::GetSameBgBankVec(BankAddress address) const
{
    // 每个bankgroup中有多个bank，该函数获得1个logical rank中，不同bank group中相同bank 偏移的所有bank
    // 例：
    // bg 0: 0,1,2,3; bg 1: 4,5,6,7; bg 2: 8,9,10,11; bg 3: 12,13,14,15
    // 获得该logical rank下的 1, 5, 9, 13 号bank
    std::vector<BankIndex> same_ba_vec;
    BankIndex num_bank_per_bg = _ddr5_memspec_3ds.NumOfBanksPerBankGroup;
    BankGroupIndex num_bg_per_lrank = _ddr5_memspec_3ds.NumOfBankGroupsPerLogicalRank;
    BankIndex num_bank_per_lrank = _ddr5_memspec_3ds.NumOfBanksPerLogicalRank;

    BankIndex ba_base = address.real_ba - address.real_ba % num_bank_per_lrank;
    BankIndex ba_offset = address.real_ba % num_bank_per_bg;

    for(unsigned i = 0; i < num_bg_per_lrank; i++)
    {
        same_ba_vec.push_back(ba_base + ba_offset + i * num_bank_per_bg);
    }
    return same_ba_vec;
}

std::vector<BankGroupIndex>
SdramConstraintDDR5_3ds::GetSameBgVec(BankAddress address) const
{
    std::vector<BankGroupIndex> same_bg_vec;

    BankGroupIndex num_bg_per_lrank = _ddr5_memspec_3ds.NumOfBankGroupsPerLogicalRank;
    BankGroupIndex bg_base = address.real_bg - address.real_bg % num_bg_per_lrank;

    for(unsigned i = 0; i < num_bg_per_lrank; i++)
    {
        same_bg_vec.push_back(bg_base + i);
    }
    return same_bg_vec;
}

    } // Controller
} // dmu