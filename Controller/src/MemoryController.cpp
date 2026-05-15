#include "Controller/MemoryController.hh"
#include "Common/Common.hh"
#include "Controller/CamEntry.hh"
#include "Controller/CamIF.hh"
#include "Configure/LoadConfigFromJson.hh"
#include "Controller/RdCam.hh"
#include "Controller/RefreshMachineManager.hh"
#include "Controller/Scheduler.hh"
#include "Controller/WrCam.hh"
#include "Controller/common/CmdExtension.hh"
#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Common/CommonDefine.hh"
#include "Controller/common/DfiExtension.hh"
#include "Common/UifExtension.hh"
#include "Common/StatisticExtension.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_time.h"
#include "sysc/utils/sc_report.h"
#include "tlm_core/tlm_2/tlm_2_interfaces/tlm_fw_bw_ifs.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_phase.h"
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iomanip>
#include <ios>
#include <iterator>
#include <sys/types.h>

namespace dmu{
    namespace Controller{

void
MemoryController::end_of_simulation()
{
}

tlm::tlm_sync_enum
MemoryController::nb_transport_fw(tlm::tlm_generic_payload& trans,
                                  tlm::tlm_phase& phase,
                                  sc_core::sc_time& delay)
{
    // InputProcess
    if(phase == UIF_REQ)
    {
        if(ctrl_event.triggered())
        {
            DPRINT_INFO(MEMORY_CONTROLLER,name(),"ControllerMethod() Process run before the TLM interface"); // Controller Method --> tlm sending
        }
        else
        {
            DPRINT_INFO(MEMORY_CONTROLLER,name(),"ControllerMethod() Process run after the TLM interface");
        }
        ctrl_event.notify(dfi_cycle_time);
        if(addr_collision_busy)
        {
            DPRINT_INFO(MEMORY_CONTROLLER,name(),"cant accept payload from UIF port, because there is address collision");
            return tlm::TLM_UPDATED;
        }

        if(_scheduler->IsTpwFull() && trans.get_command() == tlm::TLM_WRITE_COMMAND)
        {
            DPRINT_INFO(MEMORY_CONTROLLER,name(),"cant accept tpw payload from UIF port ");
            return tlm::TLM_UPDATED;
        }
        else if(_scheduler->IsHprFull() && trans.get_command() == tlm::TLM_READ_COMMAND
        && trans.get_extension<UifExtension>()->GetQosLevel() == PriorityClass::HPR)
        {
            DPRINT_INFO(MEMORY_CONTROLLER,name(),"cant accept hpr payload from UIF port ");
            return tlm::TLM_UPDATED;
        }
        else if(_scheduler->IsLprFull() && trans.get_command() == tlm::TLM_READ_COMMAND
        && (trans.get_extension<UifExtension>()->GetQosLevel() == PriorityClass::LPR
        || trans.get_extension<UifExtension>()->GetQosLevel() == PriorityClass::GPR) )
        {
            DPRINT_INFO(MEMORY_CONTROLLER,name(),"cant accept lpr payload from UIF port ");
            return tlm::TLM_UPDATED;
        }
        else
        {
            DPRINT_INFO(MEMORY_CONTROLLER,name(),"receive payload from UIF port  ");
            _input_process->AcceptRequest(trans);
            return tlm::TLM_ACCEPTED;
        }
    }
    else
    {
        payload_event_queue.notify(trans,phase,delay);
    }
    return tlm::TLM_ACCEPTED;
}

tlm::tlm_sync_enum
MemoryController::nb_transport_bw(tlm::tlm_generic_payload& trans, tlm::tlm_phase &phase, sc_core::sc_time &delay)
{
    payload_event_queue.notify(trans, phase, delay);
    DPRINT_INFO(false, "MemoryController", "nb_transport_bw() function has been called, pyaload event queue notify");
    return tlm::TLM_ACCEPTED;
}

void
MemoryController::pipline_method(tlm::tlm_generic_payload& trans, const tlm::tlm_phase &phase)
{
    if(phase == DFI_RDAT_BEGIN)
    {
        // tlm::tlm_phase rdat_phase = UIF_RDAT_BEGIN;
        // sc_core::sc_time rdat_delay = dfi_cycle_time;
        // tSocket->nb_transport_bw(trans, rdat_phase, rdat_delay);
    }
    else if(phase == DFI_RDAT_END)
    {
        // RemoveTransFromResonseQueue(trans.get_extension<StatisticExtension>()->GetTransactionId(),&trans);
        tlm::tlm_phase rdat_phase = UIF_RDAT_BEGIN;
        sc_core::sc_time rdat_delay = dfi_cycle_time;
        tSocket->nb_transport_bw(trans, rdat_phase, rdat_delay);
        // Implement with Codex
        if(trans.get_data_length() > 32) //TODO: this 32 should be computed by the uif data_width bytes
        {
            tlm::tlm_phase uif_rdat_end_phase = UIF_RDAT_END;
            sc_core::sc_time rdat_delay = dfi_cycle_time + dfi_cycle_time;
            tSocket->nb_transport_bw(trans, uif_rdat_end_phase, rdat_delay);
        }
        else
        {
            tlm::tlm_phase uif_rdat_end_phase = UIF_RDAT_END;
            sc_core::sc_time rdat_delay = dfi_cycle_time;
            tSocket->nb_transport_bw(trans, uif_rdat_end_phase, rdat_delay);
        }
    }
    else if(phase == DFI_WDAT_BEGIN)
    {
        payload_event_queue.notify(trans,DFI_WDAT_END,_config.mem_spec->tBurst * ddr_cycle_time);
        tlm::tlm_phase wdat_phase = phase;
        sc_core::sc_time wdat_delay = sc_core::SC_ZERO_TIME;
        iSocket->nb_transport_fw(trans, wdat_phase, wdat_delay);
    }
    else if(phase == DFI_WDAT_END)
    {
        tlm::tlm_phase wdat_phase = phase;
        sc_core::sc_time wdat_delay = sc_core::SC_ZERO_TIME;
        iSocket->nb_transport_fw(trans, wdat_phase, wdat_delay);
        // Implement with Codex
        //TODO: Check the Wdat Buffer is full, if not full, then check all the wr cam cmd is sending data request
    }
    else if(phase == UIF_WDAT_REQ)
    {
        DPRINT_INFO(true, "Uif Channel", "Wdat Request for Write Data");
        tlm::tlm_phase wdat_phase = phase;
        sc_core::sc_time wdat_delay = sc_core::SC_ZERO_TIME;
        tSocket->nb_transport_bw(trans, wdat_phase, wdat_delay);
    }
    else if(phase == UIF_WDAT_BEGIN)
    {

    }
    else if(phase == UIF_WDAT_END)
    {
        CAM_INDEX wr_cam_index = trans.get_extension<UifExtension>()->uif_info.wr_cam_index;
        auto wr_cam = _scheduler->GetWrCam();
        assert(wr_cam->IsCamExist(wr_cam_index) && "Wr Cam Index is not exist");
        wr_cam->SetWdataReady(wr_cam_index);
        auto wr_cam_entry = wr_cam->GetCamEntry(wr_cam_index);
        RealBaIndex request_ba_index = wr_cam_entry->GetCamEntryRealBa();
        // DPRINT_INFO(true, "UIF_WDAT_END", "Get wr cam entry Info");
        if(_scheduler->IsBscMatch(request_ba_index))
        {
            CAM_INDEX request_cam_index = wr_cam_index;
            auto bank_slice = _bankslice_manager->GetBankSliceMap()->at(_bankslice_manager->GetBa2BscTable()->at(request_ba_index)).get();
            bool is_page_hit = bank_slice->IsPageOpen() && (bank_slice->GetOpenPage() == wr_cam_entry->sdram_addr.row);
            wr_cam_entry->SetBaMatch(_bankslice_manager->GetBa2BscTable()->at(request_ba_index),is_page_hit);
            if(is_page_hit || (!is_page_hit && (!bank_slice->IsActiving() || bank_slice->IsWrNttValid())))
            {
                _scheduler->UpdateWrNttPip(_bankslice_manager->GetBa2BscTable()->at(request_ba_index),request_ba_index,UpdateType::NewCmdStore);
            }
        }
        next_trigger_delay = std::min(next_trigger_delay, _scheduler->GetNextUpdateTime() - sc_core::sc_time_stamp());
        ctrl_event.notify(next_trigger_delay);
        DPRINT_INFO(false, "UIF_WDAT_END", "function end");
    }
    else if(phase == UIF_REQ)
    {

    }
}

MemoryController::~MemoryController()
{
    ControllerFinishCheck();

    delete dfi_payload;
}

void
MemoryController::ControllerMethod()
{
    DPRINT_INFO(true, "Controller", "ControllerMethod function start");
    //initial event next trigger time
    next_trigger_delay = sc_core::sc_max_time();
    // DPRINT_INFO(TOP_DEBUG,name(),"ControllerMethod EXE");
    // do the bsc release
    if(_bankslice_manager->IsNeedBankSliceRelease())
    {
        DPRINT_INFO(TOP_DEBUG,name(),"Bank Slice Release");
        _bankslice_manager->BankSliceRelease();
        next_trigger_delay = dfi_cycle_time;
    }

    // cmd send stage
    CmdSend();
    // cmd updated to ntt stage
    ReqUpdate();
    // cmd store in cam stage
    CqStore();
    // pip process stage
    PipProcess();
    // trigger the event
    // however since the ctrl_event triggered order is not confirmed with nb_transport_fw, this need to judge the right order
    if(next_trigger_delay != sc_core::sc_max_time())
    {
        assert(next_trigger_delay >= dfi_cycle_time);
        ctrl_event.notify(next_trigger_delay);
    }
    else
    {
        // ControllerFinishCheck();
        DPRINT_INFO(TOP_DEBUG,name(),"No Event will scheduled, next_trigger_delay: %s, max time: %s ",next_trigger_delay.to_string().c_str(), sc_core::sc_max_time().to_string().c_str() );
    }
}

void
MemoryController::ControllerFinishCheck()
{
    //    auto rd_cam = _scheduler->GetRdCam();
    // auto wr_cam = _scheduler->GetWrCam();

    // Cam check
    std::cout << "---------------------------------Rd Cam---------------------------------"<<std::endl;
    std::vector<RdCamEntry*> rd_cam_entry_list;
    if(!_scheduler->GetRdCam()->IsCamEmpty())
    {
        for(auto& rd_cam_index: _scheduler->GetRdCam()->GetUsedCamIndex())
        {
            auto rd_cam_entry = _scheduler->GetRdCam()->GetCamEntry(rd_cam_index);
            rd_cam_entry->print();
            rd_cam_entry_list.push_back(rd_cam_entry);
        }
    }
    std::cout << "---------------------------------Wr Cam---------------------------------"<<std::endl;
    std::vector<WrCamEntry*> wr_cam_entry_list;
    if(!_scheduler->GetWrCam()->IsCamEmpty())
    {
        for(auto& wr_cam_index: _scheduler->GetWrCam()->GetUsedCamIndex())
        {
            auto wr_cam_entry = _scheduler->GetWrCam()->GetCamEntry(wr_cam_index);
            wr_cam_entry->print();
            wr_cam_entry_list.push_back(wr_cam_entry);
        }
    }
    std::cout << "---------------------------------Bank Slice---------------------------------"<<std::endl;
    std::vector<BankSlice*> allocated_bsc_list;
    if(!_bankslice_manager->IsAllocatedBscEmpty())
    {
        for(auto& allocated_bsc : _bankslice_manager->GetAllocatedBscSet())
        {
            auto allocated_bank_slice = _bankslice_manager->GetBankSliceMap()->at(allocated_bsc).get();
            allocated_bsc_list.push_back(allocated_bank_slice);
            allocated_bank_slice->print();
        }
    }
    std::cout << "---------------------------------Ntt---------------------------------"<<std::endl;
    std::cout << "Next Ntt trigger time: " << _scheduler->GetNextUpdateTime().to_string().c_str() << std::endl;
    assert(rd_cam_entry_list.empty() && "rd cam is not empty");
    assert(wr_cam_entry_list.empty() && "wr cam is not empty");
    assert(allocated_bsc_list.empty() && "allocated bsc is not empty");
}

void
MemoryController::AcTimingUpdate()
{
    for(auto rank_index: _refresh_machine_manager->GetRefreshRankIds())
    {
        auto refresh_machine = _refresh_machine_manager->GetRefreshMachine(rank_index);
        refresh_machine->Evaluate();
        const BankAddress rank_addr = refresh_machine->GetRankAddress();
        const Command refresh_cmd = refresh_machine->GetRefreshCommand();
        sc_core::sc_time& refresh_command_avail_time = refresh_machine->GetRefreshCommandAvailTime();
        if(refresh_cmd == Command::NOP)
        {
            refresh_command_avail_time = sc_core::sc_max_time();
        }
        else
        {
            refresh_command_avail_time = _sdram_constraint->TimeToSatisfyConstraints(refresh_cmd,rank_addr);
        }
        DPRINT_INFO(MEMORY_CONTROLLER && _config.controller_config->REFRESH_ENABLE, "Memory Controller", "Refresh Rank: %d, Refresh Command: %s, the Refresh Avail Time is %s, and all bank is closed:%d",rank_index,refresh_cmd.to_string().c_str(),refresh_command_avail_time.to_string().c_str(),refresh_machine->IsAllBanksClosed());
    }

    // Get all the command and then do the Ac Timing update, and decide the avail cmd sending time
    for(auto& allocated_bsc: _bankslice_manager->GetAllocatedBscSet())
    {
        auto bank_slice = _bankslice_manager->GetBsc(allocated_bsc);
        bank_slice->Evaluate();
        const BankAddress ba_addr = bank_slice->GetBaAddr();
        const Command::Type& rd_cmd = bank_slice->GetRdCmd();
        sc_core::sc_time& rd_cmd_avail_time = bank_slice->GetRdCmdAvailTime();
        if(rd_cmd == Command::NOP)
        {
            rd_cmd_avail_time = sc_core::sc_max_time();
        }
        else
        {
            rd_cmd_avail_time = _sdram_constraint->TimeToSatisfyConstraints(rd_cmd,ba_addr);
        }
        const Command::Type& wr_cmd = bank_slice->GetWrCmd();
        sc_core::sc_time& wr_cmd_avail_time = bank_slice->GetWrCmdAvailTime();
        if(wr_cmd == Command::NOP)
        {
            wr_cmd_avail_time = sc_core::sc_max_time();
        }
        else
        {
            wr_cmd_avail_time = _sdram_constraint->TimeToSatisfyConstraints(wr_cmd,ba_addr);
        }
    }
}

void
MemoryController::CmdSend()
{

    // 1. Get all allocated Bank command(including ACT/PRE WR/WRA RD/RDA)
    //    Update the available sending time to these bank command
    // 2. Do the mode switch policy
    //    (1) Update all the cam critical state
    //    (2) do mode switch pending check
    //    (3) based on the cs state(e.g Opphit num) do switch
    //    (4) decide the global act/pre is rd/wr mode, decide the global rd/rda/wr/wra cmd mode
    // 3. Update the global direction
    // 4. Get all bank ready command(ready: sending time is less than current timing, which do the tag)
    // 5. collect refresh command
    // 6. do the cmd selection
    // 7. sending cmd (maybe need to do the tlm nb_transport_fw to send to downstream device)
    //    (1)sending cmd related cam entry delete
    //    (2)sending cmd related bsc do the ntt updated
    //    (3)update the timing constraint and delete the cmd in the cam entry
    //    (4)because the ntt updated will call the next trigger time with next cycle, so dont do bsc cmd ac timing updated to get the next trigger time

    // every time the cam num changed, the fill level and cam full state may be affected( fill level pos edge and full state negedeg)
    // so when do cam entry store or cam entry delete

    // refresh
    AcTimingUpdate();

    ready_commands.clear();
    if(!_refresh_machine_manager->IsRefreshReadyCommandsEmpty())
    {
        ReadyCommands& refresh_ready_commands = _refresh_machine_manager->GetRefreshReadyCommands();
        for(auto& refresh_cmd: refresh_ready_commands)
        {
            Command refresh_cmd_type = std::get<CommandTuple::Command>(refresh_cmd);
            unsigned rank_id = std::get<CommandTuple::BaAddress>(refresh_cmd).real_cid;
            DPRINT_INFO(TOP_DEBUG, "Memory Controller", "Get Refresh Command: %s for Refefresh Rank: %d", refresh_cmd_type.to_string().c_str(),rank_id);
        }
        ready_commands.insert(ready_commands.end(),std::make_move_iterator(refresh_ready_commands.begin()),std::make_move_iterator(refresh_ready_commands.end()));
    }
    _mode_switch->UpdateGlobalState();
    GlobalRdWrState global_rdwr_mode = _mode_switch->GetGlobalState();
    DPRINT_INFO(MODE_SWITCH, "Memory Controller", "Get Global RdWr State: %s", PrintState(global_rdwr_mode).c_str());
    if(!_bankslice_manager->IsReadyCommandsEmpty(global_rdwr_mode))
    {
        ReadyCommands& bank_ready_commands = _bankslice_manager->GetReadyCommands();
        ready_commands.insert(ready_commands.end(),std::make_move_iterator(bank_ready_commands.begin()),std::make_move_iterator(bank_ready_commands.end()));
    }
    if(!ready_commands.empty())
    {
        CommandTuple::Type selected_cmd = _cmd_select->SelectCommand(ready_commands,global_rdwr_mode);
        BankAddress selected_cmd_addr = std::get<CommandTuple::BaAddress>(selected_cmd);
        sc_core::sc_time selected_cmd_sending_time = std::get<CommandTuple::AvailTime>(selected_cmd);
        assert(selected_cmd_sending_time <= sc_core::sc_time_stamp());
        Command selected_cmd_type = std::get<CommandTuple::Command>(selected_cmd);
        if(selected_cmd_type.IsBankCommand())
        {
            CAM_INDEX selected_cmd_cam_index = std::get<CommandTuple::CAM_INDEX>(selected_cmd);
            BankAddress selected_cmd_ba_addr = std::get<CommandTuple::BaAddress>(selected_cmd);
            RealBaIndex selected_cmd_real_ba = selected_cmd_ba_addr.real_ba;

            _sdram_constraint->InsertCommand(selected_cmd_type,selected_cmd_ba_addr);
            // bank slice manager update command
            _bankslice_manager->CommandUpdate(selected_cmd);

            // cam update
            if(selected_cmd_type.IsBankCommand()) // if command is Row access command --> ACT/PRE
            {
                bool is_rd = std::get<CommandTuple::CommandSrc>(selected_cmd) == CmdSrc::RdTransaction;
                auto trans = is_rd ? _scheduler->GetRdCam()->GetCamEntry(selected_cmd_cam_index)->GetRequest()
                                   : _scheduler->GetWrCam()->GetCamEntry(selected_cmd_cam_index)->GetRequest();
                auto phase = DFI_CMD;
                sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
                if(trans->get_extension<CmdExtension>() == nullptr)
                {
                    CmdExtension* cmd_ext = new CmdExtension();
                    trans->set_extension<CmdExtension>(cmd_ext);
                }
                trans->get_extension<CmdExtension>()->AddCommand(selected_cmd_type.type());
                if(selected_cmd_type.IsCASCommand()) // if command is Column access command --> RD/RDA, WR/WRA
                {
                    unsigned int trans_id = trans->get_extension<StatisticExtension>()->GetTransactionId();
                    trans->get_extension<StatisticExtension>()->RecordOutCamTime(sc_core::sc_time_stamp());
                    dfi_extension->AddTrans(trans,selected_cmd_type.GetCommandLength(false));
                    if(is_rd)
                    {
                        trans->get_extension<StatisticExtension>()->RecordCmdTime(sc_core::sc_time_stamp());
                        selected_cmd_type.IsApCommand() ? DramCommand::RDA : DramCommand::RD;

                        PriorityClass sending_cmd_qos_level = _scheduler->GetRdCam()->GetCamEntry(selected_cmd_cam_index)->GetQosLevel();
                        _scheduler->DeleteRdCamEntry(selected_cmd_cam_index);
                        _mode_switch->RdCmdSend();
                        if(sending_cmd_qos_level == PriorityClass::HPR)
                        {
                            _scheduler->GetRdCam()->HprCmdExe();
                            _scheduler->GetRdCam()->IncreaseHprCredit();
                            _scheduler->GetRdCam()->LprStarveCounter();
                            _scheduler->GetWrCam()->TpwStarveCounter();
                        }
                        else if(sending_cmd_qos_level == PriorityClass::LPR || sending_cmd_qos_level == PriorityClass::GPR)
                        {
                            _scheduler->GetRdCam()->LprCmdExe();
                            _scheduler->GetRdCam()->IncreaseLprCredit();
                            _scheduler->GetRdCam()->HprStarveCounter();
                            _scheduler->GetWrCam()->TpwStarveCounter();
                        }
                        _input_process->ReleaseRdCamIndex(selected_cmd_cam_index);
                        DPRINT_INFO(TOP_DEBUG,name(),"trans id:%d , delete rd cam index: %d, sending cmd: %s",trans->get_extension<StatisticExtension>()->GetTransactionId(),selected_cmd_cam_index,std::get<CommandTuple::Command>(selected_cmd).to_string().c_str());
                        _scheduler->UpdateRdNttPip(selected_cmd_real_ba,UpdateType::CmdExe);// do ntt "bank granted" update
                    }
                    else
                    {
                        trans->get_extension<StatisticExtension>()->RecordCmdTime(sc_core::sc_time_stamp(),
                        selected_cmd_type.IsApCommand() ? DramCommand::WRA : DramCommand::WR);

                        _scheduler->DeleteWrCamEntry(selected_cmd_cam_index);
                        _mode_switch->WrCmdSend();
                        _scheduler->GetWrCam()->TpwCmdExe();
                        _scheduler->GetWrCam()->IncreaseTpwCredit();
                        _scheduler->GetRdCam()->LprStarveCounter();
                        _scheduler->GetRdCam()->HprStarveCounter();

                        payload_event_queue.notify(*trans,DFI_WDAT_BEGIN,_config.mem_spec->tCWL);
                        _input_process->ReleaseWrCamIndex(selected_cmd_cam_index);
                        DPRINT_INFO(TOP_DEBUG,name(),"trans id:%d , delete wr cam index: %d, sending cmd: %s",trans->get_extension<StatisticExtension>()->GetTransactionId(),selected_cmd_cam_index,std::get<CommandTuple::Command>(selected_cmd).to_string().c_str());
                        _scheduler->UpdateWrNttPip(selected_cmd_real_ba,UpdateType::CmdExe);// do ntt "bank granted" update
                    }
                }
                if(selected_cmd_type.type() == Command::ACT)
                {
                    dfi_extension->AddTrans(trans,selected_cmd_type.GetCommandLength(false));
                    trans->get_extension<StatisticExtension>()->RecordCmdTime(sc_core::sc_time_stamp(),DramCommand::ACT);
                    if(is_rd)
                    {
                        DPRINT_INFO(TOP_DEBUG,name(),"trans id:%d , rd cam index: %d, sending cmd: %s",trans->get_extension<StatisticExtension>()->GetTransactionId(),selected_cmd_cam_index,std::get<CommandTuple::Command>(selected_cmd).to_string().c_str());
                        _scheduler->UpdateRdNttPip(selected_cmd_real_ba,UpdateType::Pre_Act);
                    }
                    else
                    {
                        DPRINT_INFO(TOP_DEBUG,name(),"trans id:%d , wr cam index: %d, sending cmd: %s",trans->get_extension<StatisticExtension>()->GetTransactionId(),selected_cmd_cam_index,std::get<CommandTuple::Command>(selected_cmd).to_string().c_str());
                        _scheduler->UpdateWrNttPip(selected_cmd_real_ba,UpdateType::Pre_Act);
                    }
                }
                else if(selected_cmd_type.type() == Command::PRE)
                {
                    _bankslice_manager->CommandUpdate(selected_cmd);
                }
                auto trans = _refresh_machine_manager->GetRefreshMachine(rank_index)->GetRefreshTrans();
                auto phase = DFI_CMD;
                sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
                assert(trans->get_extension<CmdExtension>() != nullptr);
                trans->get_extension<CmdExtension>()->AddCommand(selected_cmd_type.type());
                trans->get_extension<CmdExtension>()->SetAddress(selected_cmd_rank_addr);

                dfi_extension->AddTrans(trans,selected_cmd_type.GetCommandLength(false));
                iSocket->nb_transport_fw(*dfi_payload, phase, delay);

                // iSocket->nb_transport_fw(*trans, phase, delay);

                next_trigger_delay = std::min(next_trigger_delay , dfi_cycle_time);
            }
            else {
                DPRINT_FATAL("Memory Controller", "Invalid command type");
            }
        }

    }
    else
    {
        sc_core::sc_time next_refresh_trigger_time = _refresh_machine_manager->GetNextRefreshTriggerTime();

        if(next_refresh_trigger_time == sc_core::sc_time_stamp())
        {
            next_trigger_delay = std::min(next_trigger_delay, dfi_cycle_time);
        }
        else if(next_refresh_trigger_time >= dfi_cycle_time + sc_core::sc_time_stamp() && next_refresh_trigger_time < sc_core::sc_max_time())
        {
            next_trigger_delay = std::min(next_trigger_delay , _refresh_machine_manager->GetNextRefreshTriggerTime() - sc_core::sc_time_stamp());
        }
        else if(next_refresh_trigger_time == sc_core::sc_max_time()) {
            DPRINT_INFO(TOP_DEBUG,name(),"There is no Refresh needed to be sent");
        }
        else{
            DPRINT_INFO(TOP_DEBUG,name(),"Invalid condition");
        }

        sc_core::sc_time next_trigger_time = _bankslice_manager->GetNextCommandTriggerTime();
        if(next_trigger_time == sc_core::sc_time_stamp())
        {
            next_trigger_delay = std::min(next_trigger_delay, dfi_cycle_time);
        }
        else if(next_trigger_time >= dfi_cycle_time + sc_core::sc_time_stamp() && next_trigger_time < sc_core::sc_max_time())
        {
            next_trigger_delay = std::min(next_trigger_delay , _bankslice_manager->GetNextCommandTriggerTime() - sc_core::sc_time_stamp());
        }
        else if (next_trigger_time == sc_core::sc_max_time()) {
            DPRINT_INFO(TOP_DEBUG,name(),"There is no Bank cmd needed to be sent");
        }
        else{
            DPRINT_INFO(TOP_DEBUG,name(),"Invalid condition");
        }
    }
}

void
MemoryController::ReqUpdate()
{
    // NTT Update
    if(_scheduler->IsNeedUpdate())
    {
        DPRINT_INFO(TOP_DEBUG,name(),"[Ntt Update]:BEGIN");
        _bankslice_manager->NttUpdate();
        _scheduler->ResetUpdate();
        next_trigger_delay = std::min(next_trigger_delay,dfi_cycle_time);
        DPRINT_INFO(TOP_DEBUG,name(),"next trigger time: %s", (sc_core::sc_time_stamp()+next_trigger_delay).to_string().c_str());
        DPRINT_INFO(TOP_DEBUG,name(),"[Ntt Update]:END");
    }
    else
    {
        if (_scheduler->GetNextUpdateTime() != sc_core::sc_max_time())
        {
            DPRINT_ASSERT(_scheduler->GetNextUpdateTime() >= sc_core::sc_time_stamp(), "MemoryControlle", "ReqUpdate Func,NTT Update time is invalid: %s", _scheduler->GetNextUpdateTime().to_string().c_str());
            next_trigger_delay = std::min(next_trigger_delay, _scheduler->GetNextUpdateTime() - sc_core::sc_time_stamp());
            DPRINT_INFO(TOP_DEBUG,name()," Ntt next update time: %s", (sc_core::sc_time_stamp()+next_trigger_delay).to_string().c_str());
        }
    }
    // bsc allocation
    if(_bankslice_manager->IsNeedBankSliceAllocation())
    {
        DPRINT_INFO(TOP_DEBUG,name(),"[Bsc Allocate]:BEGIN");
        _bankslice_manager->BankSliceAllocation();
        _bankslice_manager->AllocationUpdate();
        if(_bankslice_manager->IsCurrentCycleAllocated())
        {
            next_trigger_delay = std::min(next_trigger_delay,dfi_cycle_time);// will do the bank allocation
        }
        DPRINT_INFO(TOP_DEBUG,name(),"next trigger time: %s", (sc_core::sc_time_stamp()+next_trigger_delay).to_string().c_str());
        DPRINT_INFO(TOP_DEBUG,name(),"[Bsc Allocate]:END");
    }
}
void
MemoryController::CqStore()
{
    // DPRINT_INFO(TOP_DEBUG,name(),"CqStore");
    if(!_input_process->IsPipBufferEmpty())
    {
        DPRINT_INFO(TOP_DEBUG,name(),"[CQ Store]:BEGIN");
        addr_collision_busy = false;
        RdCam* rd_cam = _scheduler->GetRdCam();
        WrCam* wr_cam = _scheduler->GetWrCam();
        if(!_scheduler->GetRdCam()->IsCamEmpty() || !_scheduler->GetWrCam()->IsCamEmpty())
        {
            _input_process->DetectAddrCollision();

            if(!rd_cam->IsRdCamBusy() && !wr_cam->IsWrCamBusy())
            {
                if(wr_cam->HasWrCombine())
                {
                    //wanmw,waw do wr merge if wr combine enable
                    // do write merge:
                    // 1. release the write cam index
                    // 2. set the write data is not ready
                }
                else {
                    std::pair<bool,unsigned> wr_store_result = _input_process->SendCmd2Cq();
                    if(wr_store_result.first)
                    {
                        tlm::tlm_generic_payload* trans = _scheduler->GetWrCam()->GetCamEntry(wr_store_result.second)->GetRequest();
                        trans->get_extension<UifExtension>()->SetWrDatRequestIndex(wr_store_result.second);
                        sc_core::sc_time delay = dfi_cycle_time;
                        payload_event_queue.notify(*trans, UIF_WDAT_REQ,delay);
                    }
                }
            }
            else {
                //stall
                DPRINT_INFO(TOP_DEBUG,name(),"Addr Collision Detect, causing flush");
                addr_collision_busy = true;
                if(rd_cam->IsRdCamBusy())
                {
                    for(auto busy_cam_index: rd_cam->GetRdCollisionCamIndex())
                    {
                        rd_cam->GetCamEntry(busy_cam_index)->print();
                    }
                }
                if(wr_cam->IsWrCamBusy())
                {
                    for(auto busy_cam_index: wr_cam->GetWrCollisionCamIndex())
                    {
                        wr_cam->GetCamEntry(busy_cam_index)->print();
                    }
                }
            }
        }
        else
        {
            std::pair<bool,unsigned> wr_store_result = _input_process->SendCmd2Cq();
            if(wr_store_result.first)
            {
                tlm::tlm_generic_payload* trans = _scheduler->GetWrCam()->GetCamEntry(wr_store_result.second)->GetRequest();
                trans->get_extension<UifExtension>()->SetWrDatRequestIndex(wr_store_result.second);
                sc_core::sc_time delay = dfi_cycle_time;
                payload_event_queue.notify(*trans, UIF_WDAT_REQ,delay);
            }
        }
        next_trigger_delay = std::min(next_trigger_delay,dfi_cycle_time);
        DPRINT_INFO(TOP_DEBUG,name(),"next trigger time: %f", (sc_core::sc_time_stamp()+next_trigger_delay).to_double());
        DPRINT_INFO(TOP_DEBUG,name(),"[CQ Store]:END");
    }
}

void
MemoryController::PipProcess()
{

}

    } // namespace Controller
} // namespace dmu