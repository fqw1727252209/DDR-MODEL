#include <cassert>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <memory>

#include "Controller/BankSliceManager.hh"
#include "Controller/CamIF.hh"
#include "Common/CommonDefine.hh"
#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "sysc/kernel/sc_simcontext.h"

namespace dmu{
    namespace Controller{
using OrderList = std::list<CAM_INDEX>;
BankSliceManager::BankSliceManager(Scheduler& scheduler, const Configure& config,unsigned pch_id)
: _config(config)
, _scheduler(scheduler)
, pch_id(pch_id)
{
    for(unsigned i = 0; i< config.controller_config->BSC_NUM; i++)
    {
        unallocated_bsc_index_queue.push_back(i);
        bsc_index_2_bankslice.emplace(static_cast<BSC_INDEX>(i),std::make_unique<BankSlice>(scheduler,config,i,this));
    }
    bsc_table.reserve(config.controller_config->BSC_NUM);
    ba2bsc_table.reserve(config.controller_config->BSC_NUM);
    bsc_ready_commands.reserve(config.controller_config->BSC_NUM);

    // Initialize bsc_cmds based on BscCmdType enum dimensions
    // First dimension: BscCmdType (Activate, Precharge, RdWr)
    bsc_cmds.resize(static_cast<size_t>(BscCmdType::Invalid));

    // Second dimension sizes based on respective command type enums:
    // - Activate uses ActCmdType (11 values: RdExpired to Invalid)
    // - Precharge uses PreCmdType (11 values: RdExpired to Invalid)
    // - RdWr uses RdWrCmdType (9 values: RdExpired to Invalid)
    bsc_cmds[static_cast<size_t>(BscCmdType::Activate)].resize(static_cast<size_t>(ActCmdType::Invalid));
    bsc_cmds[static_cast<size_t>(BscCmdType::Precharge)].resize(static_cast<size_t>(PreCmdType::Invalid));
    bsc_cmds[static_cast<size_t>(BscCmdType::RdWr)].resize(static_cast<size_t>(RdWrCmdType::Invalid));

    // Reserve capacity for each ReadyCommands vector to avoid reallocations
    for(auto& cmd_vec : bsc_cmds)
    {
        for(auto& ready_cmd : cmd_vec)
        {
            ready_cmd.reserve(config.controller_config->BSC_NUM);
        }
    }
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
    //Implement with Codex
    // TODO: support force release
    // If the empty_bsc_set is empty, but the bsc number if exceed the bsc_num_high_threshold
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
    return !(_scheduler.GetRdCam()->GetUnallocatedBscCamIndex().empty() && _scheduler.GetWrCam()->GetUnallocatedBscCamIndex().empty()) // has command not allocated bsc
           &&
           !unallocated_bsc_index_queue.empty(); // there is avail bsc can be allocated
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
        DPRINT_INFO(false,"BankSliceManager", "Bsc Allocate Stage");
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
                DPRINT_WARNING(TOP_DEBUG, "BankSliceManager", "Rd Ntt Update Failed, the selected ntt cam index: %d does not exist in Rd Cam",selected_rd_cam_index);
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
                DPRINT_WARNING(TOP_DEBUG, "BankSliceManager", "Wr Ntt Update Failed, the selected ntt cam index: %d does not exist in Wr Cam",selected_wr_cam_index);
            }
        }
    }
}

bool
BankSliceManager::IsReadyCommandsEmpty(GlobalRdWrState global_rdwr_mode)
{
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
        std::cerr << "Bank Slice Manager do command update" <<std::endl;
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
        //Implement with Codex
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
        else{
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
        {
            return true;
        }
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
        {
            return true;
        }
    }
    return false;
}

bool
BankSliceManager::HasRdNttCmdPageHitExpiredActiving()
{
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if(bank_slice->IsActiving() && bank_slice->IsRdNttPageHit() && bank_slice->IsRdNttExpired() )
        {
            return true;
        }
    }
    return false;
}
bool
BankSliceManager::HasWrNttCmdPageHitExpiredActiving(){
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if(bank_slice->IsActiving() && bank_slice->IsWrNttPageHit() && bank_slice->IsWrNttExpired() )
        {
            return true;
        }
    }
    return false;
}
bool
BankSliceManager::HasRdNttCmdPageHitFlushActiving(){
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if(_scheduler.IsRdFlush() && bank_slice->IsActiving() && bank_slice->IsRdNttPageHit() && bank_slice->IsRdNttCollision() )
        {
            return true;
        }
    }
    return false;
}
bool
BankSliceManager::HasWrNttCmdPageHitFlushActiving(){
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if(_scheduler.IsWrFlush() && bank_slice->IsActiving() && bank_slice->IsWrNttPageHit() && bank_slice->IsWrNttCollision() )
        {
            return true;
        }
    }
    return false;
}
bool
BankSliceManager::HasWrNttCmdPageHitFlush(){
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if(_scheduler.IsWrFlush() && bank_slice->IsActived() && bank_slice->IsWrNttPageHit() && bank_slice->IsWrNttCollision() )
        {
            return true;
        }
    }
    return false;
}

