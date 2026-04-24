#ifndef __SCHEDULER_HH__
#define __SCHEDULER_HH__

#include <cassert>
#include <memory>
#include <deque>
#include <set>
/*
 scheduler is will include
 1. WR CAM and RD CAM
 2. WR CAM Filter and RD CAM Filter
 3. Next-Transaction-Table(NTT)

 Note: scheduler may consume 2 cycle when a cmd to be inserted into cam and updated to the NTT

 */
#include "Controller/CamIF.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Common/CommonDefine.hh"
#include "Configure/Configure.hh"
#include "Controller/RdCam.hh"
#include "Controller/WrCam.hh"
#include "Controller/CamFilter.hh"
#include "sysc/kernel/sc_time.h"
#include "sysc/utils/sc_report.h"

namespace dmu{
    namespace Controller{
class BankSlice;

class Scheduler{

    public:
        explicit Scheduler(const Configure& config);
        ~Scheduler();

        // Get all Rd Cam used cam index
        inline const std::set<unsigned>& GetRdCamIndex() { return rd_cam->GetUsedCamIndex(); }
        // Get all Wr Cam used cam index
        inline const std::set<unsigned>& GetWrCamIndex() { return wr_cam->GetUsedCamIndex(); }
        RdCam* GetRdCam() const{ return rd_cam.get(); }
        WrCam* GetWrCam() const{ return wr_cam.get(); }
        inline bool IsBscMatch(RealBaIndex ba_addr) { return ba2bsc_table->count(ba_addr)>0; }
        void StoreRdRequest(InputProcessReq& rd_input_request);
        void StoreWrRequest(InputProcessReq& wr_input_request);

        RdCamFilter* GetRdCamFilter() const { return rd_cam_filter.get(); }
        WrCamFilter* GetWrCamFilter() const { return wr_cam_filter.get(); }


        inline bool HasLprCredit() const { return rd_cam->HasLprCredit(); }
        inline bool HasHprCredit() const { return rd_cam->HasHprCredit(); }
        inline bool HasTpwCredit() const { return wr_cam->HasTpwCredit(); }

        inline bool IsRdFlush() {return rd_cam->IsRdCamCollision();}
        //when rd flush, then will mask wr flush
        inline bool IsWrFlush() {return wr_cam->IsWrCamCollision() && !rd_cam->IsRdCamCollision(); }

        inline bool IsRdCamExpired() {return rd_cam->IsRdCamExpired();}
        inline bool IsWrCamExpired() {return wr_cam->IsWrCamExpired();}

        inline bool IsLprCritical() {return rd_cam->IsLprCritical();}
        inline bool IsHprCritical() {return rd_cam->IsHprCritical();}
        inline bool IsTpwCritical() {return wr_cam->IsTpwCritical();}

        inline bool IsHprFull() const {return rd_cam->IsHprFull();}
        inline bool IsLprFull() const {return rd_cam->IsLprFull();}
        inline bool IsTpwFull() const {return wr_cam->IsTpwFull();}

        inline void DeleteRdCamEntry(CAM_INDEX removed_cam_index) { rd_cam->DeleteCamEntry(removed_cam_index);}
        inline void DeleteWrCamEntry(CAM_INDEX removed_cam_index) { wr_cam->DeleteCamEntry(removed_cam_index);}


        void UpdateNttPip(BSC_INDEX bsc_index,RealBaIndex ba_addr, UpdateType update_type, bool is_rd);
        void UpdateRdNttPip(BSC_INDEX bsc_index,RealBaIndex ba_addr, UpdateType update_type);
        void UpdateRdNttPip(RealBaIndex ba_addr, UpdateType update_type);

        void UpdateWrNttPip(BSC_INDEX bsc_index,RealBaIndex ba_addr, UpdateType update_type);
        void UpdateWrNttPip(RealBaIndex ba_addr, UpdateType update_type);

        // inline bool IsNeedUpdate() {return !rd_updated_bsc_set.empty() | !wr_updated_bsc_set.empty();}
        inline bool IsNeedUpdate()
        {
            return ntt_store.NextTriggerTime() == sc_core::sc_time_stamp();
        }
        inline sc_core::sc_time GetNextUpdateTime() {return ntt_store.NextTriggerTime();}

        // inline void ResetUpdate()
        // {
        //     rd_updated_bsc_set.clear();
        //     wr_updated_bsc_set.clear();
        // }
        inline void ResetUpdate()
        {
            ntt_store.ClearNtt();
        }
        // inline const std::set<BSC_INDEX>& GetUpdatedBscSet(bool is_rd) {return is_rd ? rd_updated_bsc_set : wr_updated_bsc_set;}
        inline const std::set<BSC_INDEX>& GetUpdatedBscSet(bool is_rd)
        {
            auto& ntt_info = ntt_store.ntt_temp_store.at(sc_core::sc_time_stamp());
            return is_rd ? ntt_info.rd_updated_bsc_set : ntt_info.wr_updated_bsc_set;
        }

