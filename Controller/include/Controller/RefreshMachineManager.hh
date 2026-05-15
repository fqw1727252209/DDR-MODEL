#ifndef __REFRESHMACHINEMANAGER_HH__
#define __REFRESHMACHINEMANAGER_HH__

#include "Controller/BankSliceManager.hh"
#include "Controller/RefreshMachine.hh"
#include "Controller/common/Command.hh"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <iterator>
#include <memory>
#include <unordered_map>
#include <vector>

namespace dmu{
    namespace Controller{

class RefreshMachineManager{

    public:
        RefreshMachineManager( BankSliceManager& bank_slice_manager,const Configure& config,unsigned pch_id)
        : _bank_slice_manager(bank_slice_manager)
        , _config(config)
        {
            for(int i = 0; i < config.mem_spec->NumOfLogicalRanksPerPseudoChannel; i++){
                refresh_rank_ids.emplace_back(i);
                refreshMachines.emplace(i,std::make_unique<RefreshMachine>(i, bank_slice_manager,config,pch_id));
            }
        }

        // ReadyCommands GetRefreshReadyCommands(){
        //     ReadyCommands ready_commands;
        //     for(auto& refresh_machine : refreshMachines){
        //         auto commands = refresh_machine->GetReadyCommands();
        //         ready_commands.insert(ready_commands.end(), commands.begin(), commands.end());
        //     }
        //     return ready_commands;
        // }
        std::vector<unsigned> GetRefreshRankIds(){
            return refresh_rank_ids;
        }
        RefreshMachine* GetRefreshMachine(unsigned rank_index)
        {
            return refreshMachines.at(rank_index).get();
        }

        bool IsRefreshReadyCommandsEmpty(){
            refresh_ready_commands.clear();
            for(auto& refresh_machine : refreshMachines){
                auto refresh_commands = refresh_machine.second->GetRefreshAvailCommand();
                if(!refresh_commands.empty())
                {
                    refresh_ready_commands.insert(refresh_ready_commands.end(), std::make_move_iterator(refresh_commands.begin()),std::make_move_iterator(refresh_commands.end()));
                }
            }
            return refresh_ready_commands.empty();
        }
        ReadyCommands& GetRefreshReadyCommands()
        {
            return refresh_ready_commands;
        }

        sc_core::sc_time GetNextRefreshTriggerTime(){
            sc_core::sc_time next_refresh_time = sc_core::sc_time::from_value(std::numeric_limits<sc_dt::uint64>::max());
            for(auto& refresh_machine : refreshMachines){
                auto refresh_time = refresh_machine.second->GetNextRefreshTriggerTime();
                next_refresh_time = std::min(next_refresh_time, refresh_time);
            }
            return next_refresh_time;
        }
        void CommandUpdate(const CommandTuple::Type& sending_cmd)
        {
            Command sending_cmd_type = std::get<CommandTuple::Command>(sending_cmd);
            BankAddress sending_cmd_ba_addr = std::get<CommandTuple::BaAddress>(sending_cmd);
            unsigned sending_cmd_rank_index = sending_cmd_ba_addr.real_cid;
            for(auto& refresh_machine : refreshMachines){
                if(refresh_machine.first == sending_cmd_rank_index)
                    refresh_machine.second->Update(sending_cmd);
            }
        }

    private:
        std::unordered_map<unsigned,std::unique_ptr<RefreshMachine>> refreshMachines;
        std::vector<unsigned> refresh_rank_ids;
        BankSliceManager& _bank_slice_manager;
        const Configure& _config;

        ReadyCommands refresh_ready_commands;

};

    } // namespace Controller
} // namespace dmu

#endif