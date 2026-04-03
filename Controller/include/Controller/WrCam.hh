#ifndef __WR_CAM_HH__
#define __WR_CAM_HH__

#include <unordered_map>
#include <set>
#include <map>
#include <list>
#include <vector>

#include "Configure/Configure.hh"
#include "Controller/CamIF.hh"
#include "Controller/CamEntry.hh"

namespace dmu{
    namespace Controller{

using CAM_INDEX = unsigned;
using WrWaitingList = std::set<CAM_INDEX>;
using WaitingList = std::list<CAM_INDEX>;
using UnallocatedCamIndex = std::set<CAM_INDEX>;
using OrderList = std::list<CAM_INDEX>;
using CAM_SET = std::set<CAM_INDEX>;
using RealBaIndex = uint64_t;

class InputProcessReq;

class WrCam: public CamIF
{
    public:
        WrCam() = delete;
        ~WrCam() = default;
        explicit WrCam(const Configure& config)
        : CamIF(config.controller_config->WR_CAM_DEPTH)
        , _config(config)
        {
            tpw_cam_credit = _config.controller_config->TPW_CREDIT;
        }

        inline bool IsTpwFull() const {return cam_store.size() >= cam_depth;}
        bool IsWrCamExpired() const;
        bool HasTpwCredit() const { return tpw_cam_credit >0;}

        inline void DecreaseTpwCredit() { tpw_cam_credit--;}

        inline void IncreaseTpwCredit() { tpw_cam_credit++;}
        inline const bool IsWrCamCollision() const { return !collision_wr_cam_index_vec.empty();}
        inline const bool IsTpwCritical() const {return this->is_tpw_critical;}

        void StoreRequest(InputProcessReq& wr_request) override;

        void DeleteCamEntry(CAM_INDEX removed_cam_index) override;

        WrCamEntry* GetCamEntry(const CAM_INDEX& cam_index) override { return dynamic_cast<WrCamEntry*>(cam_store.at(cam_index).get());}

        // get the write data ready and bsc allocated cam index, and do filter in CamFilter
        OrderList GetAvailBaOrderList(RealBaIndex ba_addr) override;
        // check the ba related order list has the avail cam index to be selected
        bool IsAvailBaOrderListEmpty(RealBaIndex ba_addr) override;

        inline void SetWdataReady(CAM_INDEX cam_index)
        {
            GetCamEntry(cam_index)->data_ready = true;
        }
        inline bool IsBaListAvail(RealBaIndex ba_addr)
        {
            if(ba_cmds_order_list.find(ba_addr) == ba_cmds_order_list.end())
            {
                return false;
            }
            auto ba_list = GetBaOrderList(ba_addr);
            for(auto cam_index : ba_list)
            {
                if(GetCamEntry(cam_index)->is_allocated && GetCamEntry(cam_index)->data_ready) //FIX: Wr cmd should also be data ready
                    return true;
            }
            return false;
        }
        inline bool IsWrCamEntryWrCombSatisfied(const CAM_INDEX& cam_index) //to show whether the wr cam entry can be write combined
        {
            return (_config.controller_config->WR_COMBINE_ENABLE) &&
            (
                // Implement with Codex
                //TODO: write combine condition
                // if WR_COMBINE_ENABLE is true, then has wr cam has chance to do write combine
                // first is write data or read
                false

            );
        }
        inline const unsigned GetTpwFillLevel() const { return used_allocated_cam_index.size();}
        inline bool IsTpwAlmostFull() const {return tpw_fill_level_pos;} // detect the fill level exceed high threshold
        inline void UpdateTpwFillLevel()
        {
            if(cam_store.size() < _config.controller_config->TPW_LOW_THRESHOLD)
            {
                tpw_fill_level = false;
            }
            else if(cam_store.size() >= _config.controller_config->TPW_HIGH_THRESHOLD)
            {
                tpw_fill_level_pos = !tpw_fill_level;
                tpw_fill_level = true;
            }
        }