bool
BankSliceManager::HasHprLprCriticalPageHitActiving(){
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if( (_scheduler.IsHprCritical() || _scheduler.IsLprCritical()) && !_scheduler.IsTpwCritical() && bank_slice->IsActiving()
            && bank_slice->IsRdNttPageHit() )
        {
            return true;
        }
    }
    return false;
}
bool
BankSliceManager::HasTpwCriticalPageHitActived(){
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if( (!_scheduler.IsHprCritical() || !_scheduler.IsLprCritical()) && _scheduler.IsTpwCritical() && bank_slice->IsActiving()
            && bank_slice->IsWrNttPageHit() )
        {
            return true;
        }
    }
    return false;
}

unsigned
BankSliceManager::GetRdNttAvailRdCmdNum(){
    unsigned avail_cmd_num{0};
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if( bank_slice->IsRdColCmdAvail())
        {
            avail_cmd_num++;
        }
    }
    return avail_cmd_num;
}
unsigned
BankSliceManager::GetWrNttAvailWrCmdNum(){
    unsigned avail_cmd_num{0};
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if( bank_slice->IsWrColCmdAvail())
        {
            avail_cmd_num++;
        }
    }
    return avail_cmd_num;
}

unsigned
BankSliceManager::GetRdNttValidRdCmdNum(){
    unsigned avail_cmd_num{0};
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if( bank_slice->IsRdNttValid())
        {
            avail_cmd_num++;
        }
    }
    return avail_cmd_num;
}

unsigned
BankSliceManager::GetWrNttValidWrCmdNum(){
    unsigned avail_cmd_num{0};
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = bsc_index_2_bankslice.at(bsc_index).get();
        if( bank_slice->IsWrNttValid())
        {
            avail_cmd_num++;
        }
    }
    return avail_cmd_num;
}

unsigned
BankSliceManager::GetWrCamValidCmdNum(){
    return _scheduler.GetWrCam()->GetVaildCamSize();
}

unsigned
BankSliceManager::GetRdCamValidCmdNum(){
    return _scheduler.GetRdCam()->GetVaildCamSize();
}

bool
BankSliceManager::IsHprCritical(){
    return _scheduler.IsHprCritical();
}

bool
BankSliceManager::IsLprCritical(){
    return _scheduler.IsLprCritical();
}

bool
BankSliceManager::IsTpwCritical(){
    return _scheduler.IsTpwCritical();
}

/**
 * @brief Check if there are no ready commands to be processed
 * @param global_rdwr_mode Current global read/write state (Rd or Wr mode)
 * @param rd_advance_act Flag to enable read advance activate commands
 * @param wr_advance_act Flag to enable write advance activate commands
 * @return true if no ready commands exist, false if there are commands to process
 * * This function categorizes and collects commands into bsc_cmds based on priority:
 * 1. Expired commands (highest priority) - RdNttExpired and WrNttExpired
 * 2. Flush commands - RdNttCollision with IsRdFlush, WrNttCollision with IsWrFlush
 * 3. Critical commands - LprCritical/HprCritical for read, TpwCritical for write
 * 4. Advance activate commands - when rd_advance_act or wr_advance_act is true
 * 5. Normal commands - based on global_rdwr_mode
 */
