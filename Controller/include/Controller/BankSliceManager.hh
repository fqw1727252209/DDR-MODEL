#ifndef __BANK_SLICE_MANAGER_HH__
#define __BANK_SLICE_MANAGER_HH__

#include <set>
#include <deque>
#include <unordered_map>
#include <memory>

#include "Configure/Configure.hh"
#include "Controller/BankSlice.hh"
#include "Controller/Scheduler.hh"
namespace dmu{
    namespace Controller{
/*
在该模块中，会实例化诸多的BankSlice:
    以vector的形式保存，直接实例化；
    将bsc index 与 BankSlice 进行一一对应
    管理Bank Slice的分配
    管理Bank Slice的回收
    建立Bank Slice和Ba 地址的关系
    由于Bank Slice的数目 和 实际DRAM Device的Bank 数目未必完全对应，所以这里需要作出区分
*/
/*
BankSlices cmds needed to be selected, so there is function to get ready cmds(including row cmd and column cmd)
the cmd will be sent to dfi interface, so how to using the phase-aware scheduling is a big problem to be solved.
as refresh and other device cmd should use dummy payload, and not need to allocate in memory manager.

*/
class BankSliceManager{
    using CAM_INDEX = unsigned;
    using BSC_INDEX = unsigned;
    public:
        explicit BankSliceManager(Scheduler& scheduler, const Configure& config);
        ~BankSliceManager()=default;

    private:
        const Configure& _config;
        std::deque<BSC_INDEX> unallocated_bsc_index_queue;
        std::set<BSC_INDEX> allocated_bsc_index_set;
        std::unordered_map<BSC_INDEX, std::unique_ptr<BankSlice>> bsc_index_2_bankslice; // Bankslice is not equal to Bank
        std::unordered_map<BSC_INDEX, BankAddress> bsc_table; // bankslice index map to Bank address
        std::unordered_map<RealBaIndex,BSC_INDEX> ba2bsc_table;

        std::set<BSC_INDEX> empty_bsc_set; // store the empty and idle bsc
        BSC_INDEX last_released_bsc_index{0};


        struct AllocationState
        {
            BankAddress current_allocated_bank_address;
            BSC_INDEX current_allocated_bsc;
            bool current_bsc_allocated;
            bool current_is_rd_allocated;
        } allocation_state;

    private:
        Scheduler& _scheduler;
        ReadyCommands bsc_ready_commands;


    public:
        // do the bsc allocation if available bsc can be allocated
        void BankSliceAllocation();
        bool IsNeedBankSliceAllocation();
        inline bool IsCurrentCycleAllocated() {return allocation_state.current_bsc_allocated;}
        // check the idle bsc, and do the bsc release;
        //BankSliceRelease() this function is able to figure out the BankSlice Release.
        void BankSliceRelease();
        bool IsNeedBankSliceRelease();

        inline bool IsAllocatedBscEmpty() {
            return allocated_bsc_index_set.empty();
        }

        inline BSC_INDEX GetRdOldestPageHitBsc(){
            return _scheduler.GetRdCam()->GetOldestPageHitCmdBsc();
        }
        inline BSC_INDEX GetRdOldestPageMissBsc(){
            return _scheduler.GetRdCam()->GetOldestPageMissCmdBsc();
        }
        inline BSC_INDEX GetWrOldestPageHitBsc(){
            return _scheduler.GetWrCam()->GetOldestPageHitCmdBsc();
        }
        inline BSC_INDEX GetWrOldestPageMissBsc(){
            return _scheduler.GetWrCam()->GetOldestPageMissCmdBsc();
        }

        inline bool IsRdAccess()
        {
            for(auto& bsc_index: allocated_bsc_index_set)
            {
                BankSlice* bank_slice = bsc_index_2_bankslice[bsc_index].get();
                if(bank_slice->IsRdRowCmdAvail())
                    return true;
            }
            return false;
        }
        inline bool IsWrAccess()
        {
            for(auto& bsc_index: allocated_bsc_index_set)
            {
                BankSlice* bank_slice = bsc_index_2_bankslice[bsc_index].get();
                if(bank_slice->IsWrRowCmdAvail())
                    return true;
            }
            return false;
        }

        inline bool IsRdWait()
        {
            for(auto& bsc_index: allocated_bsc_index_set)
            {
                BankSlice* bank_slice = bsc_index_2_bankslice[bsc_index].get();
                if(bank_slice->IsRdColCmdAvail())
                    return true;
            }
            return false;
        }
        inline bool IsWrWait()
        {
            for(auto& bsc_index: allocated_bsc_index_set)
            {
                BankSlice* bank_slice = bsc_index_2_bankslice[bsc_index].get();
                if(bank_slice->IsWrColCmdAvail())
                    return true;
            }
            return false;
        }


        inline std::set<BSC_INDEX> GetAllocatedBscSet(){   return allocated_bsc_index_set; }
        inline BankSlice* GetBsc(BSC_INDEX bsc_index) { return bsc_index_2_bankslice.at(bsc_index).get(); }


        void NttUpdate();
        inline void ResetAllocationState()
        {
            this->allocation_state.current_bsc_allocated = false;
            this->allocation_state.current_allocated_bank_address.ResetBankAddress();
            this->allocation_state.current_allocated_bsc = 0;
            this->allocation_state.current_is_rd_allocated = true;
        }
        void BscIndexAllocate();
        BSC_INDEX GetAllocatedBscIndex();
        RealBaIndex GetBaAddr4BscAllocation();
        bool GetAllocatedDirection();

        //Get Rd Ntt Page-hit cmd number
        unsigned GetRdNttPageHitNum();
        //Get Wr Ntt Page-hit cmd number
        unsigned GetWrNttPageHitNum();

        // whether all Rd Ntt cmd is page-hit
        bool IsRdNttCmdAllPageHit();
        // whether all Wr Ntt cmd is page-hit
        bool IsWrNttCmdAllPageHit();

        bool IsRdNttCmdExpired();
        bool IsWrNttCmdExpired();

        bool IsRdNttCmdPageHitExpired();
        bool IsWrNttCmdPageHitExpired();


        bool IsReadyCommandsEmpty(GlobalRdWrState global_rdwr_mode);

        sc_core::sc_time GetNextCommandTriggerTime(); // if the cmd is not zero, then should get the last cmd sending time;

        ReadyCommands& GetReadyCommands();

        void CommandUpdate(const CommandTuple::Type& sending_cmd);


        void AllocationUpdate();


        std::unordered_map<RealBaIndex,BSC_INDEX>* GetBa2BscTable() { return &ba2bsc_table;}
        std::unordered_map<BSC_INDEX, std::unique_ptr<BankSlice>>* GetBankSliceMap() { return &bsc_index_2_bankslice;}

};

    }
}
#endif