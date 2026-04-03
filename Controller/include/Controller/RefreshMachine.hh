#ifndef __REFRESH_MACHINE_HH__
#define __REFRESH_MACHINE_HH__

#include "Controller/BankSliceManager.hh"
#include "Configure/AddressDecoder.hh"
#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Configure/Configure.hh"
#include "Configure/DDR5MemSpec.hh"
#include "Controller/common/DfiExtension.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_time.h"
#include <cassert>
#include <iostream>
#include <tuple>

namespace dmu{
    namespace Controller{

class RefreshMachine{
    public:
        RefreshMachine(unsigned rank_id, BankSliceManager& bank_slice_manager, const Configure& config)
        : rank_id(rank_id)
        , cs(static_cast<unsigned>(rank_id / config.mem_spec->NumOfLogicalRanksPerPhysicalRank))
        , cid(rank_id % config.mem_spec->NumOfLogicalRanksPerPhysicalRank)
        , _bank_slice_manager(bank_slice_manager)
        , _configure(config)
        , refresh_interval(config.mem_spec->tREFI_mc)
        , post_pone_threshold(config.controller_config->REFRESH_PENDING_THRESHOLD)
        {
            // unsigned RankBits = config.address_decoder;
            for(unsigned bank_id = rank_id * config.mem_spec->NumOfBankPerLogicalRank; bank_id < (rank_id+1) * config.mem_spec->NumOfBankPerLogicalRank; bank_id++)
            {
                rank_banks.push_back(bank_id);
            }
            if(config.controller_config->REFRESH_ENABLE)
            {
                next_refresh_trigger_time = static_cast<unsigned>(refresh_interval/(config.mem_spec->tCK_mc * config.mem_spec->NumOfBankPerLogicalRank)) * config.mem_spec->tCK_mc * rank_id;
            }
            else
            {
                next_refresh_trigger_time = sc_core::sc_max_time();
            }

            rank_address = BankAddress(0,cs,cid,rank_id);

            _refresh_trans = new tlm::tlm_generic_payload();
            _refresh_trans->set_address(0);
            _refresh_trans->set_data_ptr(nullptr);
            _refresh_trans->set_data_length(0);
            _refresh_trans->set_streaming_width(0);
            _refresh_trans->set_response_status(tlm::TLM_OK_RESPONSE);
            _refresh_trans->set_command(tlm::TLM_IGNORE_COMMAND);

            dfi_extension = new DfiExtension();
            _refresh_trans->set_extension(dfi_extension);

        }

        ~RefreshMachine()
        {
            delete _refresh_trans;
            // delete dfi_extension; // 在删除refresh_trans的时候，dfi_extension也会被删除
        }

        // check the rank bank is closed, so that the refresh command can be issued
        // rank bank has two condition:
        //  1. all banks dont allocated to bsc, so the bank must be in closed state(or idle state);
        //  2. some banks has allocated the bsc, so these banks needed to be closed first(bsc should send pre command), then refresh command can be issued;
        // however in the RTL design, if the refresh command is not critical(refresh command postponeed exceed the threshold), then the refresh command not to be serverd until critical
        // if set bsc waiting for refresh, the active bsc should send precharge command, and then keep idle
        bool IsAllBanksClosed()
        {
            auto ba2bsc_index_table = _bank_slice_manager.GetBa2BscTable();
            auto bank_slice_map = _bank_slice_manager.GetBankSliceMap();
            for(auto bank_id: rank_banks)
            {
                if(ba2bsc_index_table->find(bank_id) == ba2bsc_index_table->end())
                {
                    continue;
                }
                auto bsc_index = ba2bsc_index_table->at(bank_id);
                auto bank_slice = bank_slice_map->at(bsc_index).get();
                if((bank_slice_map->at(bsc_index))->IsPageOpen())
                {
                    return false;
                }
            }
            return true;
        }

        void SetBankInRefreshWaiting()
        {
            auto ba2bsc_index_table = _bank_slice_manager.GetBa2BscTable();
            auto bank_slice_map = _bank_slice_manager.GetBankSliceMap();
            for(auto bank_id: rank_banks)
            {
                if(ba2bsc_index_table->find(bank_id) == ba2bsc_index_table->end())
                {
                    continue;
                }
                auto bsc_index = ba2bsc_index_table->at(bank_id);
                auto bank_slice = bank_slice_map->at(bsc_index).get();
                (bank_slice_map->at(bsc_index))->SetRefreshWaiting();
            }
        }

        void Evaluate()
        {
            next_command = Command::NOP;
            if(sc_core::sc_time_stamp() >= next_refresh_trigger_time)
            {
                next_refresh_trigger_time += refresh_interval;
                refresh_pending_count++;
            }
            if(refresh_pending_count > 0)
            {
                if(IsAllBanksClosed())
                {
                    next_command = Command::REFab;
                }
                else
                {
                    if(IsRefreshCritical())
                    {
                        SetBankInRefreshWaiting();
                    }
                }
            }
        }
        // if(sc_core::sc_time_stamp() >= next_refresh_trigger_time)
        // {
        //     if(sc_core::sc_time_stamp() >= next_refresh_trigger_time + refresh_interval)
        //     {
        //         next_refresh_trigger_time += refresh_interval;
        //         state = State::Regular;
        //     }

