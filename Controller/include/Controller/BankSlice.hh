#ifndef __CONTROLLER_BANKSLICE_HH__
#define __CONTROLLER_BANKSLICE_HH__

#include <vector>
#include <set>
#include <list>
#include <optional>

#include "Configure/AddressDecoder.hh"
#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Controller/Scheduler.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_time.h"

namespace dmu{
    namespace Controller
    {
    using BSC_INDEX = unsigned;
    using CAM_INDEX = unsigned;
    using OrderList = std::list<CAM_INDEX>;

    using Row = unsigned long;
    class BankSlice
    {

    private:

        struct NttCmd
        {
            bool is_valid;
            CAM_INDEX cam_index;
        };
        NttCmd candidate_rd_cmd{false,0}; // rd candidate cmd
        NttCmd candidate_wr_cmd{false,0}; // wr candidate cmd

        unsigned refresh_management_counter; // store act number
        const bool refresh_management{false}; // whether open refresh management
        bool is_blocked; // if RAA counter exceed the RAAMax, then block bank sending ACT cmd

        //allocated bank address
        BankAddress _ba_addr;
        // page info
        struct PageInfo{
            bool is_open;
            Row open_page; // the opened page info(activated row address)
        }page_info;

        enum BankState{
            Idle, // IDLE state no operation
            //Activing, // bank is activate a row, maybe in tRCD wating time
            Actived, // bank is activated
            Force_Pre, // if tRASmax achieved ,then must force Precharge
            Pre_Wait,
            Wra_Rda, // auto-pre RD or WR executed State
            Precharged
        } current_state{Precharged},next_state{Precharged};

        bool is_allocated; // whether the bank slice is allocated to manage the related bank

        Command::Type next_rd_command{Command::NOP};//ACT PRE RD RDA WR WRA
        Command::Type next_wr_command{Command::NOP};//ACT PRE RD RDA WR WRA

        sc_core::sc_time next_rd_command_avail_time{sc_core::sc_max_time()};
        sc_core::sc_time next_wr_command_avail_time{sc_core::sc_max_time()};

        // const Scheduler& _scheduler;
        RdCam* const _rd_cam;
        WrCam* const _wr_cam;
        const BSC_INDEX m_bsc_index;

        sc_core::sc_time act_tRCD_end_time{sc_core::SC_ZERO_TIME}; // when send act cmd, then do mask with ntt update
        sc_core::sc_time act_tRAS_end_time{sc_core::sc_max_time()};

        const sc_core::sc_time Max_time{sc_core::sc_max_time()};

        const sc_core::sc_time tRCD_mc;
        const sc_core::sc_time tRAS_max_mc;


        bool is_refresh_waiting{false};
    public:
        BankSlice(const Scheduler& scheduler, const Configure& config, BSC_INDEX bsc_index)
        : _rd_cam(scheduler.GetRdCam())
        , _wr_cam(scheduler.GetWrCam())
        , m_bsc_index(bsc_index)
        , tRCD_mc(config.mem_spec->tRCD_mc)
        , tRAS_max_mc(config.mem_spec->tCK_mc * 251)
        {}
        ~BankSlice() = default;

        inline bool IsNeedForcePre() const { return sc_core::sc_time_stamp() > act_tRAS_end_time; }
        inline void SetRefreshWaiting() { is_refresh_waiting = true; }
        inline void ClearRefreshWaiting() { is_refresh_waiting = false; }
        inline bool IsRefreshWaiting() const { return is_refresh_waiting; }

        CommandTuple::Type GetNextCommand(GlobalRdWrState global_rdwr_mode);

        ReadyCommands GetAvailCommand(GlobalRdWrState global_rdwr_mode);

        // Updated the Bsc rd or wr sending avail time
        void UpdateBscAvailTime(const sc_core::sc_time& updated_time, bool is_rd);

        // before get cmd, based on the bsc state, decide what the bsc candidate cmd
        void Evaluate();
        // get ntt update cam index

        // when allocated, bsc will config with a specific bank
        void Allocate(const BankAddress& allocated_ba_addr);

        // when bsc release, do bank address reset, and bsc state reset
        void Release();
        // when send act cmd, the bsc will keep activing until tRCD ac timing

        //update the ntt cmd to the bsc
        void SelectRdNtt(CAM_INDEX rd_cam_index);
        void SelectWrNtt(CAM_INDEX wr_cam_index);

        // void Block(); // if ACT exceed the RAAMMT, block activate

        // when sending cmd, the sending cmd will update the bsc state
        void Update(const CommandTuple::Type& sending_cmd);


        // Note: this is the interface to get bsc static info
        inline bool IsActiving() const {return sc_core::sc_time_stamp() < act_tRCD_end_time; } // to show whether the BSC bank is in activing time
        inline sc_core::sc_time GetActEndTime() const {return act_tRCD_end_time;}
        inline bool IsPageOpen() const {return page_info.is_open;}
        inline Row GetOpenPage() const {return page_info.open_page;}
        CAM_INDEX GetNttCamIndex(bool is_rd_mode);
        const BankAddress& GetBaAddr() const;

