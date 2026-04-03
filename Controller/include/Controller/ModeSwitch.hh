#ifndef __MODE_SWITCH_HH__
#define __MODE_SWITCH_HH__

#include <cstdint>
#include <iostream>

#include "Controller/common/ControllerCommon.hh"
#include "Controller/BankSliceManager.hh"
#include "Controller/Scheduler.hh"

namespace dmu{
    namespace Controller{

class ModeSwitch
{

    public:
        ModeSwitch() = delete;
        explicit ModeSwitch(Scheduler& scheduler, BankSliceManager& bank_slice_manager, const Configure& config)
        : _scheduler(scheduler)
        , _bank_slice_manager(bank_slice_manager)
        , _config(config)
        {}

        ~ModeSwitch() = default;


        inline void RdCmdSend() { rd_cmd_cnt_before_switch++;}
        inline void WrCmdSend() {
            wr_cmd_cnt_before_switch++;
        }
        inline bool GetRdWrMode() { return current_state == GlobalRdWrState::Rd || current_state == GlobalRdWrState::Rd2Wr;}
        inline GlobalRdWrState GetGlobalState() {return current_state;}
        void UpdateGlobalState();
        enum class GlobalState: uint8_t{
            RD,
            WR
        };
    private:
        GlobalRdWrState current_state{GlobalRdWrState::Rd};



        Scheduler& _scheduler; //
        BankSliceManager& _bank_slice_manager;
        const Configure& _config;


        GlobalState st{GlobalState::RD};

    private:

        unsigned rd_cmd_cnt_before_switch{0};
        unsigned wr_cmd_cnt_before_switch{0};

        bool is_rd_mode;
        bool is_wr_mode;

        bool select_rd_access{true};
        bool select_wr_access{false};

        bool real_switch2rd{false};
        bool real_switch2rd_access{false};
        bool real_switch2wr{false};
        bool real_switch2wr_access{false};


};



    } // Controller
} // dmu


// page-hit number
// unsigned rd_page_hit_num;
// unsigned wr_page_hit_num;

// RunLenthCnt
// RunLenthCnt
// RunLenthCnt


// OppHitCnt

// MaxRWBeforeSwitch
#endif