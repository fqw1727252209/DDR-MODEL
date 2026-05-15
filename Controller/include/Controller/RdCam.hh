#ifndef __RD_CAM_HH__
#define __RD_CAM_HH__

// #include <map>
#include <unordered_map>
#include <list>
#include <vector>
#include <deque>
#include <set>

#include "Common/logger.hh"
#include "Configure/Configure.hh"
#include "Controller/CamEntry.hh"
#include "Controller/CamIF.hh"
namespace dmu{
    namespace Controller{

using CAM_INDEX = unsigned;
using RdWaitingList = std::set<CAM_INDEX>;
using WaitingList = std::list<CAM_INDEX>;
using UnallocatedCamIndex = std::set<CAM_INDEX>;
using OrderList = std::list<CAM_INDEX>;
using OrderEntryList = std::list<CamEntry*>;
using CAM_SET = std::set<CAM_INDEX>;

using RealBaIndex = uint64_t;

class InputProcessReq;

class RdCam: public CamIF
{
    public:
        RdCam() = delete;
        ~RdCam() = default;
        // explicit RdCam(const unsigned& cam_depth);
        explicit RdCam(const Configure& config,unsigned pch_id);

        void StoreRequest(InputProcessReq& rd_request) override;

        RdCamEntry* GetCamEntry(const CAM_INDEX& cam_index) override { return dynamic_cast<RdCamEntry*>(cam_store.at(cam_index).get());}

        void DeleteCamEntry(CAM_INDEX removed_cam_index) override;

        // get the write date ready and bsc allocated cam index, and do filter in CamFilter
        OrderList GetAvailBaOrderList(RealBaIndex ba_addr) override;
        // check the ba related order list has the avail cam index to be selected
        bool IsAvailBaOrderListEmpty(RealBaIndex ba_addr) override;

        unsigned GetVaildCamSize();

        bool HasLprCredit() const { return lpr_cam_credit > 0;}
        bool HasHprCredit() const { return hpr_cam_credit > 0;}

        inline void DecreaseLprCredit() { lpr_cam_credit--;}
        inline void DecreaseHprCredit() { hpr_cam_credit--;}

        inline void IncreaseLprCredit() { lpr_cam_credit++;}
        inline void IncreaseHprCredit() { hpr_cam_credit++;}

        inline bool IsBaListAvail(RealBaIndex ba_addr)
        {
            if(ba_cmds_order_list.find(ba_addr) == ba_cmds_order_list.end())
            {
                return false;
            }
            
            auto ba_list = GetBaOrderList(ba_addr);
            for(auto cam_index : ba_list)
            {
                if(GetCamEntry(cam_index)->is_allocated)
                    return true;
            }
            return false;
        }

        inline bool IsHprFull() const { return hpr_cmd_set.size() >= _config.controller_config->HPR_CREDIT;}
        inline bool IsLprFull() const { return lpr_cmd_set.size() >= _config.controller_config->LPR_CREDIT;}
        bool IsRdCamExpired() const;
        inline bool IsRdCamCollision() const { return !collision_rd_cam_index_vec.empty();}

        bool IsLprCritical() const;
        bool IsHprCritical() const;
        inline bool IsPageHitLimit() const {return true;}

        inline unsigned GetHprSize() const { return hpr_cmd_set.size();}
        inline unsigned GetLprSize() const { return lpr_cmd_set.size();}
        inline bool IsHprAlmostFull() const { return hpr_fill_level_pos;} // detect the fill level exceed high threshold
        inline bool IsLprAlmostFull() const { return lpr_fill_level_pos;} // detect the fill level exceed high threshold