        //     if(state == State::Regular)
        //     {
        //         bool do_refresh = true;
        //         if(IsRefreshCritical())
        //         {
        //             SetBankInRefreshWaiting();
        //         }
        //         else
        //         {
        //             if(IsAllBanksClosed())
        //             {
        //                 do_refresh = false;
        //                 refresh_pending_count++;
        //                 next_refresh_trigger_time+= refresh_interval;
        //             }
        //         }
        //         if(do_refresh)
        //         {
        //             next_command = Command::REFab;
        //         }
        //     }
        // }

        void Update(const CommandTuple::Type& sending_cmd)
        {
            Command update_cmd = std::get<Command>(sending_cmd);
            if(update_cmd == Command::REFab)
            {
                assert(refresh_pending_count > 0);
                refresh_pending_count--;
                if(IsRefreshCritical())
                {
                    SetBankInRefreshWaiting();
                }
                else
                {
                    FreeRefreshCritical();
                }
            }
        }

        void FreeRefreshCritical()
        {
            auto ba2bsc_index_table = _bank_slice_manager.GetBa2BscTable();
            auto bank_slice_map = _bank_slice_manager.GetBankSliceMap();
            for(auto bank_id: rank_banks)
            {
                if(ba2bsc_index_table->find(bank_id) == ba2bsc_index_table->end())
                {
                    continue;
                }
                auto bsc_index = ba2bsc_index_table->at(bank_id);
                auto bank_slice = bank_slice_map->at(bsc_index).get();
                (bank_slice_map->at(bsc_index))->ClearRefreshWaiting();
            }
        }

        Command GetRefreshCommand()
        {
            return next_command;
        }

        sc_core::sc_time& GetRefreshCommandAvailTime() { return command_avail_time; } // this function will return the command can be sent available time based on the AC Timing, if the command is NOP, return sc_max_time()

        inline unsigned GetRankId() const { return rank_id; }
        inline unsigned GetCs() const { return cs; }
        inline unsigned GetCid() const { return cid; }
        inline unsigned GetPendingCount() const { return refresh_pending_count; }

        inline const BankAddress& GetRankAddress() const { return rank_address; }
        inline bool IsRefreshCritical() const { return refresh_pending_count >= post_pone_threshold; }

        inline bool IsRefreshCommandAvail() const { return next_command != Command::NOP && command_avail_time <= sc_core::sc_time_stamp(); }

        ReadyCommands GetRefreshAvailCommand()
        {
            ReadyCommands ready_commands;
            if(IsRefreshCommandAvail())
            {
                ready_commands.emplace_back(next_command, 0, rank_address, command_avail_time, false);
            }
            return ready_commands;
        }

        sc_core::sc_time GetNextRefreshTriggerTime()
        {
            return std::min(next_refresh_trigger_time, command_avail_time);
        }

        void Print()
        {
            std::cout << "RankId: " << rank_id
            << " RankAddress: " << rank_address
            << " RefreshPendingCount: " << refresh_pending_count
            << " NextTriggerTime: " << next_refresh_trigger_time
            << " Next Command: " << next_command.to_string()
            << " Command availalbe time: " << command_avail_time
            << " Is Related Bank Closed: " << IsAllBanksClosed()
            << "\t";
            std::cout << "Related Bank Index: { ";
            for(auto bank_id: rank_banks)
            {
                std::cout << bank_id << " ";
            }
            std::cout << "}" << std::endl;
        }

        tlm::tlm_generic_payload* GetRefreshTrans() {
            return _refresh_trans;
        }

    private:
        enum class State
        {
            Regular,
            Pulledin
        } state = State::Regular;

        const unsigned rank_id; // logical rank id, real rank id, also the cs and cid
        const unsigned cs;
        const unsigned cid;
        Command next_command{Command::NOP};
        const sc_core::sc_time refresh_interval;

        BankSliceManager& _bank_slice_manager;
        const Configure& _configure;

        BankAddress rank_address;

        tlm::tlm_generic_payload* _refresh_trans;
        DfiExtension* dfi_extension;

        sc_core::sc_time command_avail_time{sc_core::sc_max_time()};
        sc_core::sc_time next_refresh_trigger_time; // the initial refresh trigger time should be set to staggered, based on the rank id and total rank number to decide the stagger time;
        std::vector<unsigned> rank_banks; // banks belongs to the logical rank, the recorded bank id should be real ba
        std::vector<unsigned> allocated_banks; // record the banks that are allocated bsc

        unsigned refresh_pending_count{0};

        const unsigned post_pone_threshold; // if refresh_pending_count >= this value, then the refresh will set to critical;

};

    } // namespace Controller
} // namespace dmu

#endif