        inline void UpdateTpwCamFull()
        {
            if(tpw_fill_level_pos)
                tpw_cam_full = true;
            else if(!tpw_fill_level || (_config.controller_config->TPW_MAX_STARVE!=0 && tpw_run_lenth_cnt == _config.controller_config->TPW_CMD_RUNLEN))
            {
                tpw_full_negedge = tpw_cam_full;
                tpw_cam_full = false;
            }
        }
        inline unsigned GetTpwSize() const {return cam_store.size();}

        inline void TpwCmdExe()
        {
            tpw_starve_vec.clear();
            if(tpw_run_lenth_cnt < _config.controller_config->TPW_CMD_RUNLEN && is_tpw_critical)
            {
                tpw_run_lenth_cnt++;
            }
            else if(!is_tpw_critical)
            {
                tpw_run_lenth_cnt = 0;
            }
        }

        inline void TpwStarveCounter()
        {
            if(!IsTpwAvailable())
            {
                tpw_starve_vec.clear();
            }
            else
            {
                if(!IsTpwStarve())
                    tpw_starve_vec.push_back(sc_core::sc_time_stamp());
            }
        }
        // update the wr cam critical state;
        void UpdateWrCriticalState();

        inline bool IsTpwAvailable()
        {
            for(auto& tpw_cam_index: used_allocated_cam_index)
            {
                if(GetCamEntry(tpw_cam_index)->is_allocated && GetCamEntry(tpw_cam_index)->data_ready) //FIX: need add the data ready decision
                    return true;
                else
                    continue;
            }
            return false;
        }
        inline bool IsWrCamAvailable()
        {
            for(auto tpw_cam_index: used_allocated_cam_index)
            {
                auto wr_cam_entry = GetCamEntry(tpw_cam_index);
                if( wr_cam_entry->is_allocated && wr_cam_entry->data_ready ) // FIX: wr_cam_entry->data_ready need add the data ready decision
                {
                    return true;
                }
            }
            return false;
        }

        // push back all the wr cam collision cam index
        inline void AddWrCollisionCamIndex(CAM_INDEX collision_cam_index) {collision_wr_cam_index_vec.push_back(collision_cam_index);}
        // push back the wr combine cam index
        inline void AddWrCombineCamIndex(CAM_INDEX wr_combine_cam_index)  {write_combine_cam_index_vec.push_back(wr_combine_cam_index);}
        // clear all the collision cam index
        inline void ClearWrCollisionCamIndex() { collision_wr_cam_index_vec.clear();write_combine_cam_index_vec.clear();}
        // show the wr cam is collision busy with pip buffer, and stall the pip buffer req into cam
        inline bool IsWrCamBusy() const   {return !collision_wr_cam_index_vec.empty();}
        inline std::vector<CAM_INDEX> GetWrCollisionCamIndex() const {return collision_wr_cam_index_vec;}
        // show the wr cam happen the write combine
        inline bool HasWrCombine() const    {return !write_combine_cam_index_vec.empty();}

    private:
        const Configure& _config;

        std::vector<unsigned> collison_cam_index;
        std::vector<unsigned> time_expired_cam_index;

        unsigned tpw_cam_credit;

        bool tpw_fill_level{false};
        unsigned tpw_run_lenth_cnt{0};
        bool tpw_fill_level_pos{false};

        bool is_cam_expired;
        bool is_cam_collision;

        bool is_tpw_critical{false};

        // bool is_tpw_almost_full;
        std::vector<sc_core::sc_time> tpw_starve_vec;
        inline bool IsTpwStarve()
        {
            return _config.controller_config->TPW_STARVE_COUNT_MODE ? tpw_starve_vec.size() >= _config.controller_config->TPW_MAX_STARVE
                 : (*tpw_starve_vec.begin()) + _config.controller_config->TPW_MAX_STARVE * _config.mem_spec->tCK_mc >= sc_core::sc_time_stamp();
        }

        bool tpw_cam_full{false};
        bool tpw_full_negedge{false};

        std::vector<CAM_INDEX> collision_wr_cam_index_vec;
        std::vector<CAM_INDEX> write_combine_cam_index_vec;

};

    }
}
#endif