        const CAM_INDEX GetUpdateNttTemp(BSC_INDEX bsc_index, bool is_rd);
        const CAM_INDEX GetUpdateRdNttTemp(BSC_INDEX bsc_index);
        const CAM_INDEX GetUpdateWrNttTemp(BSC_INDEX bsc_index);

        // Get the Real Bank Index 2 Bsc index mapping table
        void RegisterBa2BscTable(std::unordered_map<RealBaIndex,BSC_INDEX>* _ba2bsc_table) { ba2bsc_table = _ba2bsc_table;}
        // Get the Bsc Slice pointer
        void RegisterBscSliceMap(std::unordered_map<BSC_INDEX, std::unique_ptr<BankSlice>>* _bsc_index_2_bankslice) { bsc_index_2_bankslice = _bsc_index_2_bankslice;}
        // Real Bank Index Map function

    private:
        std::unique_ptr<RdCam> rd_cam;
        // RdCam rd_cam;
        std::unique_ptr<RdCamFilter> rd_cam_filter;
        // RdCamFilter rd_cam_filter;
        std::unique_ptr<WrCam> wr_cam;
        // WrCam wr_cam;
        std::unique_ptr<WrCamFilter> wr_cam_filter;
        // WrCamFilter wr_cam_filter;
        const sc_core::sc_time mc_cycle_time;

        std::unordered_map<RealBaIndex,BSC_INDEX>* ba2bsc_table{nullptr};
        std::unordered_map<BSC_INDEX, std::unique_ptr<BankSlice>>* bsc_index_2_bankslice{nullptr};
    private:
        // std::vector<std::vector<std::deque<CAM_INDEX>>> rd_update_ntt_temp; // bsc -
        // std::vector<std::vector<std::deque<CAM_INDEX>>> wr_update_ntt_temp;

        // std::set<BSC_INDEX> rd_updated_bsc_set;
        // std::set<BSC_INDEX> wr_updated_bsc_set;

        class Ntt
        {
            using ntt_updated_cam_index = std::deque<CAM_INDEX>; // store the updated selected cam index
            using ntt_updated_type_vec = std::vector<ntt_updated_cam_index>; // store the different type of ntt update
            using ntt_updated_bsc_vec = std::vector<ntt_updated_type_vec>; // store different bsc of ntt update
            public:
            // store rd ntt update temp info
            // store wr ntt update temp info
            struct NttUpdateInfo
            {
                // std::vector<std::vector<std::deque<CAM_INDEX>>> updated_ntt_temp;
                // std::set<BSC_INDEX> updated_bsc_set; // record the which bsc do the ntt updated action
                
