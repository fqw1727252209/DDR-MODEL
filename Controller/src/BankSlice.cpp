#include "Controller/BankSliceManager.hh"
#include "Controller/BankSlice.hh"
#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Common/CommonDefine.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <cassert>

namespace dmu{
    namespace Controller{

BankSlice::BankSlice(const Scheduler& scheduler, const Configure& config, BSC_INDEX bsc_index,BankSliceManager* manager)
: _rd_cam(scheduler.GetRdCam())
, _wr_cam(scheduler.GetWrCam())
, m_bsc_manager(manager)
, m_bsc_index(bsc_index)
, tRCD_mc(config.mem_spec->tRCD_mc)
, tRAS_max_mc(config.mem_spec->tCK_mc * 651)
, RD_OPEN_BANK_NUM(config.controller_config->RD_OPEN_BANK_NUM)
, RD_OPEN_BANK_NUM_EN(RD_OPEN_BANK_NUM != 0)
, WR_OPEN_BANK_NUM(config.controller_config->WR_OPEN_BANK_NUM)
, WR_OPEN_BANK_NUM_EN(WR_OPEN_BANK_NUM != 0)
, RD_KEEP_PAGE_NUM(config.controller_config->RD_KEEP_PAGE_NUM)
, RD_KEEP_PAGE_NUM_EN(RD_KEEP_PAGE_NUM != 0)
, WR_KEEP_PAGE_NUM(config.controller_config->WR_KEEP_PAGE_NUM)
, WR_KEEP_PAGE_NUM_EN(WR_KEEP_PAGE_NUM != 0)
, PAGE_CLOSE_TIME(config.controller_config->PAGE_CLOSE_TIME * config.mem_spec->tCK_mc)
, PAGE_CLOSE_TIME_EN(config.controller_config->PAGE_CLOSE_TIME != 0)
{
}
BankSlice::~BankSlice()
{
}

void
BankSlice::Allocate(const BankAddress& allocated_ba_addr)
{
    is_allocated = true;
    is_blocked = false;
    current_state = BankState::Precharged;
    page_info.is_open = false;
    refresh_management_counter = 0;
    _ba_addr = allocated_ba_addr;
    candidate_rd_cmd.is_valid = false;
    candidate_wr_cmd.is_valid = false;
}

void
BankSlice::Release()
{
    next_rd_command = Command::NOP;
    next_wr_command = Command::NOP;
    next_rd_command_avail_time = sc_core::SC_ZERO_TIME;
    next_wr_command_avail_time = sc_core::SC_ZERO_TIME;
    page_info.is_open = false;
    is_allocated = false;
    _ba_addr.ResetBankAddress();
}

void
BankSlice::Update(const CommandTuple::Type& sending_cmd)
{
    Command cmd = std::get<CommandTuple::Command>(sending_cmd);
    switch (cmd)
    {
        case Command::ACT:
        /* code */
        {
            page_info.is_open = true;
            // page_info.open_page = ;
            refresh_management_counter++;
            // CAM_INDEX cam_index = std::get<CommandTuple::CAM_INDEX>(sending_cmd);
            CAM_INDEX cam_index = std::get<CommandTuple::CAM_INDEX>(sending_cmd);
            bool is_rd = std::get<CommandTuple::CommandSrc>(sending_cmd) == CmdSrc::RdTransaction;
            Row open_page;
            if(is_rd)
            {
                open_page = _rd_cam->GetCamEntry(cam_index)->sdram_addr.row;
            }
            else
            {
                open_page = _wr_cam->GetCamEntry(cam_index)->sdram_addr.row;
            }
            page_info.open_page = open_page;
            _rd_cam->SetBaPageHit(_ba_addr.real_ba, open_page);
            _wr_cam->SetBaPageHit(_ba_addr.real_ba, open_page);

            assert(current_state != BankState::Actived && "Bank is already actived");
            current_state = BankState::Actived;
            act_tRCD_end_time = sc_core::sc_time_stamp() + tRCD_mc;
            act_tRAS_end_time = sc_core::sc_time_stamp() + tRAS_max_mc;
            break;
        }
        case Command::PRE:
        case Command::PREsb:
        case Command::PREab:
        {
            page_info.is_open = false;
            act_tRAS_end_time = Max_time;
            assert(current_state != BankState::Precharged && "Bank is already precharged");
            current_state = BankState::Precharged;
            _rd_cam->SetBaPageClose(_ba_addr.real_ba);
            _wr_cam->SetBaPageClose(_ba_addr.real_ba);
            break;
        }

        case Command::RD:
            //
            //candidate_rd_cmd.is_valid = false;
            break;
        case Command::WR:
            //
            //candidate_wr_cmd.is_valid = false;
            break;

        case Command::RDA:
            page_info.is_open = false;
            candidate_rd_cmd.is_valid = false;
            current_state = BankState::Precharged;
            break;
        case Command::WRA:
            page_info.is_open = false;
            candidate_wr_cmd.is_valid = false;
            current_state = BankState::Precharged;
            break;

        case Command::REFab:
        case Command::RFMab:
            ClearRefreshWaiting();
            break;
        case Command::REFsb:
        case Command::RFMsb:
            ClearRefreshWaiting();
            break;
        default:
            break;
    }
}

void
BankSlice::Evaluate()
{
    next_rd_command = Command::NOP;
    next_wr_command = Command::NOP;
    next_bank_command = Command::NOP;

    if(candidate_rd_cmd.is_valid || candidate_wr_cmd.is_valid)
    {
        if(candidate_rd_cmd.is_valid)
        {
            DPRINT_ASSERT(_rd_cam->IsCamExist(candidate_rd_cmd.cam_index),"Bank Slice","Evaluate Func: Bsc Index: %d, the candidate rd cam index: %d not exsit, but valid",m_bsc_index,candidate_rd_cmd.cam_index);
            auto rd_cam_entry = _rd_cam->GetCamEntry(candidate_rd_cmd.cam_index);
            if(current_state == BankState::Actived )
            {
                if(IsRefreshWaiting() || IsNeedForcePre())
                {
                    next_bank_command = Command::PRE;
                    next_rd_command = Command::NOP;
                }
                else
                {
                    if(rd_cam_entry->sdram_addr.row == page_info.open_page)
                    {
                        if(_rd_cam->GetBaSamePageCmdNum(_ba_addr.real_ba,page_info.open_page) < 2)
                        {
                            next_rd_command = Command::RDA;
                        }
                        else
                        {
                            next_rd_command = Command::RD;
                        }
                    }
                    else
                    {
                        next_rd_command = Command::PRE;
                    }
                }
                // if(!is_refresh_waiting && !IsNeedForcePre() && ( rd_cam_entry->sdram_addr.row == page_info.open_page))
                // {
                //     if(_rd_cam->GetBaOrderList(_ba_addr.real_ba).size() < 2)
                //     {
                //         next_rd_command = Command::RDA;
                //     }
                //     else
                //     {
                //         next_rd_command = Command::RD;
                //     }
                // }
                // else
                // {
                //     next_rd_command = Command::PRE;
                // }
            }
            else if(current_state == BankState::Precharged)
            {
                if(IsRefreshWaiting())
                {
                    next_rd_command = Command::NOP;
                }
                else
                {
                    next_rd_command = Command::ACT;
                }
            }
        }

        if(candidate_wr_cmd.is_valid)
        {
            DPRINT_ASSERT(_wr_cam->IsCamExist(candidate_wr_cmd.cam_index),"Bank Slice","Evaluate Func: Bsc Index: %d, the candidate wr cam index: %d not exsit, but valid",m_bsc_index,candidate_wr_cmd.cam_index);
            auto wr_cam_entry = _wr_cam->GetCamEntry(candidate_wr_cmd.cam_index);

            if(current_state == BankState::Actived)
            {
                if(IsRefreshWaiting() || IsNeedForcePre())
                {
                    next_bank_command = Command::PRE;
                    next_wr_command = Command::NOP;
                }
                else
                {
                    if(wr_cam_entry->sdram_addr.row == page_info.open_page)
                    {
                        if(_wr_cam->GetBaSamePageCmdNum(_ba_addr.real_ba,page_info.open_page) < 2)
                        {
                            next_wr_command = Command::WRA;
                        }
                        else
                        {
                            next_wr_command = Command::WR;
                        }
                    }
                    else
                    {
                        next_wr_command = Command::PRE;
                    }
                }
                // if(!is_refresh_waiting && !IsNeedForcePre() && (wr_cam_entry->sdram_addr.row == page_info.open_page))
                // {
                //     if(_wr_cam->GetBaOrderList(_ba_addr.real_ba).size() < 2)
                //     {
                //         next_wr_command = Command::WRA;
                //     }
                //     else
                //     {
                //         next_wr_command = Command::WR;
                //     }
                // }
                // else
                // {
                //     next_wr_command = Command::PRE;
                // }
            }
            else if(current_state == BankState::Precharged)
            {
                if(IsRefreshWaiting())
                {
                    next_wr_command = Command::NOP;
                }
                else
                {
                    next_wr_command = Command::ACT;
                }
            }
        }
    }
    else
    {
        if(current_state == BankState::Actived )
        {
            if(IsRefreshWaiting() || IsNeedForcePre())
            {
                next_bank_command = Command::PRE;
            }
        }
        return;
    }
}

const BankAddress&
BankSlice::GetBaAddr() const
{
    return this->_ba_addr;
}

ReadyCommands
BankSlice::GetAvailCommand(GlobalRdWrState global_rdwr_mode)
{
    ReadyCommands bank_ready_commands;
    if(global_rdwr_mode == GlobalRdWrState::Rd)
    {
        if(this->IsRdCmdAvail())
        {
            bank_ready_commands.emplace_back(next_rd_command,candidate_rd_cmd.cam_index,this->_ba_addr,next_rd_command_avail_time,
                CmdSrc::RdTransaction,
                _rd_cam->GetCamEntry(candidate_rd_cmd.cam_index)->GetRequest());
        }
    }
    else if(global_rdwr_mode == GlobalRdWrState::Rd2Wr)
    {
        if(this->IsRdCmdAvail() && (next_rd_command == Command::RD || next_rd_command == Command::RDA))
        {
            bank_ready_commands.emplace_back(next_rd_command,candidate_rd_cmd.cam_index,this->_ba_addr,next_rd_command_avail_time,
                CmdSrc::RdTransaction,
                _rd_cam->GetCamEntry(candidate_rd_cmd.cam_index)->GetRequest());
        }

        if(this->IsWrCmdAvail() && (next_wr_command == Command::ACT || next_wr_command == Command::PRE))
        {
            bank_ready_commands.emplace_back(next_wr_command,candidate_wr_cmd.cam_index,this->_ba_addr,next_wr_command_avail_time,
                CmdSrc::WrTransaction,
                _wr_cam->GetCamEntry(candidate_wr_cmd.cam_index)->GetRequest());
        }
    }
    else if(global_rdwr_mode == GlobalRdWrState::Wr)
    {
        if(this->IsWrCmdAvail())
        {
            bank_ready_commands.emplace_back(next_wr_command,candidate_wr_cmd.cam_index,this->_ba_addr,next_wr_command_avail_time,
                CmdSrc::WrTransaction,
                _wr_cam->GetCamEntry(candidate_wr_cmd.cam_index)->GetRequest());
        }
    }
    else if(global_rdwr_mode == GlobalRdWrState::Wr2Rd)
    {
        if(this->IsRdCmdAvail() && (next_rd_command == Command::ACT || next_rd_command == Command::PRE))
        {
            bank_ready_commands.emplace_back(next_rd_command,candidate_rd_cmd.cam_index,this->_ba_addr,next_rd_command_avail_time,
                CmdSrc::RdTransaction,
                _rd_cam->GetCamEntry(candidate_rd_cmd.cam_index)->GetRequest());
        }
        if(this->IsWrCmdAvail() && (next_wr_command == Command::WR || next_wr_command == Command::WRA))
        {
            bank_ready_commands.emplace_back(next_wr_command,candidate_wr_cmd.cam_index,this->_ba_addr,next_wr_command_avail_time,
                CmdSrc::WrTransaction,
                _wr_cam->GetCamEntry(candidate_wr_cmd.cam_index)->GetRequest());
        }
    }
    else
    {
        std::cerr<< "Bank Slice get invalid global mode"<<std::endl;
        std::abort();
    }
    return bank_ready_commands;
}

ReadyCommands
BankSlice::GetRdAvailCommand()
{
    ReadyCommands bank_ready_commands;
    if (IsRdCmdAvail())
    {
        bank_ready_commands.emplace_back(next_rd_command,candidate_rd_cmd.cam_index,this->_ba_addr,next_rd_command_avail_time,
            CmdSrc::RdTransaction,
            _rd_cam->GetCamEntry(candidate_rd_cmd.cam_index)->GetRequest());
    }
    return bank_ready_commands;
}

ReadyCommands
BankSlice::GetWrAvailCommand()
{
    ReadyCommands bank_ready_commands;
    if (IsWrCmdAvail())
    {
        bank_ready_commands.emplace_back(next_wr_command,candidate_wr_cmd.cam_index,this->_ba_addr,next_wr_command_avail_time,
            CmdSrc::WrTransaction,
            _wr_cam->GetCamEntry(candidate_wr_cmd.cam_index)->GetRequest());
    }
    return bank_ready_commands;
}

ReadyCommands
BankSlice::GetForcePreCmd()
{
    ReadyCommands force_pre_cmd;
    if(IsForcePreAvail())
    {
        force_pre_cmd.emplace_back(Command::PRE,0,this->_ba_addr,next_bank_command_avail_time,
            CmdSrc::BankTransaction,
            &bank_payload);
    }
    return force_pre_cmd;
}

ReadyCommands
BankSlice::GetRefreshPreCmd()
{
    ReadyCommands refresh_pre_cmd;
    if(IsRefreshPreAvail())
    {
        refresh_pre_cmd.emplace_back(Command::PRE,0,this->_ba_addr,next_bank_command_avail_time,
            CmdSrc::BankTransaction,
            &bank_payload);
    }
    return refresh_pre_cmd;
}

CAM_INDEX
BankSlice::GetNttCamIndex(bool is_rd_mode)
{
    return is_rd_mode ? candidate_rd_cmd.cam_index : candidate_wr_cmd.cam_index;
}

void
BankSlice::SelectRdNtt(CAM_INDEX rd_cam_index)
{
    DPRINT_INFO(TOP_DEBUG,"BankSlice","update the rd ntt to the bsc: %d, with selected rd cam index: %d",m_bsc_index,rd_cam_index);
    DPRINT_ASSERT(_ba_addr.real_ba == (_rd_cam->GetCamEntry(rd_cam_index)->sdram_addr.real_ba), "Bank Slice", "SelectWrNtt Func: the selected rd cam index: %d, the real ba of the cam entry: %d, the real ba of the bsc: %d",rd_cam_index,_rd_cam->GetCamEntry(rd_cam_index)->sdram_addr.real_ba,_ba_addr.real_ba);
    candidate_rd_cmd.cam_index = rd_cam_index;
    candidate_rd_cmd.is_valid = true;
}

void
BankSlice::SelectWrNtt(CAM_INDEX wr_cam_index)
{
    DPRINT_INFO(TOP_DEBUG,"BankSlice","update the wr ntt to the bsc: %d, with selected wr cam index: %d",m_bsc_index,wr_cam_index);
    candidate_wr_cmd.cam_index = wr_cam_index;
    candidate_wr_cmd.is_valid = true;
}

    }
}