        inline void UpdateHprFillLevel()
        {
            if(hpr_fill_level)
            {
                if(hpr_cmd_set.size() < _config.controller_config->HPR_LOW_THRESHOLD)
                {
                    hpr_fill_level = false;
                    hpr_fill_level_pos = false;
                }
                else if(hpr_cmd_set.size() >= _config.controller_config->HPR_HIGH_THRESHOLD)
                {
                    hpr_fill_level = true;
                    hpr_fill_level_pos = false;
                }
            }
            else
            {
                if(hpr_cmd_set.size() < _config.controller_config->HPR_LOW_THRESHOLD)
                {
                    hpr_fill_level = false;
                    hpr_fill_level_pos = false;
                }
                else if(hpr_cmd_set.size() >= _config.controller_config->HPR_HIGH_THRESHOLD)
                {
                    hpr_fill_level = true;
                    hpr_fill_level_pos = true;
                }
            }
            // if(hpr_cmd_set.size() < _config.controller_config->HPR_LOW_THRESHOLD)
            // {
            //     hpr_fill_level = false;
            // }
            // else if(hpr_cmd_set.size() >= _config.controller_config->HPR_HIGH_THRESHOLD)
            // {
            //     hpr_fill_level_pos = !hpr_fill_level;
            //     hpr_fill_level = true;
            // }
        }

        inline void UpdateLprFillLevel()
        {
            if(lpr_cmd_set.size() < _config.controller_config->LPR_LOW_THRESHOLD)
            {
                lpr_fill_level = false;
            }
            else if(lpr_cmd_set.size() >= _config.controller_config->LPR_HIGH_THRESHOLD)
            {
                lpr_fill_level_pos = !lpr_fill_level;
                lpr_fill_level = true;
            }
        } // when in fill level, only below the low thresold will exsist fill level state

        inline void UpdateHprCamFull()
        {
            if(hpr_fill_level_pos)
            {
                if(!hpr_cam_full)
                {
                    DMU_LOG_INFO_NF("Scheduler_"+std::to_string(pch_id),"[PCH:%d] Hpr enter Cam Full for fill level pos",pch_id);
                }
                hpr_cam_full = true;
            }
            else if(!hpr_fill_level || (_config.controller_config->HPR_MAX_STARVE!=0 && hpr_run_lenth_cnt == _config.controller_config->HPR_CMD_RUNLEN))
            {
                hpr_full_negedge = hpr_cam_full;
                if(hpr_cam_full)
                {
                    DMU_LOG_INFO_NF("Scheduler_"+std::to_string(pch_id),"[PCH:%d] Hpr exit Cam Full for: fill level under low threshold=%d,run length cnt achieve:%d, run length counter:%d",pch_id,!hpr_fill_level,hpr_run_lenth_cnt == _config.controller_config->HPR_CMD_RUNLEN,hpr_run_lenth_cnt);
                }
                hpr_cam_full = false;
            }
        }
        inline void UpdateLprCamFull()
        {
            if(lpr_fill_level_pos)
            {
                lpr_cam_full = true;
            }
            else if(!lpr_fill_level || (_config.controller_config->LPR_MAX_STARVE!=0 && lpr_run_lenth_cnt == _config.controller_config->LPR_CMD_RUNLEN))
            {
                lpr_full_negedge = lpr_cam_full;
                lpr_cam_full = false;
            }
        }

        inline void HprCmdExe()
        {
            hpr_starve_vec.clear();
            if(hpr_run_lenth_cnt < _config.controller_config->HPR_CMD_RUNLEN && is_hpr_critical)
            {
                hpr_run_lenth_cnt++;
            }
            else if(!is_hpr_critical)
            {
                hpr_run_lenth_cnt = 0;
            }

            // if(!is_hpr_critical)
            // {
            //     hpr_run_lenth_cnt = 0;
            // }
            // else if(hpr_run_lenth_cnt < _config.controller_config->HPR_CMD_RUNLEN)
            // {
            //     hpr_run_lenth_cnt++;
            // }
        }

        // do the HPR starve counter,if in cycle-counter mode,record the first cmd time
        // if in cmd-counter mode, record lpr/tpw cmd exe number(exe time)
        inline void HprStarveCounter()
        {
            if(!IsHprAvailable())
            {
                hpr_starve_vec.clear();
            }
            else
            {
                if(!IsHprStarve())
                    hpr_starve_vec.push_back(sc_core::sc_time_stamp());
            }
        }

