#include <tuple>

#include "Controller/BankSlice.hh"
#include "Controller/BankSliceManager.hh"
#include "Controller/CamEntry.hh"
#include "Controller/CmdSelect.hh"
#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Common/CommonDefine.hh"

namespace dmu{
    namespace Controller{
using Rank_INDEX = unsigned;

CommandTuple::Type
CmdSelect::SelectCommand(const ReadyCommands& ready_commands, GlobalRdWrState global_rdwr_state)
{
    sc_core::sc_time min_avail_time = MaxTime;
    sc_core::sc_time cmd_avail_time = sc_core::SC_ZERO_TIME;
    BSC_INDEX cmd_bsc_index;

    std::set<BSC_INDEX> row_bsc_set;
    std::set<BSC_INDEX> col_bsc_set;
    std::set<Rank_INDEX> rank_index_set;

    std::map<BSC_INDEX,CommandTuple::Type> row_bsc2row_cmd_map;
    std::map<BSC_INDEX,CommandTuple::Type> col_bsc2col_cmd_map;
    std::map<Rank_INDEX,CommandTuple::Type> rank_index2rank_cmd_map;

    // auto result = ready_commands.cend();
    for(auto it = ready_commands.cbegin(); it!=ready_commands.cend();it++)
    {
        // Implement with Codex
        cmd_avail_time = std::get<CommandTuple::AvailTime>(*it);//TODO: Cmd lenth if command is 2-N mode
        // std::cout << " Debug: "<<cmd_avail_time << ",\t";
        assert(cmd_avail_time <= sc_core::sc_time_stamp() );
        if(std::get<CommandTuple::Command>(*it).IsCASCommand())
        {
            // readyCasCommands.emplace_back(*it);
            cmd_bsc_index = _bank_slice_manager.GetBa2BscTable()->at(std::get<CommandTuple::BaAddress>(*it).real_ba);
            col_bsc_set.emplace(cmd_bsc_index);
            col_bsc2col_cmd_map.emplace(cmd_bsc_index,*it);
        }
        else if(std::get<CommandTuple::Command>(*it).IsRASCommand())
        {
            // readyRasCommands.emplace_back(*it);
            cmd_bsc_index = _bank_slice_manager.GetBa2BscTable()->at(std::get<CommandTuple::BaAddress>(*it).real_ba);
            row_bsc_set.emplace(cmd_bsc_index);
            row_bsc2row_cmd_map.emplace(cmd_bsc_index,*it);
        }
        else if(std::get<CommandTuple::Command>(*it).IsRefCommand())
        {
            Rank_INDEX rank_index = std::get<CommandTuple::BaAddress>(*it).real_cid;
            rank_index_set.emplace(rank_index);
            rank_index2rank_cmd_map.emplace(rank_index,*it);
        }
        else
        {
            ;
        }

    }
    DPRINT_INFO(CMD_SELECT, "Cmd Select", "ready refresh commands size: %ld, ready row commands size: %ld, ready col commands size: %ld",
        rank_index2rank_cmd_map.size(),
        row_bsc2row_cmd_map.size(),
        col_bsc2col_cmd_map.size()
    );

    // BSC_INDEX oldest_page_hit_bsc = _bank_slice_manager.GetOldestPageHitCmdBsc();
    if(!rank_index2rank_cmd_map.empty())
    {
        // for(auto rank_index:rank_index_set)
        // {
        //     std::cout << " "<< rank_index <<",\t";
        // }
        // std::cout << std::endl;
        // for(auto iter = rank_index2rank_cmd_map.begin(); iter != rank_index2rank_cmd_map.end();iter++)
        // {
        //     std::cout << " "<< iter->first << " ; "<<std::get<CommandTuple::BaAddress>(iter->second).real_cid<<",\t";
        // }
        // std::cout<< std::endl;
        auto arb_iter = rank_index_set.upper_bound(last_selected_rank_index);
        if(arb_iter != rank_index_set.cend())
        {
            last_selected_rank_index = *arb_iter;
        }
        else {
            last_selected_rank_index = *rank_index_set.begin();
        }
        return rank_index2rank_cmd_map.at(last_selected_rank_index);
    }
    if(!col_bsc_set.empty())
    {
        //round-robin select a bsc and oldest page-hit cmd bsc
        BSC_INDEX oldest_page_hit_bsc;
        if(global_rdwr_state == GlobalRdWrState::Rd || global_rdwr_state == GlobalRdWrState::Rd2Wr)
        {
            oldest_page_hit_bsc = _bank_slice_manager.GetRdOldestPageHitBsc();
        }
        else if(global_rdwr_state == GlobalRdWrState::Wr || global_rdwr_state == GlobalRdWrState::Wr2Rd)
        {
            oldest_page_hit_bsc = _bank_slice_manager.GetWrOldestPageHitBsc();
        }
        else {
            ;
        }
        if(col_bsc_set.count(oldest_page_hit_bsc)>0)
        {
            return col_bsc2col_cmd_map.at(oldest_page_hit_bsc);
        }
        else {
            auto arb_iter = col_bsc_set.lower_bound(last_selected_col_bsc);
            if(arb_iter != col_bsc_set.cend())
            {
                last_selected_col_bsc = *arb_iter;
            }
            else {
                last_selected_col_bsc = *col_bsc_set.begin();
            }
            return col_bsc2col_cmd_map.at(last_selected_col_bsc);
        }
    }
    if(!row_bsc_set.empty())
    {
        //round-robin select a bsc and oldest page-miss cmd bsc
        
        BSC_INDEX oldest_page_miss_bsc;
        if(global_rdwr_state == GlobalRdWrState::Rd || global_rdwr_state == GlobalRdWrState::Rd2Wr)
        {
            oldest_page_miss_bsc = _bank_slice_manager.GetRdOldestPageMissBsc();
        }
        else if(global_rdwr_state == GlobalRdWrState::Wr || global_rdwr_state == GlobalRdWrState::Wr2Rd)
        {
            oldest_page_miss_bsc = _bank_slice_manager.GetWrOldestPageMissBsc();
        }
        else {
            ;
        }
        if(row_bsc_set.count(oldest_page_miss_bsc)>0)
        {
            return row_bsc2row_cmd_map.at(oldest_page_miss_bsc);
        }
        else {
            auto arb_iter = row_bsc_set.lower_bound(last_selected_row_bsc);
            if(arb_iter != row_bsc_set.cend())
            {
                last_selected_row_bsc = *arb_iter;
            }
            else {
                last_selected_row_bsc = *row_bsc_set.begin();
            }
            return row_bsc2row_cmd_map.at(last_selected_row_bsc);
        }
    }

    DPRINT_FATAL(true, "No command to select");
    std::abort();
    // if(result != ready_commands.cend())
    // {
    //     return *result;
    // }
    // return {Command::NOP , 0 ,BankAddress(),MaxTime,true};
}


    }
}