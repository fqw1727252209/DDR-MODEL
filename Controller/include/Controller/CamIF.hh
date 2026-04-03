#ifndef __CAM_IF_HH__
#define __CAM_IF_HH__

#include <unordered_map>

#include <list>
#include <deque>
#include <set>
#include <memory>

#include "Controller/CamEntry.hh"

namespace dmu{
    namespace Controller{
using CAM_INDEX = unsigned;
using BSC_INDEX = unsigned;
using WaitingList = std::list<CAM_INDEX>;
using OrderList = std::list<CAM_INDEX>;
using OrderEntryList = std::list<CamEntry*>;
using UnallocatedCamIndex = std::set<CAM_INDEX>;
using RealBaIndex = uint64_t;
class CamIF
{
    public:
        CamIF() = delete;
        virtual ~CamIF() = default;
        explicit CamIF(const unsigned& _cam_depth)
        : is_cam_collision(false)
        , is_cam_expired(false)
        , cam_depth(_cam_depth)
        {
            // cam_store.reserve(cam_depth);
        }

    protected:
        const unsigned cam_depth; // the cam depth
        std::unordered_map<CAM_INDEX, std::unique_ptr<CamEntry>> cam_store; // store the CamEntry and build the CamIndex <--> CamEntry map
        std::set<CAM_INDEX> used_allocated_cam_index; // record the used cam index
        OrderList cam_order_list; // record the cam entering order
        std::unordered_map<RealBaIndex,OrderList> ba_cmds_order_list; // store the Ba Addr related Cam Cmd Order

        CAM_INDEX oldest_page_hit_cam_index; // find oldest page-hit and allocated
        CAM_INDEX oldest_page_miss_cam_index; // find oldest page-miss and allocated

        bool is_cam_collision{false};
        bool is_cam_expired{false};

        std::unordered_map<RealBaIndex,unsigned> num_of_page_hit_cmd_per_bank; // record the

    public:
        // get the cam cmd entring order
        inline const OrderList& GetOrderList() const {return this->cam_order_list;}
        // whether the Cam OrderList is empty, also mean whether the cam is empty
        inline bool IsOrderListEmpty() const {return this->cam_order_list.empty();}
        // decide the ba address related cam order list is whether empty
        inline bool IsBaOrderListEmpty(RealBaIndex ba_addr) const
        {
            if(ba_cmds_order_list.find(ba_addr) == ba_cmds_order_list.end())
            {
                return true;
            }
            return this->ba_cmds_order_list.at(ba_addr).empty();
        }
        // get the (ba address/ bank slice) related cam order list
        inline const OrderList& GetBaOrderList(RealBaIndex ba_addr)
        {
            // DPRINT_INFO(true, "Wr Cam Ba Get: ", "stage 0");
            // assert(ba_cmds_order_list.find(ba_addr) != ba_cmds_order_list.end());
            return ba_cmds_order_list.at(ba_addr);
        }
        inline void SetBaPageHit(RealBaIndex ba_addr, unsigned open_page)
        {
            if(IsBaOrderListEmpty(ba_addr))
                return;
            auto ba_order_list = this->GetBaOrderList(ba_addr);
            for(auto cam_index: ba_order_list)
            {
                auto cam_enrty = cam_store.at(cam_index).get();
                if(cam_enrty->sdram_addr.row == open_page)
                {
                    cam_enrty->SetPageOpen();
                }
                else
                {
                    cam_enrty->SetPageClose();
                }
            }
        }
        inline void SetBaPageClose(RealBaIndex ba_addr)
        {
            if(IsBaOrderListEmpty(ba_addr))
                return;
            auto ba_order_list = this->GetBaOrderList(ba_addr);
            for(auto cam_index: ba_order_list)
            {
                auto cam_enrty = cam_store.at(cam_index).get();
                cam_enrty->SetPageClose();
            }
        }

        // get the used cam index for cam
        inline const std::set<unsigned>& GetUsedCamIndex() const {return used_allocated_cam_index;}
        inline bool IsCamExist(CAM_INDEX cam_index) const {
            return used_allocated_cam_index.find(cam_index) != used_allocated_cam_index.end()
                   && cam_store.count(cam_index) !=0;
        }

        inline OrderEntryList GetBaOrderEntryList(RealBaIndex ba_addr)
        {
            OrderEntryList ba_order_entry_list;
            for(auto& cam_index: this->ba_cmds_order_list.at(ba_addr))
            {
                ba_order_entry_list.emplace_back(cam_store.at(cam_index).get());
            }
            return ba_order_entry_list;
        }

        // get the bank slice related cam order list, and filter the avail cam index, rd ok and write ok
        virtual OrderList GetAvailBaOrderList(RealBaIndex ba_addr) = 0;
        virtual bool IsAvailBaOrderListEmpty(RealBaIndex ba_addr) = 0;

        virtual CamEntry* GetCamEntry(const CAM_INDEX& cam_index) = 0;

        virtual void StoreRequest(InputProcessReq& requets) = 0; // need to override

        virtual void DeleteCamEntry(CAM_INDEX removed_cam_index) = 0;

        const WaitingList GetUnallocatedBscCamIndex()
        {
            std::list<CAM_INDEX> unallocated_bsc_cam_index;
            for(const auto& cam_index: cam_order_list)
            {
                CamEntry* cam_entry = this->GetCamEntry(cam_index);
                if(!cam_entry->IsAllocated())
                {
                    unallocated_bsc_cam_index.push_back(cam_index);
                }
                else
                {
                    continue;
                }
            }
            return unallocated_bsc_cam_index;
        }

        BSC_INDEX GetOldestPageHitCmdBsc() // this situation may be empty value
        {
            for(auto it = cam_order_list.cbegin(); it != cam_order_list.cend();it++)
            {
                if(this->GetCamEntry(*it)->IsPageHit() && (this->GetCamEntry(*it)->IsAllocated()))
                {
                    return this->GetCamEntry(*it)->allocated_bsc_index;
                }
            }
            // Implement with Codex
            //TODO: return empty value
        }
        BSC_INDEX GetOldestPageMissCmdBsc() // this situation may be empty value
        {
            for(auto it = cam_order_list.cbegin(); it != cam_order_list.cend();it++)
            {
                if(!(this->GetCamEntry(*it)->IsPageHit()) && (this->GetCamEntry(*it)->IsAllocated()))
                {
                    return this->GetCamEntry(*it)->allocated_bsc_index;
                }
            }
            // Implement with Codex
            //TODO: return empty value
        }
        // Is Cam Empty
        inline bool IsCamEmpty() const { return cam_store.empty();}


};
    }
}
#endif