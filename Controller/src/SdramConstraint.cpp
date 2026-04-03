#include <algorithm>

#include <cassert>
#include <systemc>

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
, is_x4_device(_ddr5_memspec_3ds.width == 4)
{

    previous_command_time4bank = std::vector<std::vector<sc_time>>(Command::NumOfCommands(),
                                 std::vector<sc_time>(_ddr5_memspec_3ds.NumOfTotalBanks,MaxTime));

    previous_command_time4bg = std::vector<std::vector<sc_time>>(Command::NumOfCommands(),
                               std::vector<sc_time>(_ddr5_memspec_3ds.NumOfTotalBankGroups,MaxTime));

    previous_command_time4lrank = std::vector<std::vector<sc_time>>(Command::NumOfCommands(),
                                  std::vector<sc_time>(_ddr5_memspec_3ds.TotalNumOfLogicalRanks,MaxTime));

    previous_command_time4prank = std::vector<std::vector<sc_time>>(Command::NumOfCommands(),
                                  std::vector<sc_time>(_ddr5_memspec_3ds.NumOfPhysicalRanksPerChannel,MaxTime));

    previous_command_time4prank_lrank = std::vector<std::vector<std::vector<sc_time>>>(Command::NumOfCommands(),
                                        std::vector<std::vector<sc_time>>(_ddr5_memspec_3ds.NumOfPhysicalRanksPerChannel,
                                        std::vector<sc_time>(_ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank,MaxTime))
                                        );

    previous_command_time4channel_prank = std::vector<std::vector<std::vector<sc_time>>>(Command::NumOfCommands(),
                                          std::vector<std::vector<sc_time>>(_ddr5_memspec_3ds.NumOfChannels,
                                          std::vector<sc_time>(_ddr5_memspec_3ds.NumOfPhysicalRanksPerChannel,MaxTime))
                                          );


    previous_command_time4channel = std::vector<std::vector<sc_core::sc_time>>(Command::NumOfCommands(),
                                    std::vector<sc_time>(_ddr5_memspec_3ds.NumOfSubChannels,MaxTime));

    previous_command_time4channel_onbus = std::vector<sc_core::sc_time>(_ddr5_memspec_3ds.NumOfSubChannels,MaxTime);

    // 修正初始化方式，确保容器正确初始化
    last_4Activates4lrank = std::vector<std::deque<sc_time>>(_ddr5_memspec_3ds.TotalNumOfLogicalRanks,std::deque(4,MaxTime));
    last_4Activates4prank = std::vector<std::deque<sc_time>>(_ddr5_memspec_3ds.NumOfPhysicalRanksPerChannel,std::deque(4,MaxTime));

}