        inline sc_core::sc_time& GetRdCmdAvailTime() {  return next_rd_command_avail_time;      }
        inline sc_core::sc_time& GetWrCmdAvailTime() {  return next_wr_command_avail_time;      }

        inline const Command::Type& GetRdCmd() {return next_rd_command;}
        inline const Command::Type& GetWrCmd() {return next_wr_command;}

        inline bool IsWrCmdAvail() const
        {
            if(candidate_wr_cmd.is_valid && next_wr_command != Command::NOP && next_wr_command_avail_time <= sc_core::sc_time_stamp())
            {
                return true;
            }
            return false;
        }
        inline bool IsRdCmdAvail() const
        {
            if(candidate_rd_cmd.is_valid && next_rd_command != Command::NOP && next_rd_command_avail_time <= sc_core::sc_time_stamp())
            {
                return true;
            }
            return false;
        }

        inline bool IsWrRowCmdAvail() const {return candidate_wr_cmd.is_valid && (next_wr_command == Command::PRE || next_wr_command == Command::ACT) && next_wr_command_avail_time <= sc_core::sc_time_stamp();}
        inline bool IsRdRowCmdAvail() const {return candidate_rd_cmd.is_valid && (next_rd_command == Command::PRE || next_rd_command == Command::ACT) && next_rd_command_avail_time <= sc_core::sc_time_stamp();}

        inline bool IsWrColCmdAvail() const {return candidate_wr_cmd.is_valid && (next_wr_command == Command::WR || next_wr_command == Command::WRA) && next_wr_command_avail_time <= sc_core::sc_time_stamp();}
        inline bool IsRdColCmdAvail() const {return candidate_rd_cmd.is_valid && (next_rd_command == Command::RD || next_rd_command == Command::RDA) && next_rd_command_avail_time <= sc_core::sc_time_stamp();}

        inline bool IsRdPageClosePending() const {  return IsRdCmdAvail() && next_rd_command == Command::PRE;} // Bank Rd direction being PRE
        inline bool IsWrPageClosePending() const {  return IsWrCmdAvail() && next_wr_command == Command::PRE;} // Bank Wr direction being PRE

        inline bool IsRdPageOpenPending() const {   return IsRdCmdAvail() && next_rd_command == Command::ACT;}//Bank Rd direction being ACT
        inline bool IsWrPageOpenPending() const {   return IsWrCmdAvail() && next_wr_command == Command::ACT;}//Bank Wr direction being ACT

        inline bool IsWrNttValid() const {return candidate_wr_cmd.is_valid;}
        inline bool IsRdNttValid() const {return candidate_rd_cmd.is_valid;}
        inline bool IsWrNttPageHit() const
        {return candidate_wr_cmd.is_valid && _wr_cam->GetCamEntry(candidate_wr_cmd.cam_index)->IsPageHit();}
        inline bool IsRdNttPageHit() const
        {return candidate_rd_cmd.is_valid && _rd_cam->GetCamEntry(candidate_rd_cmd.cam_index)->IsPageHit();}

        inline bool IsRdNttExpired() const
        {return candidate_rd_cmd.is_valid && _rd_cam->GetCamEntry(candidate_rd_cmd.cam_index)->IsExpired();}
        inline bool IsWrNttExpired() const
        {return candidate_wr_cmd.is_valid && _wr_cam->GetCamEntry(candidate_wr_cmd.cam_index)->IsExpired();}

        // Implement with Codex
        //TODO: how to implement force precharge due to tRASmax

        inline sc_core::sc_time GetNextCommandTriggerTime()
        {
            return std::min(next_wr_command_avail_time,next_rd_command_avail_time);
        }

        void print()
        {
            std::cout << "Bank Slice: " << m_bsc_index
                      << " ba_addr: " << _ba_addr
                      << " is_refresh_waiting: " << is_refresh_waiting
                      << " page open: " << page_info.is_open
                      << " page open row: " << page_info.open_page
                      << ";\t"
                      << "wr ntt valid: " << candidate_wr_cmd.is_valid << ", wr ntt cam index: " << candidate_wr_cmd.cam_index
                      << "wr cmd: " << Command(next_wr_command).to_string() << ", wr cmd avail time: " << next_wr_command_avail_time << "; \t"
                      << "rd ntt valid: " << candidate_rd_cmd.is_valid << ", rd ntt cam index: " << candidate_rd_cmd.cam_index
                      << "rd cmd: " << Command(next_rd_command).to_string() << ", rd cmd avail time: " << next_rd_command_avail_time << "; \t"
            <<std::endl;
        }

    };
    } // namespace Controller
} //namespace dmu

#endif