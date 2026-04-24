#include <cassert>
#include <iterator>
#include <memory>

#include "Controller/BankSliceManager.hh"
#include "Controller/CamIF.hh"
#include "Common/CommonDefine.hh"
#include "sysc/kernel/sc_simcontext.h"

namespace dmu{
    namespace Controller{
using OrderList = std::list<CAM_INDEX>;
BankSliceManager::BankSliceManager(Scheduler& scheduler, const Configure& config)
: _config(config)
, _scheduler(scheduler)
{
    for(unsigned i = 0; i < config.controller_config->BSC_NUM; i++)
    {
        unallocated_bsc_index_queue.push_back(i);
        bsc_index_2_bankslice.emplace(static_cast<BSC_INDEX>(i),std::make_unique<BankSlice>(scheduler,config,i));
    }
    bsc_table.reserve(config.controller_config->BSC_NUM);
    ba2bsc_table.reserve(config.controller_config->BSC_NUM);
    bsc_ready_commands.reserve(config.controller_config->BSC_NUM);
}

bool
BankSliceManager::IsNeedBankSliceRelease()
{
    // bank release will call the bank slice release
    // 1. bsc has no valid cmd in rd and wr cam, and bsc is used and idle state(also precharged)
    // 2. bsc mapped bank is in refreshing, the bsc can be released
    empty_bsc_set.clear();
    for(auto& bsc_index: allocated_bsc_index_set)
    {
        auto& bsc_ba_addr = bsc_table[bsc_index].real_ba;
        if(_scheduler.GetRdCam()->IsBaOrderListEmpty(bsc_ba_addr) &&
           _scheduler.GetWrCam()->IsBaOrderListEmpty(bsc_ba_addr) &&
           !bsc_index_2_bankslice[bsc_index]->IsPageOpen())
        {
            empty_bsc_set.insert(bsc_index);
        }
    }
    return !empty_bsc_set.empty();
    // Implement with Codex
    // TODO: support force release
    // if the empty_bsc_set is empty, but the bsc number if exceed the bsc_num_high_threshold
    // will do the force release, will call the selected bank slice to enter bsc precharged state
}

void
BankSliceManager::BankSliceRelease()
{
    auto find_rr_first_element = empty_bsc_set.lower_bound(last_released_bsc_index);
    BSC_INDEX rr_released_bsc_index;
    if(find_rr_first_element != empty_bsc_set.end())
    {
        rr_released_bsc_index = *find_rr_first_element;
    }
    else
    {
        rr_released_bsc_index = *empty_bsc_set.begin();
    }
    last_released_bsc_index = rr_released_bsc_index;

    auto rr_released_bsc_ba = bsc_table[rr_released_bsc_index].real_ba;
    bsc_table.erase(rr_released_bsc_index);
    ba2bsc_table.erase(rr_released_bsc_ba);
    bsc_index_2_bankslice[rr_released_bsc_index]->Release();
    unallocated_bsc_index_queue.push_back(rr_released_bsc_index);
    allocated_bsc_index_set.erase(rr_released_bsc_index);

    if(!_scheduler.GetRdCam()->IsBaOrderListEmpty(rr_released_bsc_ba))
    {
        const OrderList& rd_cam_ba_order_list = _scheduler.GetRdCam()->GetBaOrderList(rr_released_bsc_ba);
        for(auto& cam_index: rd_cam_ba_order_list)
        {
            RdCamEntry* rd_cam_entry = _scheduler.GetRdCam()->GetCamEntry(cam_index);
            rd_cam_entry->SetReleaseBsc();
        }
    }
    if(!_scheduler.GetWrCam()->IsBaOrderListEmpty(rr_released_bsc_ba))
    {
        const OrderList& wr_cam_ba_order_list = _scheduler.GetWrCam()->GetBaOrderList(rr_released_bsc_ba);
        for(auto& cam_index: wr_cam_ba_order_list)
        {
            WrCamEntry* wr_cam_entry = _scheduler.GetWrCam()->GetCamEntry(cam_index);
            wr_cam_entry->SetReleaseBsc();
        }
    }
}

bool
BankSliceManager::IsNeedBankSliceAllocation()
{
    return !(_scheduler.GetRdCam()->GetUnallocatedBscCamIndex().empty() && _scheduler.GetWrCam()->GetUnallocatedBscCamIndex().empty())
           && !unallocated_bsc_index_queue.empty();
    // Implement with Codex
    //TODO: Need to add a condition decide, when there is no avail bsc can be allocated
}

void
BankSliceManager::BankSliceAllocation()
{
    ResetAllocationState();
    const WaitingList& rd_wating_list = _scheduler.GetRdCam()->GetUnallocatedBscCamIndex();
    const WaitingList& wr_wating_list = _scheduler.GetWrCam()->GetUnallocatedBscCamIndex();

    if(rd_wating_list.empty() && wr_wating_list.empty())
    {
        return;
    }
    else
    {
        DPRINT_INFO(false,"BankSliceManager","Bsc Allocate Stage");
        this->allocation_state.current_bsc_allocated = true;
        BankAddress& ba_addr = this->allocation_state.current_allocated_bank_address;

        if(!rd_wating_list.empty() && wr_wating_list.empty())
        {
            CAM_INDEX winning_rd_cam_index = _scheduler.GetRdCamFilter()->GetSelectedRdCamIndex(rd_wating_list);
            RdCamEntry* selected_rd_cam_entry = _scheduler.GetRdCam()->GetCamEntry(winning_rd_cam_index);

            ba_addr = BankAddress(selected_rd_cam_entry->sdram_addr);
            allocation_state.current_is_rd_allocated = true;
        }
        else if(rd_wating_list.empty() && !wr_wating_list.empty())
        {
            CAM_INDEX winning_wr_cam_index = _scheduler.GetWrCamFilter()->GetSelectedWrCamIndex(wr_wating_list);
            WrCamEntry* selected_wr_cam_entry = _scheduler.GetWrCam()->GetCamEntry(winning_wr_cam_index);

            ba_addr = BankAddress(selected_wr_cam_entry->sdram_addr);
            allocation_state.current_is_rd_allocated = false;
        }
        else if(!rd_wating_list.empty() && !wr_wating_list.empty())
        {
            CAM_INDEX winning_rd_cam_index = _scheduler.GetRdCamFilter()->GetSelectedRdCamIndex(rd_wating_list);
            RdCamEntry* selected_rd_cam_entry = _scheduler.GetRdCam()->GetCamEntry(winning_rd_cam_index);

            CAM_INDEX winning_wr_cam_index = _scheduler.GetWrCamFilter()->GetSelectedWrCamIndex(wr_wating_list);
            WrCamEntry* selected_wr_cam_entry = _scheduler.GetWrCam()->GetCamEntry(winning_wr_cam_index);

            bool rd_urgent = selected_rd_cam_entry->is_expired || selected_rd_cam_entry->is_addr_collision;
            bool wr_urgent = selected_wr_cam_entry->is_expired || selected_wr_cam_entry->is_addr_collision;
            if(rd_urgent)
            {
                ba_addr = BankAddress(selected_rd_cam_entry->sdram_addr);
                allocation_state.current_is_rd_allocated = true;
            }
            else if(!rd_urgent && wr_urgent)
            {
                ba_addr = BankAddress(selected_wr_cam_entry->sdram_addr);
                allocation_state.current_is_rd_allocated = false;
            }
            else
            {
                if(!_scheduler.IsTpwCritical() || _scheduler.IsLprCritical() || _scheduler.IsHprCritical())
                {
                    ba_addr = BankAddress(selected_rd_cam_entry->sdram_addr);
                    allocation_state.current_is_rd_allocated = true;
                }
                else
                {
                    ba_addr = BankAddress(selected_wr_cam_entry->sdram_addr);
                    allocation_state.current_is_rd_allocated = false;
                }
            }
        }
        else
        {
            ABORT_MESSAGE("Bank Allocation, illeagl situation");
        }

        // 先分配BSC索引
        this->BscIndexAllocate();
        // 不再需要重新设置current_allocated_bsc, 因为BscIndexAllocate()已经设置了该值
        for(auto& bsc_index: allocated_bsc_index_set)
        {
            std::cout << "Allocated BSC Index: " << bsc_index <<
            " Allocated Bank Address: " << (bsc_index_2_bankslice.at(bsc_index))->GetBaAddr() << std::endl;
        }

    }

}

void
BankSliceManager::BscIndexAllocate()
{
    if(true)
    {
        assert(!unallocated_bsc_index_queue.empty());
        this->allocation_state.current_allocated_bsc = unallocated_bsc_index_queue.front();
        unallocated_bsc_index_queue.pop_front();
    }
    else
    {
        this->allocation_state.current_allocated_bsc = static_cast<BSC_INDEX>(this->allocation_state.current_allocated_bank_address.real_ba);
    }
}

BSC_INDEX
BankSliceManager::GetAllocatedBscIndex()
{
    return this->allocation_state.current_allocated_bsc;
}

RealBaIndex
BankSliceManager::GetBaAddr4BscAllocation()
{
    return this->allocation_state.current_allocated_bank_address.real_ba;
}

bool
BankSliceManager::GetAllocatedDirection()
{
    return this->allocation_state.current_is_rd_allocated;
}

void
BankSliceManager::NttUpdate()
{
    if(!_scheduler.GetUpdatedBscSet(true).empty())
    {
        for(auto& bsc_index: _scheduler.GetUpdatedBscSet(true))
        {
            CAM_INDEX selected_rd_cam_index = _scheduler.GetUpdateNttTemp(bsc_index,true);
            DPRINT_INFO(TOP_DEBUG,"BankSliceManager","update the rd ntt to the bsc: %d, with selected rd cam index: %d",bsc_index,selected_rd_cam_index);
            // for there is a sceneray that a same ntt update do more than one time
            // , so the same ntt may exist in two near cycles, add this function to show that
            // ntt selected cam index may have been already deleted
            // if not exist, then do not update it to the bsc
            if(_scheduler.GetRdCam()->IsCamExist(selected_rd_cam_index))
            {
                bsc_index_2_bankslice.at(bsc_index)->SelectRdNtt(selected_rd_cam_index);
            }
            else
            {
                DPRINT_WARNING(TOP_DEBUG, "BankSliceManager","Rd Ntt Update Failed, the selected ntt cam index: %d does not exist in Rd Cam",selected_rd_cam_index);
            }
        }
    }
    if(!_scheduler.GetUpdatedBscSet(false).empty())
    {
        for(auto& bsc_index: _scheduler.GetUpdatedBscSet(false))
        {
            CAM_INDEX selected_wr_cam_index = _scheduler.GetUpdateNttTemp(bsc_index,false);
            DPRINT_INFO(TOP_DEBUG,"BankSliceManager","update the wr ntt to the bsc: %d, with selected wr cam index: %d",bsc_index,selected_wr_cam_index);
            // for there is a sceneray that a same ntt update do more than one time
            // , so the same ntt may exist in two near cycles, add this function to show that
            // ntt selected cam index may have been already deleted
            // if not exist, then do not update it to the bsc
            if(_scheduler.GetWrCam()->IsCamExist(selected_wr_cam_index))
            {
                bsc_index_2_bankslice.at(bsc_index)->SelectWrNtt(selected_wr_cam_index);
            }
            else
            {
                DPRINT_WARNING(TOP_DEBUG, "BankSliceManager","Wr Ntt Update Failed, the selected ntt cam index: %d does not exist in Wr Cam",selected_wr_cam_index);
            }
        }
    }
}

bool
BankSliceManager::IsReadyCommandsEmpty(GlobalRdWrState global_rdwr_mode)
{
    // bsc_ready_commands.clear();
    // for(auto& bsc_index: allocated_bsc_index_set)
    // {
        // BankSlice* bank_slice = bsc_index_2_bankslice[bsc_index].get();
        // auto bank_cmd_tuple = bank_slice->GetNextCommand(global_rdwr_mode);
        // Command bank_cmd = std::get<CommandTuple::Command>(bank_cmd_tuple);
        // // std::cout << "Command: "<<bank_cmd.to_string() << "\t";
        // if(bank_cmd != Command::NOP)
        // {
        //     bsc_ready_commands.emplace_back(bank_cmd_tuple);
        // }
    // }

    bsc_ready_commands.clear();
    for(auto& bsc_index: allocated_bsc_index_set)
    {
        BankSlice* bank_slice = bsc_index_2_bankslice[bsc_index].get();
        auto bank_ready_commands = bank_slice->GetAvailCommand(global_rdwr_mode);
        if(!bank_ready_commands.empty())
        {
            bsc_ready_commands.insert(bsc_ready_commands.end(),std::make_move_iterator(bank_ready_commands.begin()),std::make_move_iterator(bank_ready_commands.end()));
        }
    }

    return bsc_ready_commands.empty();
}

sc_core::sc_time
BankSliceManager::GetNextCommandTriggerTime()
{
    sc_core::sc_time trigger_time{sc_core::sc_max_time()};
    for(auto& bsc_index: allocated_bsc_index_set)
    {
        BankSlice* bank_slice = bsc_index_2_bankslice[bsc_index].get();
        trigger_time = std::min(trigger_time,bank_slice->GetNextCommandTriggerTime());
    }
    return trigger_time;

}

ReadyCommands&
BankSliceManager::GetReadyCommands()
{
    return this->bsc_ready_commands;
}

void
BankSliceManager::CommandUpdate(const CommandTuple::Type& sending_cmd)
{
    Command sending_cmd_type = std::get<CommandTuple::Command>(sending_cmd);
    BankAddress sending_cmd_ba_addr = std::get<CommandTuple::BaAddress>(sending_cmd);
    if(sending_cmd_type.IsBankCommand())
    {
        RealBaIndex sending_cmd_real_ba = sending_cmd_ba_addr.real_ba;
        BSC_INDEX sending_cmd_bsc_index = ba2bsc_table[sending_cmd_real_ba];
        bsc_index_2_bankslice[sending_cmd_bsc_index]->Update(sending_cmd);
    }
    else if(sending_cmd_type.IsGroupCommand())
    {
        // Implement with Codex
        //TODO: add the Group cmd update code
        for(auto& bsc_index: allocated_bsc_index_set)
        {
            if(bsc_index_2_bankslice.at(bsc_index)->GetBaAddr().real_cid == sending_cmd_ba_addr.real_cid
            && bsc_index_2_bankslice.at(bsc_index)->GetBaAddr().bank == sending_cmd_ba_addr.bank)
            {
                bsc_index_2_bankslice.at(bsc_index)->Update(sending_cmd);
            }
        }
    }
    else if(sending_cmd_type.IsRankCommand())
    {
        // Implement with Codex
        //TODO: add the rank cmd update code
        for(auto& bsc_index: allocated_bsc_index_set)
        {
            if(bsc_index_2_bankslice.at(bsc_index)->GetBaAddr().real_cid == sending_cmd_ba_addr.real_cid)
            {
                bsc_index_2_bankslice.at(bsc_index)->Update(sending_cmd);
            }
        }
    }
    else
    {
        std::cerr << "Bank Slice Manager do command update"<<std::endl;
        std::abort();
    }
}


void
BankSliceManager::AllocationUpdate()
{
    if(allocation_state.current_bsc_allocated)
    {
        //update bsc table;
        bsc_table.emplace(allocation_state.current_allocated_bsc,
                          allocation_state.current_allocated_bank_address);
        ba2bsc_table.emplace(allocation_state.current_allocated_bank_address.real_ba,
                          allocation_state.current_allocated_bsc);
        assert(!(allocated_bsc_index_set.count(allocation_state.current_allocated_bsc)>0));
        allocated_bsc_index_set.insert(allocation_state.current_allocated_bsc);
        // Implement with Codex
        //TODO: need to build a map:
        // 1. all same rank bankslice,based on the rank number can find all bankslice
        // 2. same bank refresh to the same bank id in different bankgroup, so need to know all same bank id bankslice in same rank
        // update bank slice
        bsc_index_2_bankslice.at(allocation_state.current_allocated_bsc)->Allocate(allocation_state.current_allocated_bank_address);
        // for every cam entry need to update
        if(!_scheduler.GetRdCam()->IsBaOrderListEmpty(allocation_state.current_allocated_bank_address.real_ba))
        {
            const OrderList& rd_cam_ba_order_list = _scheduler.GetRdCam()->GetBaOrderList(allocation_state.current_allocated_bank_address.real_ba);
            for(auto& cam_index: rd_cam_ba_order_list)
            {
                RdCamEntry* rd_cam_entry = _scheduler.GetRdCam()->GetCamEntry(cam_index);
                rd_cam_entry->SetAllocateBsc(allocation_state.current_allocated_bsc);
            }
            _scheduler.UpdateNttPip(this->GetAllocatedBscIndex(),this->GetBaAddr4BscAllocation(),
                                UpdateType::BscAllocate,true);
        }

        if(!_scheduler.GetWrCam()->IsBaOrderListEmpty(allocation_state.current_allocated_bank_address.real_ba))
        {
            const OrderList& wr_cam_ba_order_list = _scheduler.GetWrCam()->GetBaOrderList(allocation_state.current_allocated_bank_address.real_ba);
            for(auto& cam_index: wr_cam_ba_order_list)
            {
                WrCamEntry* wr_cam_entry = _scheduler.GetWrCam()->GetCamEntry(cam_index);
                wr_cam_entry->SetAllocateBsc(allocation_state.current_allocated_bsc);
            }
            _scheduler.UpdateNttPip(this->GetAllocatedBscIndex(),this->GetBaAddr4BscAllocation(),
                                UpdateType::BscAllocate,false);
        }

    }
}

unsigned
BankSliceManager::GetRdNttPageHitNum()
{
    unsigned rd_ntt_page_hit_num{0};
    for(auto bsc_index: allocated_bsc_index_set)
    {
        if(bsc_index_2_bankslice.at(bsc_index)->IsRdNttPageHit())
        {
            rd_ntt_page_hit_num++;
        }
    }
    return rd_ntt_page_hit_num;
}
unsigned
BankSliceManager::GetWrNttPageHitNum()
{
    unsigned wr_ntt_page_hit_num{0};
    for(auto bsc_index: allocated_bsc_index_set)
    {
        if(bsc_index_2_bankslice.at(bsc_index)->IsWrNttPageHit())
        {
            wr_ntt_page_hit_num++;
        }
    }
    return wr_ntt_page_hit_num;
}

bool
BankSliceManager::IsRdNttCmdAllPageHit()
{
    for(auto bsc_index: allocated_bsc_index_set)
    {
        if(!bsc_index_2_bankslice.at(bsc_index)->IsRdNttValid())
        {
            continue;
        }
        else
        {
            if(!bsc_index_2_bankslice.at(bsc_index)->IsRdNttPageHit())
            {
                return false;
            }
        }
    }
    return true;
}

bool
BankSliceManager::IsWrNttCmdAllPageHit()
{
    for(auto bsc_index: allocated_bsc_index_set)
    {
        if(!bsc_index_2_bankslice.at(bsc_index)->IsWrNttValid())
        {
            continue;
        }
        else
        {
            if(!bsc_index_2_bankslice.at(bsc_index)->IsWrNttPageHit())
            {
                return false;
            }
        }
    }
    return true;
}

bool
BankSliceManager::IsRdNttCmdExpired()
{
    for(auto bsc_index: allocated_bsc_index_set)
    {
        if(bsc_index_2_bankslice.at(bsc_index)->IsRdNttExpired())
        {
            return true;
        }
    }
    return false;
}

bool
BankSliceManager::IsWrNttCmdExpired()
{
    for(auto bsc_index: allocated_bsc_index_set)
    {
        if(bsc_index_2_bankslice.at(bsc_index)->IsWrNttExpired())
        {
            return true;
        }
    }
    return false;
}

bool
BankSliceManager::IsRdNttCmdPageHitExpired()
{
    for(auto bsc_index: allocated_bsc_index_set)
    {
        if(bsc_index_2_bankslice.at(bsc_index)->IsRdNttPageHit() &&
           bsc_index_2_bankslice.at(bsc_index)->IsRdNttExpired() )
            return true;
    }
    return false;
}

bool
BankSliceManager::IsWrNttCmdPageHitExpired()
{
    for(auto bsc_index: allocated_bsc_index_set)
    {
        if(bsc_index_2_bankslice.at(bsc_index)->IsWrNttPageHit() &&
           bsc_index_2_bankslice.at(bsc_index)->IsWrNttExpired())
            return true;
    }
    return false;
}

    } // Controller
} // dmu