sc_core::sc_time
SdramConstraintDDR5_3ds::TimeToSatisfyConstraints(Command command,BankAddress address) const
{
    BankIndex command_bank = address.real_ba;
    BankGroupIndex command_bg = address.real_bg;
    LrankIndex command_lrank = address.real_cid;
    PrankIndex command_prank = address.cs;
    ChannelIndex command_ch = address.ch;

    LrankIndex command_lrank_on_pranks = address.cid;
    PrankIndex command_prank_on_channels = address.cs;

    assert(command_bank < _ddr5_memspec_3ds.NumOfTotalBanks);
    DPRINT_ASSERT(command_bg < _ddr5_memspec_3ds.NumOfTotalBankGroups, "SDRAMconstraint", "command_bg: %d",command_bg);
    assert(command_bg < _ddr5_memspec_3ds.NumOfBankGroups);
    assert(command_lrank < _ddr5_memspec_3ds.TotalNumOfLogicalRanks);
    assert(command_prank < _ddr5_memspec_3ds.NumOfPhysicalRanksPerChannel);

    sc_time previous_cmd_record_time;
    sc_time command_avail_time = sc_time_stamp();

    if(command == Command::RFMab)
        command = Command::REFab;
    if(command == Command::RFMsb)
        command = Command::REFsb;

    if(command == Command::ACT)
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
        // assert((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
        }

        previous_cmd_record_time = previous_command_time4lrank[Command::ACT][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_S_slr_mc);
        }

        // previous_cmd_record_time = previous_command_time4prank[Command::ACT][command_prank];
        // if(previous_cmd_record_time != MaxTime)
        // {
        //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_dlr_mc);
        // }

        for(int i=0; i< _ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
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

        // previous_cmd_record_time = previous_command_time4prank[Command::REFab][command_prank];
        // if(previous_cmd_record_time != MaxTime)
        // {
        //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dlr_mc);
        // }

        for(int i=0; i< _ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
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

        // previous_cmd_record_time = previous_command_time4channel[Command::REFab][command_ch];
        // if(previous_cmd_record_time != MaxTime)
        // {
        //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dpr_mc);
        // }
        for(int i=0; i< _ddr5_memspec_3ds.NumOfPhysicalRanksPerChannel; i++)
        {
            if( i == command_prank_on_channels)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4channel_prank[Command::REFab][command_ch][i];
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

        // previous_cmd_record_time = previous_command_time4prank[Command::REFsb][command_prank];
        // if(previous_cmd_record_time != MaxTime)
        // {
        //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tREFSBRD_dlr_mc);
        // }
        for(int i=0; i< _ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
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
            // assert((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
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

        for(int i=0; i< _ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
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

        // previous_cmd_record_time = previous_command_time4prank[Command::REFsb][command_prank];
        // if(previous_cmd_record_time != MaxTime)
        // {
        //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFCsb_dlr_mc);
        // }

        previous_cmd_record_time = previous_command_time4lrank[Command::REFab][command_lrank];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_slr_mc);
        }

        // previous_cmd_record_time = previous_command_time4prank[Command::REFab][command_prank];
        // if(previous_cmd_record_time != MaxTime)
        // {
        //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dlr_mc);
        // }

        // previous_cmd_record_time = previous_command_time4channel[Command::REFab][command_ch];
        // if(previous_cmd_record_time != MaxTime)
        // {
        //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dpr_mc);
        // }
        for(int i=0; i< _ddr5_memspec_3ds.NumOfPhysicalRanksPerChannel; i++)
        {
            if( i == command_prank_on_channels)
            {
                continue;
            }
            previous_cmd_record_time = previous_command_time4channel_prank[Command::REFab][command_ch][i];
            if(previous_cmd_record_time != MaxTime)
            {
                command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dpr_mc);
            }
    }

    // previous_cmd_record_time = previous_command_time4lrank[Command::ACT][command_lrank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_dlr_mc);
    // }

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

    for(int i=0; i< _ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
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

    // previous_cmd_record_time = previous_command_time4prank[Command::REFab][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dlr_mc);
    // }

    // previous_cmd_record_time = previous_command_time4channel[Command::REFab][command_ch];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFC_dpr_mc);
    // }
    for(int i=0; i< _ddr5_memspec_3ds.NumOfPhysicalRanksPerChannel; i++)
    {
        if( i == command_prank_on_channels)
        {
            continue;
        }
        previous_cmd_record_time = previous_command_time4channel_prank[Command::REFab][command_ch][i];
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

    // previous_cmd_record_time = previous_command_time4prank[Command::REFsb][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRFCsb_dlr_mc);
    // }

    previous_cmd_record_time = previous_command_time4lrank[Command::ACT][command_lrank];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_L_slr_mc);
    }

    // previous_cmd_record_time = previous_command_time4prank[Command::ACT][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tRRD_dlr_mc);
    // }

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
            command_avail_time = std::max(command_avail_time, last_4Activates4lrank[command_prank].front()
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

    // previous_cmd_record_time = previous_command_time4prank[Command::RD][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_dlr_mc);
    // }

    // same sub-channel, though has no time constraint, should also limited to tburst length
    previous_cmd_record_time = previous_command_time4channel[Command::RD][command_ch];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
    }
    previous_cmd_record_time = previous_command_time4channel[Command::RDA][command_ch];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
    }
    previous_cmd_record_time = previous_command_time4channel[Command::WR][command_ch];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
    }
    previous_cmd_record_time = previous_command_time4channel[Command::WRA][command_ch];
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

    // previous_cmd_record_time = previous_command_time4prank[Command::WR][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WTR_dlr_mc);
    // }

    for(int i=0; i< _ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
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
    }

    previous_cmd_record_time = previous_command_time4bg[Command::RDA][command_bg];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_L_slr_mc);
    }

    previous_cmd_record_time = previous_command_time4lrank[Command::RDA][command_lrank];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_S_slr_mc);
    }

    // previous_cmd_record_time = previous_command_time4prank[Command::RDA][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_dlr_mc);
    // }

    previous_cmd_record_time = previous_command_time4bg[Command::WRA][command_bg];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_L_WTR_slr_mc);
    }

    previous_cmd_record_time = previous_command_time4lrank[Command::WRA][command_lrank];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_S_WTR_slr_mc);
    }

    // previous_cmd_record_time = previous_command_time4prank[Command::WRA][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WTR_dlr_mc);
    // }
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

    // previous_cmd_record_time = previous_command_time4prank[Command::WR][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dlr_mc);
    // }

    // previous_cmd_record_time = previous_command_time4channel[Command::WR][command_ch];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dpr_mc);
    // }
    for(int i=0; i< _ddr5_memspec_3ds.NumOfPhysicalRanksPerChannel; i++)
    {
        if( i == command_prank_on_channels)
        {
            continue;
        }
        previous_cmd_record_time = previous_command_time4channel_prank[Command::WR][command_ch][i];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dpr_mc);
        }
        previous_cmd_record_time = previous_command_time4channel_prank[Command::WRA][command_ch][i];
        if(previous_cmd_record_time != MaxTime)
        {
            command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dpr_mc);
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

    // previous_cmd_record_time = previous_command_time4prank[Command::RD][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_RTW_dlr_mc);
    // }

    // same sub-channel, though has no time constraint, should also limited to tburst length
    previous_cmd_record_time = previous_command_time4channel[Command::RD][command_ch];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc + _ddr5_memspec_3ds.tCK_mc);
    }
    previous_cmd_record_time = previous_command_time4channel[Command::RDA][command_ch];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc + _ddr5_memspec_3ds.tCK_mc);
    }
    previous_cmd_record_time = previous_command_time4channel[Command::WR][command_ch];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
    }
    previous_cmd_record_time = previous_command_time4channel[Command::WRA][command_ch];
    if(previous_cmd_record_time != MaxTime)
    {
        command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tBurst_mc);
    }

    for(int i=0; i< _ddr5_memspec_3ds.NumOfLogicalRanksPerPhysicalRank; i++)
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

    // previous_cmd_record_time = previous_command_time4prank[Command::WRA][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dlr_mc);
    // }

    // previous_cmd_record_time = previous_command_time4channel[Command::WRA][command_ch];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_WR_dpr_mc);
    // }

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

    // previous_cmd_record_time = previous_command_time4prank[Command::RDA][command_prank];
    // if(previous_cmd_record_time != MaxTime)
    // {
    //     command_avail_time = std::max(command_avail_time, previous_cmd_record_time + _ddr5_memspec_3ds.tCCD_RTW_dlr_mc);
    // }
}
else
{
    SC_REPORT_ERROR("SdramConstraint", "Unknown command!");
}
if(previous_command_time4channel_onbus[command_ch] != MaxTime)
{
    command_avail_time = std::max(command_avail_time, previous_command_time4channel_onbus[command_ch] + _ddr5_memspec_3ds.tCK_mc);
}
if(command_avail_time != MaxTime)
{
    // assert((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
    DPRINT_ASSERT((command_avail_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME,"SDRAM constraint","%s command_avail_time:%s",command.to_string().c_str(),command_avail_time.to_string().c_str());
}
return command_avail_time;
}