bool
BankSliceManager::IsReadyCommandsEmpty(GlobalRdWrState global_rdwr_mode, bool rd_advance_act,bool wr_advance_act)
{
    bool is_rd_state = global_rdwr_mode == GlobalRdWrState::Rd;
    bool is_wr_state = global_rdwr_mode == GlobalRdWrState::Wr;
    ClearBscCmds();
    for(auto bsc_index: allocated_bsc_index_set)
    {
        auto bank_slice = GetBsc(bsc_index);

        if(bank_slice->IsNeedForcePre())
        {
            if(bank_slice->IsForcePreAvail())
            {
                auto bank_force_pre_cmd = bank_slice->GetForcePreCmd();
                CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::Force,bank_force_pre_cmd);
                continue;
            }
        }

        if(bank_slice->IsRefreshWaiting())
        {
            if(bank_slice->IsRefreshPreAvail())
            {
                auto bank_refresh_pre_cmd = bank_slice->GetRefreshPreCmd();
                CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::Device,bank_refresh_pre_cmd);
                continue;
            }
        }

        // ==========================================
        // Priority 1: Expired Commands
        // These commands have exceeded their latency requirements and must be processed immediately
        // ==========================================

        // Handle expired read commands (RdNttExpired)
        if(bank_slice->IsRdNttExpired())
        {
            // ACT command: Open row for expired read
            if(bank_slice->IsRdPageOpenPending())
            {
                auto bank_rd_expired_act_cmd = bank_slice->GetRdAvailCommand();
                CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::RdExpired,bank_rd_expired_act_cmd);
            }
            // PRE command: Close current row for expired read
            if(bank_slice->IsRdPageClosePending())
            {
                auto bank_rd_expired_pre_cmd = bank_slice->GetRdAvailCommand();
                CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::RdExpired,bank_rd_expired_pre_cmd);
            }
            // Column command: Execute expired read (RD/RDA)
            if(bank_slice->IsRdColCmdAvail())
            {
                auto bank_rd_expired_col_cmd = bank_slice->GetRdAvailCommand();
                CollectTypeReadyCommand<RdWrCmdType>(BscCmdType::RdWr,RdWrCmdType::RdExpired,bank_rd_expired_col_cmd);
            }
        }

        // Handle expired write commands (WrNttExpired)
        if(bank_slice->IsWrNttExpired())
        {
            // ACT command: Open row for expired write
            if(bank_slice->IsWrPageOpenPending())
            {
                auto bank_wr_expired_act_cmd = bank_slice->GetWrAvailCommand();
                CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::WrExpired,bank_wr_expired_act_cmd);
            }
            // PRE command: Close current row for expired write
            if(bank_slice->IsWrPageClosePending())
            {
                auto bank_wr_expired_pre_cmd = bank_slice->GetWrAvailCommand();
                CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::WrExpired,bank_wr_expired_pre_cmd);
            }
            // Column command: Execute expired write (WR/WRA)
            if(bank_slice->IsWrColCmdAvail())
            {
                auto bank_wr_expired_col_cmd = bank_slice->GetWrAvailCommand();
                CollectTypeReadyCommand<RdWrCmdType>(BscCmdType::RdWr,RdWrCmdType::WrExpired,bank_wr_expired_col_cmd);
            }
        }

        // ==========================================
        // Priority 2: Flush Commands (Address Collision)
        // These commands need to be flushed due to address conflicts
        // ==========================================

        // Handle read flush commands (RdNttCollision with RdFlush enabled)
        if(bank_slice->IsRdNttCollision() && _scheduler.IsRdFlush())
        {
            if(bank_slice->IsRdPageOpenPending())
            {
                auto bank_rd_flush_act_cmd = bank_slice->GetRdAvailCommand();
                CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::RdFlush,bank_rd_flush_act_cmd);
            }
            if(bank_slice->IsRdPageClosePending())
            {
                auto bank_rd_flush_pre_cmd = bank_slice->GetRdAvailCommand();
                CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::RdFlush,bank_rd_flush_pre_cmd);
            }
            if(bank_slice->IsRdColCmdAvail())
            {
                auto bank_rd_flush_col_cmd = bank_slice->GetRdAvailCommand();
                CollectTypeReadyCommand<RdWrCmdType>(BscCmdType::RdWr,RdWrCmdType::RdFlush,bank_rd_flush_col_cmd);
            }
        }

        // Handle write flush commands (WrNttCollision with WrFlush enabled)
        if(bank_slice->IsWrNttCollision() && _scheduler.IsWrFlush())
        {
            if(bank_slice->IsWrPageOpenPending())
            {
                auto bank_wr_flush_act_cmd = bank_slice->GetWrAvailCommand();
                CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::WrFlush,bank_wr_flush_act_cmd);
            }
            if(bank_slice->IsWrPageClosePending())
            {
                auto bank_wr_flush_pre_cmd = bank_slice->GetWrAvailCommand();
                CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::WrFlush,bank_wr_flush_pre_cmd);
            }
            if(bank_slice->IsWrColCmdAvail())
            {
                auto bank_wr_flush_col_cmd = bank_slice->GetWrAvailCommand();
                CollectTypeReadyCommand<RdWrCmdType>(BscCmdType::RdWr,RdWrCmdType::WrFlush,bank_wr_flush_col_cmd);
            }
        }

        // ==========================================
        // Priority 3: Critical Commands (High Priority Read)
        // Processed when LPR (Low Priority Read) or HPR (High Priority Read) is critical
        // and TPW (Turn-around for Pending Writes) is not critical
        // ==========================================
        if((_scheduler.IsLprCritical() || _scheduler.IsHprCritical()) && !_scheduler.IsTpwCritical())
        {
            // ACT: Open row for critical read
            if(bank_slice->IsRdPageOpenPending())
            {
                auto bank_rd_critical_act_cmd = bank_slice->GetRdAvailCommand();
                CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::RdCritical,bank_rd_critical_act_cmd);

            }
            // PRE: Close row for critical read (only in Rd mode)
            if(global_rdwr_mode == GlobalRdWrState::Rd)
            {
                if(bank_slice->IsRdPageClosePending())
                {
                    auto bank_rd_critical_pre_cmd = bank_slice->GetRdAvailCommand();
                    CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::RdCritical,bank_rd_critical_pre_cmd);
                }
            }
            // Column command: Execute critical read
            if(bank_slice->IsRdColCmdAvail())
            {
                auto bank_rd_critical_col_cmd = bank_slice->GetRdAvailCommand();
                CollectTypeReadyCommand<RdWrCmdType>(BscCmdType::RdWr,RdWrCmdType::RdCritical,bank_rd_critical_col_cmd);
            }
        }

        // ==========================================
        // Priority 4: Critical Commands (High Priority Write)
        // Processed when TPW is critical and LPR/HPR are not critical
        // ==========================================
        if((!_scheduler.IsLprCritical() || !_scheduler.IsHprCritical()) && _scheduler.IsTpwCritical())
        {
            // ACT: Open row for critical write
            if(bank_slice->IsWrPageOpenPending())
            {
                auto bank_wr_critical_act_cmd = bank_slice->GetWrAvailCommand();
                CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::WrCritical,bank_wr_critical_act_cmd);
            }
            // PRE: Close row for critical write (only in Wr mode)
            if(global_rdwr_mode == GlobalRdWrState::Wr)
            {
                if(bank_slice->IsWrPageClosePending())
                {
                    auto bank_wr_critical_pre_cmd = bank_slice->GetWrAvailCommand();
                    CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::WrCritical,bank_wr_critical_pre_cmd);
                }
            }
            // Column command: Execute critical write
            if(bank_slice->IsWrColCmdAvail())
            {
                auto bank_wr_critical_col_cmd = bank_slice->GetWrAvailCommand();
                CollectTypeReadyCommand<RdWrCmdType>(BscCmdType::RdWr,RdWrCmdType::WrCritical,bank_wr_critical_col_cmd);
            }
        }

        // ==========================================
        // Priority 5: Advance Activate Commands
        // These are speculative activate commands for performance optimization
        // ==========================================
        if(rd_advance_act)
        {
            if(bank_slice->IsRdPageOpenPending())
            {
                auto bank_rd_advance_act_cmd = bank_slice->GetRdAvailCommand();
                CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::RdAdvance,bank_rd_advance_act_cmd);
            }
        }
        if(wr_advance_act)
        {
            if(bank_slice->IsWrPageOpenPending())
            {
                auto bank_wr_advance_act_cmd = bank_slice->GetWrAvailCommand();
                CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::WrAdvance,bank_wr_advance_act_cmd);
            }
        }

        // ==========================================
        // Priority 6: Normal Commands based on global_rdwr_mode
        // These are regular commands processed when no urgent conditions exist
        // ==========================================

        // Normal commands in Read mode
        if(global_rdwr_mode == GlobalRdWrState::Rd)
        {
            // Normal read ACT: Open row for read when bank is not open
            if(!bank_slice->IsPageOpen())
            {
                if(bank_slice->IsRdPageOpenPending())
                {
                    auto bank_rd_normal_act_cmd = bank_slice->GetRdAvailCommand();
                    CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::RdNormal,bank_rd_normal_act_cmd);
                }
            }

            // Handle write page hit with read page conflict scenario:
            // If write is page-hit but read is page-conflict, it means the bank's current row
            // doesn't match the read command's row. Need to close current row first.
            if(bank_slice->IsWrNttPageHit() && bank_slice->IsRdNttPageConflict())
            {
                if(bank_slice->IsWrKeepPageHitNumMeet(this->GetWrNttPageHitNum()))
                {
                    if(bank_slice->IsRdPageClosePending())
                    {
                        auto bank_rd_normal_pre_cmd = bank_slice->GetRdAvailCommand();
                        CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::RdNormal,bank_rd_normal_pre_cmd);
                    }
                }
            }

            // Opportunistic write in read mode:
            // When bank has no valid read command and is not open,
            // if write command is valid and meets threshold conditions,
            // allow write to open row for future efficiency
            if(!bank_slice->IsPageOpen() && !bank_slice->IsRdNttValid())
            {
                // Check if all read commands in other banks are page-hit,
                // or if page-hit count exceeds threshold
                if(bank_slice->IsRdOpenBankNumMeet(this->GetRdNttPageHitNum())
                   || this->GetRdNttPageHitNum() == this->GetRdNttValidRdCmdNum())
                {
                    if(bank_slice->IsWrPageOpenPending())
                    {
                        auto bank_wr_normal_act_cmd = bank_slice->GetWrAvailCommand();
                        CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::WrNormal,bank_wr_normal_act_cmd);
                    }
                }
            }
        }
        // Normal commands in Write mode
        if(global_rdwr_mode == GlobalRdWrState::Wr)
        {
            // Normal write ACT: Open row for write when bank is not open
            if(!bank_slice->IsPageOpen())
            {
                if(bank_slice->IsWrPageOpenPending())
                {
                    auto bank_wr_normal_act_cmd = bank_slice->GetWrAvailCommand();
                    CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::WrNormal,bank_wr_normal_act_cmd);
                }
            }

            // Handle read page hit with write page conflict scenario:
            // If read is page-hit but write is page-conflict, close current row first
            if(bank_slice->IsRdNttPageHit() && bank_slice->IsWrNttPageConflict())
            {
                if(bank_slice->IsRdKeepPageHitNumMeet(this->GetRdNttPageHitNum()))
                {
                    auto bank_wr_normal_pre_cmd = bank_slice->GetWrAvailCommand();
                    CollectTypeReadyCommand<PreCmdType>(BscCmdType::Precharge,PreCmdType::WrNormal,bank_wr_normal_pre_cmd);
                }
            }

            // Opportunistic read in write mode:
            // Similar logic to write in read mode - when bank has no valid write
            // and meets threshold conditions, allow read to open row
            if(!bank_slice->IsPageOpen() && !bank_slice->IsWrNttValid())
            {
                // Check if all write commands in other banks are page-hit,
                // or if page-hit count exceeds threshold
                if(bank_slice->IsWrOpenBankNumMeet(this->GetWrNttPageHitNum())
                   || this->GetWrNttPageHitNum() == this->GetWrNttValidWrCmdNum())
                {
                    if(bank_slice->IsRdPageOpenPending())
                    {
                        auto bank_rd_normal_act_cmd = bank_slice->GetRdAvailCommand();
                        CollectTypeReadyCommand<ActCmdType>(BscCmdType::Activate,ActCmdType::RdNormal,bank_rd_normal_act_cmd);
                    }
                }
            }
        }

        if(bank_slice->IsRdColCmdAvail() && is_rd_state)
        {
            auto bank_rd_normal_col_cmd = bank_slice->GetRdAvailCommand();
            CollectTypeReadyCommand<RdWrCmdType>(BscCmdType::RdWr,RdWrCmdType::RdNormal,bank_rd_normal_col_cmd);
        }
        if(bank_slice->IsWrColCmdAvail() && is_wr_state)
        {
            auto bank_wr_normal_col_cmd = bank_slice->GetWrAvailCommand();
            CollectTypeReadyCommand<RdWrCmdType>(BscCmdType::RdWr,RdWrCmdType::WrNormal,bank_wr_normal_col_cmd);
        }

    }

    // 函数末尾，替换 return is_empty;
    // 当 global_rdwr_mode 为 Rd/Wr 时，仅检查对应方向的命令子类型
    for (size_t i = 0; i < bsc_cmds.size(); ++i) {
        auto bsc_type = static_cast<BscCmdType>(i);
        for (size_t j = 0; j < bsc_cmds[i].size(); ++j) {
            if(!bsc_cmds[i][j].empty())
            {
                if(global_rdwr_mode == GlobalRdWrState::Rd && (
                    j == static_cast<size_t>(RdWrCmdType::WrNormal)
                    || j == static_cast<size_t>(RdWrCmdType::WrCritical)))
                {
                    continue;
                }
                if(global_rdwr_mode == GlobalRdWrState::Wr && (
                    j == static_cast<size_t>(RdWrCmdType::RdNormal)
                    || j == static_cast<size_t>(RdWrCmdType::RdCritical)))
                {
                    continue;
                }
                return false;
            }
        }
    }
    return true;
}
    } // Controller
} // dmu