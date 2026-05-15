#ifndef __MRDIMM_MODE_SWITCH_HH__
#define __MRDIMM_MODE_SWITCH_HH__

#include "Configure/Configure.hh"
#include "Controller/BankSliceManager.hh"
#include "Controller/Scheduler.hh"
#include <algorithm>
#include <memory>
namespace dmu{
    namespace Controller{

        class MrdimmModeSwitch{
            using scheduler_vec = std::vector<std::unique_ptr<Scheduler>>;
            using bankslice_manager_vec = std::vector<std::unique_ptr<BankSliceManager>>;
            public:
            explicit MrdimmModeSwitch(const Configure& config, scheduler_vec& pch_scheduler, bankslice_manager_vec& pch_bank_manager);

            private:
            const Configure& m_config;
            scheduler_vec& m_pch_schedulers;
            bankslice_manager_vec& m_pch_bank_managers;

            const bool RD_CNT_THR_EN;
            const unsigned RD_CNT_THR;
            const bool WR_CNT_THR_EN;
            const unsigned WR_CNT_THR;
            const bool SW_WR_DELAY_EN;
            const sc_core::sc_time SW_WR_DELAY;
            const bool PREFER_WR;
            const bool SW_WR_CAM_THR_EN;
            const unsigned SW_WR_CAM_THR;

            GlobalRdWrState current_state{GlobalRdWrState::Rd};
            std::vector<bool> advance_prepare_wr;
            std::vector<bool> advance_prepare_rd;

            std::vector<bool> pch_expired_rd_page_hit_activing;
            std::vector<bool> pch_expired_wr_page_hit_activing;
            std::vector<bool> pch_flush_rd_page_hit_activing;
            std::vector<bool> pch_flush_wr_page_hit_activing;
            std::vector<bool> pch_flush_wr_page_hit_actived;

            std::vector<bool> pch_rd_critical_page_hit_activing;
            std::vector<bool> pch_wr_critical_page_hit_actived;

            std::vector<unsigned> pch_rd_ntt_avail_cmd_num;
            std::vector<unsigned> pch_wr_ntt_avail_cmd_num;
            std::vector<unsigned> pch_rd_ntt_valid_cmd_num;
            std::vector<unsigned> pch_wr_ntt_valid_cmd_num;

            std::vector<unsigned> pch_wr_cam_cmd_num;

            sc_core::sc_time switch_to_write_delay_ready_time;
            bool m_wr_delay_active = false; // 写延迟计时器是否激活

            private:
            inline bool HasAnyTrue(const std::vector<bool>& vec) {
                return std::any_of(vec.begin(), vec.end(), [](bool v) { return v; });
            }

            inline void ResetWrDelayTimer() {
                m_wr_delay_active = false;
                switch_to_write_delay_ready_time = sc_core::sc_max_time();
            }

            // inline bool HasAllTrue(const std::vector<unsigned>& vec,unsigned threshold){
            //     return std::all_of(vec.begin(), vec.end(),[threshold](unsigned num) {return num >= threshold;});
            // }

            public:
            bool inline GetPseudoChannelWriteAdvanceState(unsigned pseudo_index) {return advance_prepare_wr[pseudo_index];}
            bool inline GetPseudoChannelReadAdvanceState(unsigned pseudo_index) {return advance_prepare_rd[pseudo_index];}

            inline GlobalRdWrState GetGlobalRdWrState() {return current_state;}

            void ResetSchedulerInfo();
            void GetSchedulerInfo();
            void UpdateGlobalState();

        };
    }
}
#endif