void
SdramConstraintDDR5_3ds::InsertCommand(Command command,BankAddress address)
{
    BankIndex command_bank = address.real_ba;
    BankGroupIndex command_bg = address.real_bg;
    LrankIndex command_lrank = address.real_cid;
    PrankIndex command_prank = address.cs;
    ChannelIndex command_ch = address.ch;

    LrankIndex command_lrank_on_pranks = address.cid;
    PrankIndex command_prank_on_lrank = address.cs;

    if(command == Command::RFMab)
        command = Command::REFab;
    if(command == Command::RFMsb)
        command = Command::REFsb;

    sc_time record_time = sc_time_stamp();
    assert((record_time % _ddr5_memspec_3ds.tCK_mc) == sc_core::SC_ZERO_TIME);
    previous_command_time4lrank[command][command_lrank] = record_time;
    previous_command_time4prank[command][command_prank] = record_time;
    previous_command_time4channel[command][command_ch] = record_time;

    previous_command_time4prank_lrank[command][command_prank][command_lrank_on_pranks] = record_time;
    previous_command_time4channel_prank[command][command_ch][command_prank_on_lrank] = record_time;


    previous_command_time4channel_onbus[command_ch] = record_time;


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

    if(command == Command::ACT || command == Command::REFsb || command == Command::RFMsb)
    {
        if(last_4Activates4lrank[command_lrank].size() == 4)
        {
            last_4Activates4lrank[command_lrank].pop_front();
        }
        last_4Activates4lrank[command_lrank].push_back(record_time);

        if(last_4Activates4prank[command_prank].size() == 4)
        {
            last_4Activates4prank[command_prank].pop_front();
        }
        last_4Activates4prank[command_prank].push_back(record_time);
    }
    if(command == Command::REFab || command == Command::RFMab)
    {
        if(last_4Activates4prank[command_prank].size() == 4)
        {
            last_4Activates4prank[command_prank].pop_front();
        }
        last_4Activates4prank[command_prank].push_back(record_time);
    }
    assert(last_4Activates4prank[command_prank].size() <=4 && last_4Activates4lrank[command_lrank].size() <=4);
}