        // lpr cmd exe
        inline void LprCmdExe()
        {
            lpr_starve_vec.clear();
            if(lpr_run_lenth_cnt < _config.controller_config->LPR_CMD_RUNLEN && is_lpr_critical)
            {
                lpr_run_lenth_cnt++;
            }
            else if(!is_lpr_critical)
            {
                lpr_run_lenth_cnt = 0;
            }
        }

        // do the LPR starve counter,if in cycle-counter mode,record the first cmd time
        // if in cmd-counter mode, record hpr/tpw cmd exe number(exe time)
        inline void LprStarveCounter()
        {
            if(!IsLprAvailable())
            {
                lpr_starve_vec.clear();
            }
            else
            {
                if(!IsLprStarve())
                    lpr_starve_vec.push_back(sc_core::sc_time_stamp());
            }
        }

        void UpdateRdCriticalState();

        inline bool IsHprAvailable()
        {
            for(auto& hpr_cam_index: hpr_cmd_set)
            {
                if(GetCamEntry(hpr_cam_index)->is_allocated)
                    return true;
                else
                    continue;
            }
            return false;
        }

        inline bool IsLprAvailable()
        {
            for(auto& lpr_cam_index: lpr_cmd_set)
            {
                if(GetCamEntry(lpr_cam_index)->is_allocated)
                    return true;
                else
                    continue;
            }
            return false;
        }

        // push back all the rd cam collision cam index
        inline void AddRdCollisionCamIndex(CAM_INDEX collision_cam_index) {collision_rd_cam_index_vec.push_back(collision_cam_index);}
        // clear all the collision cam index
        inline void ClearRdCollisionCamIndex() {collision_rd_cam_index_vec.clear();}
        inline std::vector<CAM_INDEX> GetRdCollisionCamIndex() const {return collision_rd_cam_index_vec;}
        //
        inline bool IsRdCamBusy() const {return !collision_rd_cam_index_vec.empty();}
    private:
        const Configure& _config;

        std::set<CAM_INDEX> hpr_cmd_set;
        std::set<CAM_INDEX> lpr_cmd_set;//for lpr and gpr;

        std::deque<CAM_INDEX> collison_cam_index;
        std::deque<CAM_INDEX> time_expired_cam_index;

        unsigned lpr_cam_credit;
        unsigned hpr_cam_credit;

        bool is_cam_expired{false};
        bool is_cam_collision{false};

        bool is_lpr_critical{false};
        bool is_hpr_critical{false};

        bool lpr_fill_level{false};
        bool hpr_fill_level{false};

        bool hpr_fill_level_pos{false};
        bool lpr_fill_level_pos{false};

        unsigned hpr_run_lenth_cnt{0};
        unsigned lpr_run_lenth_cnt{0};

        std::vector<sc_core::sc_time> hpr_starve_vec;
        std::vector<sc_core::sc_time> lpr_starve_vec;

        inline bool IsHprStarve()
        { return _config.controller_config->HPR_STARVE_COUNT_MODE ? hpr_starve_vec.size() >= _config.controller_config->HPR_MAX_STARVE
            : (*hpr_starve_vec.begin()) + _config.controller_config->HPR_MAX_STARVE * _config.mem_spec->tCK_mc >= sc_core::sc_time_stamp(); }

        inline bool IsLprStarve()
        { return _config.controller_config->LPR_STARVE_COUNT_MODE ? lpr_starve_vec.size() >= _config.controller_config->LPR_MAX_STARVE
            : (*lpr_starve_vec.begin()) + _config.controller_config->LPR_MAX_STARVE * _config.mem_spec->tCK_mc >= sc_core::sc_time_stamp(); }

        bool hpr_cam_full{false};
        bool lpr_cam_full{false};

        bool hpr_full_negedge{false};
        bool lpr_full_negedge{false};

        std::vector<CAM_INDEX> collision_rd_cam_index_vec; // record the collision rd cam index

};
    }
}
#endif