                std::vector<std::vector<std::deque<CAM_INDEX>>> rd_updated_ntt_temp;
                std::set<BSC_INDEX> rd_updated_bsc_set;
                std::vector<std::vector<std::deque<CAM_INDEX>>> wr_updated_ntt_temp;
                std::set<BSC_INDEX> wr_updated_bsc_set;
                explicit NttUpdateInfo(unsigned bsc_num)
                {
                    wr_updated_ntt_temp = std::vector<std::vector<std::deque<BSC_INDEX>>>(bsc_num,
                                          std::vector<std::deque<CAM_INDEX>>(static_cast<size_t>(UpdateType::Invalid)));
                    rd_updated_ntt_temp = std::vector<std::vector<std::deque<BSC_INDEX>>>(bsc_num,
                                          std::vector<std::deque<CAM_INDEX>>(static_cast<size_t>(UpdateType::Invalid)));
                }
            };
            std::map<sc_core::sc_time, NttUpdateInfo> ntt_temp_store;
            std::deque<sc_core::sc_time> ntt_time_deque;
            const BSC_INDEX _bsc_num;
            explicit Ntt(unsigned bsc_num): _bsc_num(bsc_num){}
            inline const sc_core::sc_time NextTriggerTime() {return ntt_time_deque.empty() ? sc_core::sc_max_time() : *(ntt_time_deque.begin());}
            inline void RecordNttsBsc(bool is_rd, BSC_INDEX updated_bsc, const sc_core::sc_time& updating_time)
            {
                if(ntt_temp_store.find(updating_time) == ntt_temp_store.end())
                {
                    ntt_temp_store.emplace(updating_time,NttUpdateInfo(_bsc_num));
                    ntt_time_deque.push_back(updating_time);
                }
                auto& ntt_update_info = ntt_temp_store.at(updating_time);
                if(is_rd)
                    ntt_update_info.rd_updated_bsc_set.insert(updated_bsc);
                else
                    ntt_update_info.wr_updated_bsc_set.insert(updated_bsc);
            }
            inline void WrNttStore(BSC_INDEX bsc_index, UpdateType update_type, CAM_INDEX updated_cam_index,const sc_core::sc_time& updating_time)
            {
                auto& ntt_update_info = ntt_temp_store.at(updating_time);
                ntt_update_info.wr_updated_ntt_temp.at(bsc_index).at(static_cast<size_t>(update_type)).push_back(updated_cam_index);
            }
            inline void RdNttStore(BSC_INDEX bsc_index, UpdateType update_type, CAM_INDEX updated_cam_index,const sc_core::sc_time& updating_time)
            {
                auto& ntt_update_info = ntt_temp_store.at(updating_time);
                ntt_update_info.rd_updated_ntt_temp.at(bsc_index).at(static_cast<size_t>(update_type)).push_back(updated_cam_index);
            }
            inline void ClearNtt()
            {
                sc_core::sc_time current_time = sc_core::sc_time_stamp();
                sc_assert(ntt_time_deque.front() == current_time);
                ntt_time_deque.pop_front();
                ntt_temp_store.erase(current_time);
            }
            inline const CAM_INDEX GetRdNtt(BSC_INDEX updated_bsc_index)
            {
                sc_core::sc_time current_time = sc_core::sc_time_stamp();
                auto& rd_ntt_update_info = ntt_temp_store.at(current_time).rd_updated_ntt_temp;
                auto& ntt_temp = rd_ntt_update_info.at(updated_bsc_index);
                CAM_INDEX selected_ntt_cam_index;
                if(!ntt_temp.at(static_cast<size_t>(UpdateType::CmdExe)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::CmdExe)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Rd Ntt, Type: CmdExe, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else if(!ntt_temp.at(static_cast<size_t>(UpdateType::Pre_Act)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::Pre_Act)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Rd Ntt, Type: Pre_Act, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else if(!ntt_temp.at(static_cast<size_t>(UpdateType::NewCmdStore)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::NewCmdStore)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Rd Ntt, Type: NewCmdStore, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else if(!ntt_temp.at(static_cast<size_t>(UpdateType::BscAllocate)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::BscAllocate)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Rd Ntt, Type: BscAllocate, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else if(!ntt_temp.at(static_cast<size_t>(UpdateType::BrokenTerminate)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::BrokenTerminate)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Rd Ntt, Type: BrokenTerminate, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else
                {
                    std::cerr << "Scheduler: " << " no valid ntt temp in the rd deque "<<std::endl;
                    std::abort();
                }
                return selected_ntt_cam_index;
            }
            inline const CAM_INDEX GetWrNtt(BSC_INDEX updated_bsc_index)
            {
                sc_core::sc_time current_time = sc_core::sc_time_stamp();
                auto& wr_ntt_update_info = ntt_temp_store.at(current_time).wr_updated_ntt_temp;
                auto& ntt_temp = wr_ntt_update_info.at(updated_bsc_index);
                CAM_INDEX selected_ntt_cam_index;
                if(!ntt_temp.at(static_cast<size_t>(UpdateType::CmdExe)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::CmdExe)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Wr Ntt, Type: CmdExe, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else if(!ntt_temp.at(static_cast<size_t>(UpdateType::Pre_Act)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::Pre_Act)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Wr Ntt, Type: Pre_Act, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else if(!ntt_temp.at(static_cast<size_t>(UpdateType::WrCombine)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::WrCombine)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Wr Ntt, Type: WrCombine, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else if(!ntt_temp.at(static_cast<size_t>(UpdateType::NewCmdStore)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::NewCmdStore)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Wr Ntt, Type: NewCmdStore, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else if(!ntt_temp.at(static_cast<size_t>(UpdateType::BscAllocate)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::BscAllocate)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Wr Ntt, Type: BscAllocate, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else if(!ntt_temp.at(static_cast<size_t>(UpdateType::BrokenTerminate)).empty())
                {
                    selected_ntt_cam_index = ntt_temp.at(static_cast<size_t>(UpdateType::BrokenTerminate)).front();
                    DPRINT_INFO(TOP_DEBUG, "Ntt", "Get Wr Ntt, Type: BrokenTerminate, selected ntt cam index: %d",selected_ntt_cam_index);
                }
                else
                {
                    std::cerr << "Scheduler: " << " no valid ntt temp in the wr deque "<<std::endl;
                    std::abort();
                }
                return selected_ntt_cam_index;
            }
        } ntt_store;

};

}// Controller
} // dmu

#endif