std::vector<BankIndex>
SdramConstraintDDR5_3ds::GetSameBgBankVec(BankAddress address) const
{
    std::vector<BankGroupIndex> same_ba_vec;
    BankIndex num_bank_per_bg = _ddr5_memspec_3ds.NumOfBanksPerBg;
    BankGroupIndex num_bg_per_lrank = _ddr5_memspec_3ds.NumOfBgPerLogicalRank;
    BankIndex num_bank_per_lrank = _ddr5_memspec_3ds.NumOfBankPerLogicalRank;
    // BankGroupIndex bg_base = address.real_bg - address.real_bg % num_bg_per_lrank;
    BankIndex ba_base = address.real_ba - address.real_ba % num_bank_per_lrank;
    BankIndex ba_offset = address.real_ba % num_bank_per_bg;

    for(unsigned i =0; i< num_bg_per_lrank; i++)
    {
        same_ba_vec.push_back(ba_base + ba_offset + i * num_bank_per_bg);
    }
    return same_ba_vec;
}

std::vector<BankGroupIndex>
SdramConstraintDDR5_3ds::GetSameBgVec(BankAddress address) const
{
    std::vector<BankGroupIndex> same_bg_vec;

    BankGroupIndex num_bg_per_lrank = _ddr5_memspec_3ds.NumOfBgPerLogicalRank;
    BankGroupIndex bg_base = address.real_bg - address.real_bg % num_bg_per_lrank;

    for(unsigned i =0; i< num_bg_per_lrank; i++)
    {
        same_bg_vec.push_back(bg_base + i);
    }
    return same_bg_vec;
}

    }// Controller
} // dmu