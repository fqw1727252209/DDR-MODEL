#include "Controller/MrdimmController.hh"
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

// Constructor
MrdimmController::MrdimmController(const sc_core::sc_module_name& name,
                                   const Configure& config,
                                   SdramConstraintIF* sdram_constraint,
                                   const std::string& output_dir)
: sc_module(name)
, m_config(config)
, m_sdram_constraint(dynamic_cast<SdramConstraintDDR5_3ds*>(sdram_constraint))
, dfi_cycle_time(config.mem_spec->tCK_mc)
, ddr_cycle_time(config.mem_spec->tCK)
, next_trigger_delay(sc_core::sc_max_time())
, dfi_payload{nullptr, nullptr}
, dfi_extension{nullptr, nullptr}
, dfi_buffer_sel(0)
, iSocket((std::string(name) + "_iSocket").c_str())
{
    unsigned num_pch = config.mem_spec->NumOfPseudoChannelsPerSubChannel;

    // 1. 初始化每个伪通道的组件
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        // 创建socket
        std::string tSocketName = std::string(name) + "_tSocket_" + std::to_string(pch_id);
        tSocket.emplace_back(std::make_unique<tlm_utils::simple_target_socket<MrdimmController>>(tSocketName.c_str()));

        // 绑定nb_transport_fw - 为每个伪通道绑定独立的回调函数
        if (pch_id == 0) {
            tSocket[pch_id]->register_nb_transport_fw(this, &MrdimmController::nb_transport_fw_0);
        } else {
            tSocket[pch_id]->register_nb_transport_fw(this, &MrdimmController::nb_transport_fw_1);
        }

        // 创建其他组件
        m_scheduler.push_back(std::make_unique<Scheduler>(m_config, pch_id));
        m_input_process.push_back(std::make_unique<InputProcess>(m_config, *m_scheduler.back(), pch_id));
        m_bankslice_manager.push_back(std::make_unique<BankSliceManager>(*m_scheduler.back(), m_config, pch_id));
        m_mrdimm_cmd_select.push_back(std::make_unique<MrdimmCmdSelect>(m_config, *m_bankslice_manager.back(), pch_id));
        m_refresh_manager.push_back(std::make_unique<RefreshMachineManager>(*m_bankslice_manager.back(), m_config, pch_id));

        // 创建每个伪通道的peq
        std::string peq_name = "peq_" + std::to_string(pch_id);
        m_payload_event_queues.push_back(
            std::make_unique<tlm_utils::peq_with_cb_and_phase<MrdimmController>>(
                peq_name.c_str(), this,
                pch_id == 0 ? &MrdimmController::pipline_method_0 : &MrdimmController::pipline_method_1
            )
        );

        // 初始化状态
        addr_collision_busy.push_back(false);
        refresh_ready_commands.emplace_back();

        // 创建控制器全局日志
        Logger::getInstance("Mc_"+std::to_string(pch_id)).initialize("Mc_"+std::to_string(pch_id),output_dir,"Mc_"+std::to_string(pch_id));
        Logger::getInstance("Scheduler_"+std::to_string(pch_id)).initialize("Scheduler_"+std::to_string(pch_id),output_dir,"Scheduler_"+std::to_string(pch_id));
    }

    // 2. 创建全局组件
    Logger::getInstance("MrdimmModeSwitch").initialize("MrdimmModeSwitch", output_dir, "MrdimmModeSwitch");
    m_mrdimm_mode_switch = std::make_unique<MrdimmModeSwitch>(config, m_scheduler, m_bankslice_manager);
    iSocket.register_nb_transport_bw(this, &MrdimmController::nb_transport_bw);

    // 3. 初始化DFI相关 (双缓冲)
    for (unsigned i = 0; i < 2; i++) {
        dfi_payload[i] = new tlm::tlm_generic_payload();
        dfi_extension[i] = new DfiCmdExtension(config.mem_spec->FreqRatio, false);
        dfi_payload[i]->set_extension(dfi_extension[i]);
    }

    SC_HAS_PROCESS(MrdimmController);
    // 4. 注册SC_METHOD - ControllerMethod
    SC_METHOD(ControllerMethod);
    sensitive << ctrl_event;
    dont_initialize();

    // 5. 注册CreditSend为独立SC_METHOD，由DFI时钟下降沿触发
    SC_METHOD(CreditSend);
    sensitive << dfi_clock.neg();
    dont_initialize();

    // 6. 为每个伪通道的Scheduler注册映射表
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        m_scheduler[pch_id]->RegisterBa2BscTable(m_bankslice_manager[pch_id]->GetBa2BscTable());
        m_scheduler[pch_id]->RegisterBscSliceMap(m_bankslice_manager[pch_id]->GetBankSliceMap());
    }
}

// Destructor
MrdimmController::~MrdimmController()
{
    ControllerFinishCheck();
    for (unsigned i = 0; i < 2; i++) {
        delete dfi_payload[i];
    }
}

// nb_transport_fw for pseudo channel 0
tlm::tlm_sync_enum
MrdimmController::nb_transport_fw_0(tlm::tlm_generic_payload& trans,
                                    tlm::tlm_phase& phase,
                                    sc_core::sc_time& delay)
{
    return nb_transport_fw_internal(trans, phase, delay, 0);
}

// nb_transport_fw for pseudo channel 1
tlm::tlm_sync_enum
MrdimmController::nb_transport_fw_1(tlm::tlm_generic_payload& trans,
                                    tlm::tlm_phase& phase,
                                    sc_core::sc_time& delay)
{
    return nb_transport_fw_internal(trans, phase, delay, 1);
}

// Internal nb_transport_fw implementation
tlm::tlm_sync_enum
MrdimmController::nb_transport_fw_internal(tlm::tlm_generic_payload& trans,
                                           tlm::tlm_phase& phase,
                                           sc_core::sc_time& delay,
                                           unsigned pseudo_channel_id)
{
    if (phase == UIF_REQ) {
        DPRINT_INFO(MEMORY_CONTROLLER, name(), "nb_transport_fw_internal PCH %d: UIF_REQ received", pseudo_channel_id);

        ctrl_event.notify(dfi_cycle_time);

        // 背压检查
        if (addr_collision_busy[pseudo_channel_id]) {
            DPRINT_INFO(MEMORY_CONTROLLER, name(), "PCH %d: Address collision busy, returning TLM_UPDATED", pseudo_channel_id);
            return tlm::TLM_UPDATED;
        }

        if (m_scheduler[pseudo_channel_id]->IsTpwFull() &&
            trans.get_command() == tlm::TLM_WRITE_COMMAND) {
            DPRINT_INFO(MEMORY_CONTROLLER, name(), "PCH %d: TPW CAM full, returning TLM_UPDATED", pseudo_channel_id);
            return tlm::TLM_UPDATED;
        }

        if (m_scheduler[pseudo_channel_id]->IsHprFull() &&
            trans.get_command() == tlm::TLM_READ_COMMAND &&
            trans.get_extension<UifExtension>()->GetQosLevel() == PriorityClass::HPR) {
            DPRINT_INFO(MEMORY_CONTROLLER, name(), "PCH %d: HPR CAM full, returning TLM_UPDATED", pseudo_channel_id);
            return tlm::TLM_UPDATED;
        }

        if (m_scheduler[pseudo_channel_id]->IsLprFull() &&
            trans.get_command() == tlm::TLM_READ_COMMAND &&
            (trans.get_extension<UifExtension>()->GetQosLevel() == PriorityClass::LPR ||
             trans.get_extension<UifExtension>()->GetQosLevel() == PriorityClass::GPR)) {
            DPRINT_INFO(MEMORY_CONTROLLER, name(), "PCH %d: LPR/GPR CAM full, returning TLM_UPDATED", pseudo_channel_id);
            return tlm::TLM_UPDATED;
        }

        // 接受请求
        DPRINT_INFO(MEMORY_CONTROLLER, name(), "PCH %d: Accepting request", pseudo_channel_id);
        m_input_process[pseudo_channel_id]->AcceptRequest(trans);
        return tlm::TLM_ACCEPTED;
    } else {
        // 非UIF_REQ phase放入对应伪通道的peq
        DPRINT_INFO(false, "MrdimmController", "PCH %d: Non-UIF_REQ phase, notifying peq", pseudo_channel_id);
        m_payload_event_queues[pseudo_channel_id]->notify(trans, phase, delay);
    }
    return tlm::TLM_ACCEPTED;
}

// nb_transport_bw
tlm::tlm_sync_enum
MrdimmController::nb_transport_bw(tlm::tlm_generic_payload& trans,
                                  tlm::tlm_phase& phase,
                                  sc_core::sc_time& delay)
{
    // 从CmdExtension中获取伪通道ID
    CmdExtension* cmd_ext = trans.get_extension<CmdExtension>();

    unsigned pseudo_channel_id = 0;
    if (cmd_ext != nullptr) {
        BankAddress cmd_addr = cmd_ext->GetAddress();
        pseudo_channel_id = cmd_addr.pseudo_ch;
    }

    if (pseudo_channel_id >= m_config.mem_spec->NumOfPseudoChannelsPerSubChannel) {
        DPRINT_FATAL("MrdimmController", "Invalid pseudo channel id: %d", pseudo_channel_id);
        return tlm::TLM_ACCEPTED;
    }

    // 分发到对应伪通道的peq
    if (phase == WR_RESPONSE_COMPLETE)
    {
        m_payload_event_queues[pseudo_channel_id]->notify(trans, phase, delay);
    }
    else
    {
        (*tSocket[pseudo_channel_id])->nb_transport_bw(trans, phase, delay);
    }

    return tlm::TLM_ACCEPTED;
}

// ControllerMethod - 单一方法处理两个伪通道
void
MrdimmController::ControllerMethod()
{
    DPRINT_INFO(true, "MrdimmController", "ControllerMethod function start");
    next_trigger_delay = sc_core::sc_max_time();
    unsigned num_pch = m_config.mem_spec->NumOfPseudoChannelsPerSubChannel;

    // 1. BankSlice释放 - 每个伪通道独立
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        if (m_bankslice_manager[pch_id]->IsNeedBankSliceRelease()) {
            DPRINT_INFO(TOP_DEBUG, name(), "Bank Slice Release for PCH %d", pch_id);
            m_bankslice_manager[pch_id]->BankSliceRelease();
            next_trigger_delay = dfi_cycle_time;
        }
    }

    // 2. 命令发送 - 全局协调 (两个伪通道的命令一起发送到DFI)
    // DPRINT_INFO(true, "MrdimmController", "CmdSend");
    CmdSend();

    // 3. 请求更新 - 每个伪通道独立
    // DPRINT_INFO(true, "MrdimmController", "ReqUpdate");
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        ReqUpdate(pch_id);
    }

    // 4. CQ存储 - 每个伪通道独立
    // DPRINT_INFO(true, "MrdimmController", "CqStore");
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        CqStore(pch_id);
    }

    // 触发下一次执行
    if (next_trigger_delay != sc_core::sc_max_time()) {
        assert(next_trigger_delay >= dfi_cycle_time);
        ctrl_event.notify(next_trigger_delay);
    } else {
        DPRINT_INFO(TOP_DEBUG, name(), "No Event will scheduled");
    }
}

// AcTimingUpdate - 全局统一
void
MrdimmController::AcTimingUpdate()
{
    unsigned num_pch = m_config.mem_spec->NumOfPseudoChannelsPerSubChannel;

    // 1. 处理所有伪通道的刷新命令时序
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        for (auto rank_index : m_refresh_manager[pch_id]->GetRefreshRankIds()) {
            auto refresh_machine = m_refresh_manager[pch_id]->GetRefreshMachine(rank_index);
            refresh_machine->Evaluate();
            const BankAddress rank_addr = refresh_machine->GetRankAddress();
            const Command refresh_cmd = refresh_machine->GetRefreshCommand();
            sc_core::sc_time& refresh_command_avail_time = refresh_machine->GetRefreshCommandAvailTime();

            if (refresh_cmd == Command::NOP) {
                refresh_command_avail_time = sc_core::sc_max_time();
            } else {
                refresh_command_avail_time = m_sdram_constraint->TimeToSatisfyConstraints(refresh_cmd, rank_addr);
            }

            DPRINT_INFO(MEMORY_CONTROLLER && m_config.controller_config->REFRESH_ENABLE,
                        "MrdimmController", "Refresh Rank: %d, Command: %s, Avail Time: %s",
                        rank_index, refresh_cmd.to_string().c_str(),
                        refresh_command_avail_time.to_string().c_str());
        }
    }

    // 2. 处理所有伪通道的Bank命令时序
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        for (auto& allocated_bsc : m_bankslice_manager[pch_id]->GetAllocatedBscSet()) {
            auto bank_slice = m_bankslice_manager[pch_id]->GetBsc(allocated_bsc);
            bank_slice->Evaluate();
            const BankAddress ba_addr = bank_slice->GetBaAddr();

            // 读命令时序
            const Command::Type& rd_cmd = bank_slice->GetRdCmd();
            sc_core::sc_time& rd_cmd_avail_time = bank_slice->GetRdCmdAvailTime();
            if (rd_cmd == Command::NOP) {
                rd_cmd_avail_time = sc_core::sc_max_time();
            } else {
                rd_cmd_avail_time = m_sdram_constraint->TimeToSatisfyConstraints(rd_cmd, ba_addr);
            }

            // 写命令时序
            const Command::Type& wr_cmd = bank_slice->GetWrCmd();
            sc_core::sc_time& wr_cmd_avail_time = bank_slice->GetWrCmdAvailTime();
            if (wr_cmd == Command::NOP) {
                wr_cmd_avail_time = sc_core::sc_max_time();
            } else {
                wr_cmd_avail_time = m_sdram_constraint->TimeToSatisfyConstraints(wr_cmd, ba_addr);
            }

            // Bsc 命令时序: Force Pre, Refresh-Pre
            const Command::Type& bsc_cmd = bank_slice->GetBankCmd();
            sc_core::sc_time& bsc_cmd_avail_time = bank_slice->GetBankCmdAvailTime();
            if (bsc_cmd == Command::NOP) {
                bsc_cmd_avail_time = sc_core::sc_max_time();
            } else {
                bsc_cmd_avail_time = m_sdram_constraint->TimeToSatisfyConstraints(bsc_cmd, ba_addr);
            }
        }
    }
}

// CmdSend - 全局协调
void
MrdimmController::CmdSend()
{
    unsigned num_pch = m_config.mem_spec->NumOfPseudoChannelsPerSubChannel;

    // 1. 全局AC时序更新
    AcTimingUpdate();

    // 2. 清空每个伪通道的就绪命令
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        refresh_ready_commands[pch_id].clear();
    }

    // 3. 收集所有伪通道的刷新命令
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        if (!m_refresh_manager[pch_id]->IsRefreshReadyCommandsEmpty()) {
            ReadyCommands& refresh_cmds = m_refresh_manager[pch_id]->GetRefreshReadyCommands();
            refresh_ready_commands[pch_id].insert(
                refresh_ready_commands[pch_id].end(),
                std::make_move_iterator(refresh_cmds.begin()),
                std::make_move_iterator(refresh_cmds.end())
            );
        }
    }

    // 4. 获取全局读写状态
    m_mrdimm_mode_switch->UpdateGlobalState();
    GlobalRdWrState global_rdwr_mode = m_mrdimm_mode_switch->GetGlobalRdWrState();
    DPRINT_INFO(MODE_SWITCH, "MrdimmController", "Get Global RdWr State: %s", PrintState(global_rdwr_mode).c_str());

    // 5. 收集每个伪通道的就绪命令，独立选择命令

    std::vector<std::optional<CommandTuple::Type>> selected_cmds(num_pch);
    bool has_cmd_to_send = false;
    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        bool act_advance_wr = m_mrdimm_mode_switch->GetPseudoChannelWriteAdvanceState(pch_id);
        bool act_advance_rd = m_mrdimm_mode_switch->GetPseudoChannelReadAdvanceState(pch_id);
        if (!m_bankslice_manager[pch_id]->IsReadyCommandsEmpty(global_rdwr_mode,act_advance_rd,act_advance_wr)
            || !refresh_ready_commands[pch_id].empty()) {

            selected_cmds[pch_id] = m_mrdimm_cmd_select[pch_id]->SelectCommand(m_bankslice_manager[pch_id]->GetBscReadyCommands(), refresh_ready_commands[pch_id], global_rdwr_mode);
            has_cmd_to_send = true;
            DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: Selected command", pch_id);
        }
    }

    // 7. 发送命令到DFI
    if (has_cmd_to_send) {
        auto* cur_payload = dfi_payload[dfi_buffer_sel];
        auto* cur_extension = dfi_extension[dfi_buffer_sel];

        // 重置DfiExtension
        cur_extension->reset();
        assert(!cur_extension->HasCommand() && "dfi_extension should not has command this cycle");

        // 处理每个伪通道选中的命令
        for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
            if (selected_cmds[pch_id].has_value()) {
                auto& cmd_tuple = selected_cmds[pch_id].value();
                Command cmd_type = std::get<CommandTuple::Command>(cmd_tuple);
                BankAddress ba_addr = std::get<CommandTuple::BaAddress>(cmd_tuple);
                unsigned real_ba = ba_addr.real_ba;
                CAM_INDEX cam_index = std::get<CommandTuple::CAM_INDEX>(cmd_tuple);
                CmdSrc cmd_src = std::get<CommandTuple::CommandSrc>(cmd_tuple);

                DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: Processing command %s",
                            pch_id, cmd_type.to_string().c_str());

                // 插入时序约束
                m_sdram_constraint->InsertCommand(cmd_type, ba_addr);

                // 更新BankSliceManager
                m_bankslice_manager[pch_id]->CommandUpdate(cmd_tuple);
                // 更新RefreshManager
                if(cmd_type.IsRefCommand())
                {
                    m_refresh_manager[pch_id]->CommandUpdate(cmd_tuple);
                }

                // 获取事务
                tlm::tlm_generic_payload* trans = nullptr;
                if (cmd_src == CmdSrc::RdTransaction) {
                    trans = m_scheduler[pch_id]->GetRdCam()->GetCamEntry(cam_index)->GetRequest();
                    DPRINT_INFO(TOP_DEBUG, "Mrdimm Command Send", "Rd trans ID: %d, trans ptr: %p",
                                trans->get_extension<StatisticExtension>()->GetTransactionId(),static_cast<void*>(trans));
                } else if (cmd_src == CmdSrc::WrTransaction)
                {
                    trans = m_scheduler[pch_id]->GetWrCam()->GetCamEntry(cam_index)->GetRequest();
                    DPRINT_INFO(TOP_DEBUG, "Mrdimm Command Send", "Wr trans ID: %d, trans ptr: %p",
                                trans->get_extension<StatisticExtension>()->GetTransactionId(),static_cast<void*>(trans));
                } else if (cmd_src == CmdSrc::BankTransaction)
                {
                    trans = m_bankslice_manager[pch_id]->GetBsc(real_ba)->GetBankTlmPayload();
                } else if (cmd_src == CmdSrc::RankTransaction)
                {
                    unsigned rank_id = ba_addr.real_cid;
                    trans = m_refresh_manager[pch_id]->GetRefreshMachine(rank_id)->GetRefreshTrans();
                }

                // 设置CmdExtension
                CmdExtension* cmd_ext = trans->get_extension<CmdExtension>();
                if (cmd_ext == nullptr) {
                    cmd_ext = new CmdExtension();
                    trans->set_extension<CmdExtension>(cmd_ext);
                }
                cmd_ext->AddCommand(cmd_type.type());
                cmd_ext->SetAddress(ba_addr);

                // 添加到DfiExtension
                bool is_even = pch_id % 2 == 0;
                cur_extension->AddTrans(trans, cmd_type.GetCommandLength(false), is_even);

                // 记录统计信息
                auto stat_ext = trans->get_extension<StatisticExtension>();
                if (stat_ext != nullptr) {
                    stat_ext->RecordOutCamTime(sc_core::sc_time_stamp());
                    if (cmd_type.IsCASCommand()) {
                        bool is_rd = cmd_src == CmdSrc::RdTransaction;
                        if (is_rd) {
                            stat_ext->RecordCmdTime(sc_core::sc_time_stamp(),
                                                    cmd_type.IsApCommand() ? DramCommand::RDA : DramCommand::RD);
                        } else {
                            stat_ext->RecordCmdTime(sc_core::sc_time_stamp(),
                                                    cmd_type.IsApCommand() ? DramCommand::WRA : DramCommand::WR);
                        }
                    } else if (cmd_type.type() == Command::ACT) {
                        stat_ext->RecordCmdTime(sc_core::sc_time_stamp(), DramCommand::ACT);
                    } else if (cmd_type.type() == Command::PRE) {
                        stat_ext->RecordCmdTime(sc_core::sc_time_stamp(), DramCommand::PRE);
                    }
                }

                // 更新CAM和状态
                if (cmd_type.IsCASCommand()) {
                    bool is_rd = cmd_src == CmdSrc::RdTransaction;
                    if (is_rd) {
                        // 读命令处理
                        auto qos_level = m_scheduler[pch_id]->GetRdCam()->GetCamEntry(cam_index)->GetQosLevel();
                        m_scheduler[pch_id]->DeleteRdCamEntry(cam_index);
                        // m_mrdimm_mode_switch->RdCmdSend();

                        // Credit更新
                        if (qos_level == PriorityClass::HPR) {
                            m_scheduler[pch_id]->GetRdCam()->HprCmdExe();
                            m_scheduler[pch_id]->GetRdCam()->IncreaseHprCredit();
                            m_scheduler[pch_id]->GetRdCam()->LprStarveCounter();
                            m_scheduler[pch_id]->GetWrCam()->TpwStarveCounter();
                        } else if (qos_level == PriorityClass::LPR || qos_level == PriorityClass::GPR) {
                            m_scheduler[pch_id]->GetRdCam()->LprCmdExe();
                            m_scheduler[pch_id]->GetRdCam()->IncreaseLprCredit();
                            m_scheduler[pch_id]->GetRdCam()->HprStarveCounter();
                            m_scheduler[pch_id]->GetWrCam()->TpwStarveCounter();
                        }

                        m_input_process[pch_id]->ReleaseRdCamIndex(cam_index);
                        DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: Deleted RD CAM index %d", pch_id, cam_index);
                        m_scheduler[pch_id]->UpdateRdNttPip(ba_addr.real_ba, UpdateType::CmdExe);
                    } else {
                        // 写命令处理
                        m_scheduler[pch_id]->DeleteWrCamEntry(cam_index);
                        // m_mrdimm_mode_switch->WrCmdSend();
                        m_scheduler[pch_id]->GetWrCam()->TpwCmdExe();
                        m_scheduler[pch_id]->GetWrCam()->IncreaseTpwCredit();
                        m_scheduler[pch_id]->GetRdCam()->LprStarveCounter();
                        m_scheduler[pch_id]->GetRdCam()->HprStarveCounter();

                        // 通知写数据
                        m_payload_event_queues[pch_id]->notify(*trans, DFI_WDAT_BEGIN, m_config.mem_spec->tCWL_mc);
                        m_input_process[pch_id]->ReleaseWrCamIndex(cam_index);
                        DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: Deleted WR CAM index %d", pch_id, cam_index);
                        m_scheduler[pch_id]->UpdateWrNttPip(ba_addr.real_ba, UpdateType::CmdExe);
                    }
                } else if (cmd_type.type() == Command::ACT) {
                    bool is_rd = cmd_src == CmdSrc::RdTransaction;
                    // 激活命令
                    if (is_rd) {
                        m_scheduler[pch_id]->UpdateRdNttPip(ba_addr.real_ba, UpdateType::Pre_Act);
                    } else {
                        m_scheduler[pch_id]->UpdateWrNttPip(ba_addr.real_ba, UpdateType::Pre_Act);
                    }
                } else if (cmd_type.type() == Command::PRE) {
                    // 预充电命令
                    m_scheduler[pch_id]->UpdateRdNttPip(ba_addr.real_ba, UpdateType::Pre_Act);
                    m_scheduler[pch_id]->UpdateWrNttPip(ba_addr.real_ba, UpdateType::Pre_Act);
                } else if (cmd_type.IsRefCommand()) {

                } else {

                }
            }
        }

        // 发送DFI命令
        if (cur_extension->HasNextCommand()) {
            tlm::tlm_phase phase = DFI_CMD;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            iSocket->nb_transport_fw(*cur_payload, phase, delay);
            next_trigger_delay = std::min(next_trigger_delay, dfi_cycle_time);
            DPRINT_INFO(TOP_DEBUG, name(), "Sent DFI command");
        }

        // 切换DFI双缓冲
        dfi_buffer_sel = 1 - dfi_buffer_sel;
    } else {
        // 计算下一次触发时间
        sc_core::sc_time next_refresh_trigger_time = sc_core::sc_max_time();
        for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
            sc_core::sc_time pch_next_refresh = m_refresh_manager[pch_id]->GetNextRefreshTriggerTime();
            if (pch_next_refresh < next_refresh_trigger_time) {
                next_refresh_trigger_time = pch_next_refresh;
            }
        }

        if (next_refresh_trigger_time == sc_core::sc_time_stamp()) {
            next_trigger_delay = std::min(next_trigger_delay, dfi_cycle_time);
        } else if (next_refresh_trigger_time >= dfi_cycle_time + sc_core::sc_time_stamp() &&
                   next_refresh_trigger_time < sc_core::sc_max_time()) {
            next_trigger_delay = std::min(next_trigger_delay,
                                          next_refresh_trigger_time - sc_core::sc_time_stamp());
        }
        // else if (next_refresh_trigger_time == sc_core::sc_max_time()) {
        //     DPRINT_INFO(TOP_DEBUG, name(), "There is no Refresh needed to be sent");
        // }
        // else {
        //     DPRINT_INFO(TOP_DEBUG, name(), "Invalid condition");
        // }

        sc_core::sc_time next_trigger_time = sc_core::sc_max_time();
        for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
            sc_core::sc_time pch_next_trigger = m_bankslice_manager[pch_id]->GetNextCommandTriggerTime();
            if (pch_next_trigger < next_trigger_time) {
                next_trigger_time = pch_next_trigger;
            }
        }

        if (next_trigger_time == sc_core::sc_time_stamp()) {
            next_trigger_delay = std::min(next_trigger_delay, dfi_cycle_time);
        } else if (next_trigger_time >= dfi_cycle_time + sc_core::sc_time_stamp() &&
                   next_trigger_time < sc_core::sc_max_time()) {
            next_trigger_delay = std::min(next_trigger_delay,
                                          next_trigger_time - sc_core::sc_time_stamp());
        }
        // else if (next_trigger_time == sc_core::sc_max_time()) {
        //     DPRINT_INFO(TOP_DEBUG, name(), "There is no Bank cmd needed to be sent");
        // }
        // else {
        //     DPRINT_INFO(TOP_DEBUG, name(), "Invalid condition");
        // }
    }
}

// ReqUpdate - 每个伪通道
void
MrdimmController::ReqUpdate(unsigned pseudo_channel_id)
{
    // NTT Update
    if (m_scheduler[pseudo_channel_id]->IsNeedUpdate()) {
        DPRINT_INFO(TOP_DEBUG, name(), "[Ntt Update PCH %d]:BEGIN", pseudo_channel_id);
        m_bankslice_manager[pseudo_channel_id]->NttUpdate();
        m_scheduler[pseudo_channel_id]->ResetUpdate();
        next_trigger_delay = std::min(next_trigger_delay, dfi_cycle_time);
        DPRINT_INFO(TOP_DEBUG, name(), "[Ntt Update PCH %d]:END", pseudo_channel_id);
    } else {
        sc_core::sc_time next_update = m_scheduler[pseudo_channel_id]->GetNextUpdateTime();
        if (next_update != sc_core::sc_max_time()) {
            DPRINT_ASSERT(next_update >= sc_core::sc_time_stamp(), "MrdimmController",
                          "ReqUpdate Func, NTT Update time is invalid: %s", next_update.to_string().c_str());
            next_trigger_delay = std::min(next_trigger_delay,
                                          next_update - sc_core::sc_time_stamp());
        }
    }

    // BankSlice分配
    if (m_bankslice_manager[pseudo_channel_id]->IsNeedBankSliceAllocation()) {
        DPRINT_INFO(TOP_DEBUG, name(), "[Bsc Allocate PCH %d]:BEGIN", pseudo_channel_id);
        m_bankslice_manager[pseudo_channel_id]->BankSliceAllocation();
        m_bankslice_manager[pseudo_channel_id]->AllocationUpdate();
        if (m_bankslice_manager[pseudo_channel_id]->IsCurrentCycleAllocated()) {
            next_trigger_delay = std::min(next_trigger_delay, dfi_cycle_time);
        }
        DPRINT_INFO(TOP_DEBUG, name(), "[Bsc Allocate PCH %d]:END", pseudo_channel_id);
    }
}

// CqStore - 每个伪通道
void
MrdimmController::CqStore(unsigned pseudo_channel_id)
{
    if (!m_input_process[pseudo_channel_id]->IsPipBufferEmpty()) {
        DPRINT_INFO(TOP_DEBUG, name(), "[CQ Store PCH %d]:BEGIN", pseudo_channel_id);
        addr_collision_busy[pseudo_channel_id] = false;

        RdCam* rd_cam = m_scheduler[pseudo_channel_id]->GetRdCam();
        WrCam* wr_cam = m_scheduler[pseudo_channel_id]->GetWrCam();

        if (!rd_cam->IsCamEmpty() || !wr_cam->IsCamEmpty()) {
            m_input_process[pseudo_channel_id]->DetectAddrCollision();

            if (!rd_cam->IsRdCamBusy() && !wr_cam->IsWrCamBusy()) {
                // 无地址冲突，执行CQ存储
                std::pair<bool, unsigned> wr_store_result =
                    m_input_process[pseudo_channel_id]->SendCmd2Cq();

                if (wr_store_result.first) {
                    // 写命令需要请求数据
                    tlm::tlm_generic_payload* trans =
                        wr_cam->GetCamEntry(wr_store_result.second)->GetRequest();
                    trans->get_extension<UifExtension>()->SetWrDatRequestIndex(wr_store_result.second);
                    m_payload_event_queues[pseudo_channel_id]->notify(*trans, UIF_WDAT_REQ, dfi_cycle_time);
                    DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: Sent UIF_WDAT_REQ for WR CAM %d",
                                pseudo_channel_id, wr_store_result.second);
                }
            } else {
                // 地址冲突，stall
                DPRINT_INFO(TOP_DEBUG, name(), "Addr Collision Detect PCH %d, causing flush", pseudo_channel_id);
                addr_collision_busy[pseudo_channel_id] = true;

                if (rd_cam->IsRdCamBusy()) {
                    for (auto busy_cam_index : rd_cam->GetRdCollisionCamIndex()) {
                        rd_cam->GetCamEntry(busy_cam_index)->print();
                    }
                }

                if (wr_cam->IsWrCamBusy()) {
                    for (auto busy_cam_index : wr_cam->GetWrCollisionCamIndex()) {
                        wr_cam->GetCamEntry(busy_cam_index)->print();
                    }
                }
            }
        } else {
            // CAM为空，直接存储
            std::pair<bool, unsigned> wr_store_result =
                m_input_process[pseudo_channel_id]->SendCmd2Cq();

            if (wr_store_result.first) {
                tlm::tlm_generic_payload* trans =
                    wr_cam->GetCamEntry(wr_store_result.second)->GetRequest();
                trans->get_extension<UifExtension>()->SetWrDatRequestIndex(wr_store_result.second);
                m_payload_event_queues[pseudo_channel_id]->notify(*trans, UIF_WDAT_REQ, dfi_cycle_time);
                DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: Sent UIF_WDAT_REQ for WR CAM %d",
                            pseudo_channel_id, wr_store_result.second);
            }
        }

        next_trigger_delay = std::min(next_trigger_delay, dfi_cycle_time);
        DPRINT_INFO(TOP_DEBUG, name(), "[CQ Store PCH %d]:END", pseudo_channel_id);
    }
}

// CreditSend - 独立SC_METHOD，内部循环处理两个伪通道
void
MrdimmController::CreditSend()
{
    unsigned num_pch = m_config.mem_spec->NumOfPseudoChannelsPerSubChannel;

    // 内部循环处理两个伪通道的credit发送
    for (unsigned pseudo_channel_id = 0; pseudo_channel_id < num_pch; pseudo_channel_id++) {
        // 检查是否需要发送credit
        bool hpr_credit = m_scheduler[pseudo_channel_id]->HasHprCredit();
        bool lpr_credit = m_scheduler[pseudo_channel_id]->HasLprCredit();
        bool tpw_credit = m_scheduler[pseudo_channel_id]->HasTpwCredit();

        if (hpr_credit || lpr_credit || tpw_credit) {
            DPRINT_INFO(MEMORY_CONTROLLER, name(), "PCH %d: Sending credits HPR=%d LPR=%d TPW=%d",
                        pseudo_channel_id, hpr_credit, lpr_credit, tpw_credit);

            tlm::tlm_generic_payload* credit_trans = new tlm::tlm_generic_payload();
            UifSideBandInfo uif_side_band_info;
            uif_side_band_info.hpr_credit_valid = hpr_credit;
            uif_side_band_info.lpr_credit_valid = lpr_credit;
            uif_side_band_info.tpw_credit_valid = tpw_credit;
            if (uif_side_band_info.hpr_credit_valid)
            {
                m_scheduler[pseudo_channel_id]->GetRdCam()->DecreaseHprCredit();
            }
            if (uif_side_band_info.lpr_credit_valid)
            {
                m_scheduler[pseudo_channel_id]->GetRdCam()->DecreaseLprCredit();
            }
            if (uif_side_band_info.tpw_credit_valid)
            {
                m_scheduler[pseudo_channel_id]->GetWrCam()->DecreaseTpwCredit();
            }

            UifSideBandExtension* uif_side_band_extension = new UifSideBandExtension(uif_side_band_info);
            credit_trans->set_extension<UifSideBandExtension>(uif_side_band_extension);

            tlm::tlm_phase phase = UIF_CREDIT;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            (*tSocket[pseudo_channel_id])->nb_transport_bw(*credit_trans, phase, delay);
        }
    }
}

// pipline_method for pseudo channel 0
void
MrdimmController::pipline_method_0(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase)
{
    pipline_method_internal(trans, phase, 0);
}

// pipline_method for pseudo channel 1
void
MrdimmController::pipline_method_1(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase)
{
    pipline_method_internal(trans, phase, 1);
}

// Internal pipline_method implementation
void
MrdimmController::pipline_method_internal(tlm::tlm_generic_payload& trans,
                                          const tlm::tlm_phase& phase,
                                          unsigned pseudo_channel_id)
{
    if (phase == DFI_RDAT_END) {
        // 读数据返回，转发到对应的tSocket
        DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: DFI_RDAT_END received", pseudo_channel_id);

        tlm::tlm_phase rdat_phase = UIF_RDAT_BEGIN;
        sc_core::sc_time rdat_delay = dfi_cycle_time;
        (*tSocket[pseudo_channel_id])->nb_transport_bw(trans, rdat_phase, rdat_delay);

        if (trans.get_data_length() > 32) {
            tlm::tlm_phase uif_rdat_end_phase = UIF_RDAT_END;
            sc_core::sc_time rdat_delay_2 = dfi_cycle_time + dfi_cycle_time;
            (*tSocket[pseudo_channel_id])->nb_transport_bw(trans, uif_rdat_end_phase, rdat_delay_2);
        } else {
            tlm::tlm_phase uif_rdat_end_phase = UIF_RDAT_END;
            sc_core::sc_time rdat_delay_2 = dfi_cycle_time;
            (*tSocket[pseudo_channel_id])->nb_transport_bw(trans, uif_rdat_end_phase, rdat_delay_2);
        }
    } else if (phase == DFI_WDAT_BEGIN) {
        // 写数据开始
        DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: DFI_WDAT_BEGIN received", pseudo_channel_id);

        m_payload_event_queues[pseudo_channel_id]->notify(trans, DFI_WDAT_END,
            m_config.mem_spec->tBurst_mc); // ddr_cycle_time

        tlm::tlm_phase wdat_phase = phase;
        sc_core::sc_time wdat_delay = sc_core::SC_ZERO_TIME;
        iSocket->nb_transport_fw(trans, wdat_phase, wdat_delay);
    } else if (phase == DFI_WDAT_END) {
        // 写数据结束
        DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: DFI_WDAT_END received", pseudo_channel_id);

        tlm::tlm_phase wdat_phase = phase;
        sc_core::sc_time wdat_delay = sc_core::SC_ZERO_TIME;
        iSocket->nb_transport_fw(trans, wdat_phase, wdat_delay);
    } else if (phase == UIF_WDAT_REQ) {
        // 写数据请求，转发到对应的tSocket
        DPRINT_INFO(true, "MrdimmController", "Wdat Request for Write Data PCH %d", pseudo_channel_id);

        tlm::tlm_phase wdat_phase = phase;
        sc_core::sc_time wdat_delay = sc_core::SC_ZERO_TIME;
        (*tSocket[pseudo_channel_id])->nb_transport_bw(trans, wdat_phase, wdat_delay);
    } else if (phase == UIF_WDAT_END) {
        // 写数据从UIF返回，更新CAM状态
        DPRINT_INFO(TOP_DEBUG, name(), "PCH %d: UIF_WDAT_END received", pseudo_channel_id);

        CAM_INDEX wr_cam_index = trans.get_extension<UifExtension>()->uif_info.wr_cam_index;
        auto wr_cam = m_scheduler[pseudo_channel_id]->GetWrCam();

        if (!wr_cam->IsCamExist(wr_cam_index)) {
            DPRINT_FATAL("MrdimmController", "PCH %d: Wr Cam Index %d does not exist",
                         pseudo_channel_id, wr_cam_index);
            return;
        }

        wr_cam->SetWdataReady(wr_cam_index);

        auto wr_cam_entry = wr_cam->GetCamEntry(wr_cam_index);
        RealBaIndex request_ba_index = wr_cam_entry->GetCamEntryRealBa();

        if (m_scheduler[pseudo_channel_id]->IsBscMatch(request_ba_index)) {
            CAM_INDEX request_cam_index = wr_cam_index;
            BSC_INDEX bsc_index = m_bankslice_manager[pseudo_channel_id]->GetBa2BscTable()->at(request_ba_index);
            auto bank_slice = m_bankslice_manager[pseudo_channel_id]->GetBankSliceMap()->at(bsc_index).get();

            bool is_page_hit = bank_slice->IsPageOpen() &&
                               (bank_slice->GetOpenPage() == wr_cam_entry->sdram_addr.row);
            wr_cam_entry->SetBaMatch(bsc_index, is_page_hit);

            if (is_page_hit || (!is_page_hit && (!bank_slice->IsActiving() || bank_slice->IsWrNttValid()))) {
                m_scheduler[pseudo_channel_id]->UpdateWrNttPip(
                    bsc_index,
                    request_ba_index,
                    UpdateType::NewCmdStore
                );
            }
        }

        next_trigger_delay = std::min(next_trigger_delay,
            m_scheduler[pseudo_channel_id]->GetNextUpdateTime() - sc_core::sc_time_stamp());
        ctrl_event.notify(next_trigger_delay);
    } else if (phase == UIF_WDAT_BEGIN || phase == DFI_RDAT_BEGIN)
    {

    }
    else
    {
        std::cerr << " Invalid Phase in the MrdimmController"<<std::endl;
        std::abort();
    }
}

// end_of_simulation
void
MrdimmController::end_of_simulation()
{
    ControllerFinishCheck();
}

// ControllerFinishCheck
void
MrdimmController::ControllerFinishCheck()
{
    unsigned num_pch = m_config.mem_spec->NumOfPseudoChannelsPerSubChannel;
    bool has_error = false;

    for (unsigned pch_id = 0; pch_id < num_pch; pch_id++) {
        std::cout << "--------------------------------- Pseudo Channel " << pch_id << " ---------------------------------" << std::endl;

        // 检查读CAM
        std::cout << "---------------------------------Rd Cam---------------------------------" << std::endl;
        std::vector<RdCamEntry*> rd_cam_entry_list;
        if (!m_scheduler[pch_id]->GetRdCam()->IsCamEmpty()) {
            for (auto& rd_cam_index : m_scheduler[pch_id]->GetRdCam()->GetUsedCamIndex()) {
                auto rd_cam_entry = m_scheduler[pch_id]->GetRdCam()->GetCamEntry(rd_cam_index);
                rd_cam_entry->print();
                rd_cam_entry_list.push_back(rd_cam_entry);
            }
        }

        // 检查写CAM
        std::cout << "---------------------------------Wr Cam---------------------------------" << std::endl;
        std::vector<WrCamEntry*> wr_cam_entry_list;
        if (!m_scheduler[pch_id]->GetWrCam()->IsCamEmpty()) {
            for (auto& wr_cam_index : m_scheduler[pch_id]->GetWrCam()->GetUsedCamIndex()) {
                auto wr_cam_entry = m_scheduler[pch_id]->GetWrCam()->GetCamEntry(wr_cam_index);
                wr_cam_entry->print();
                wr_cam_entry_list.push_back(wr_cam_entry);
            }
        }

        // 检查BankSlice
        std::cout << "---------------------------------Bank Slice---------------------------------" << std::endl;
        std::vector<BankSlice*> allocated_bsc_list;
        if (!m_bankslice_manager[pch_id]->IsAllocatedBscEmpty()) {
            for (auto& allocated_bsc : m_bankslice_manager[pch_id]->GetAllocatedBscSet()) {
                auto allocated_bank_slice = m_bankslice_manager[pch_id]->GetBankSliceMap()->at(allocated_bsc).get();
                allocated_bank_slice->print();
                allocated_bsc_list.push_back(allocated_bank_slice);
            }
        }

        // 检查错误
        if (!rd_cam_entry_list.empty() || !wr_cam_entry_list.empty() || !allocated_bsc_list.empty()) {
            has_error = true;
        }
    }

    if (has_error) {
        std::cerr << "ERROR: MrdimmController has unfinished transactions!" << std::endl;
    }
}

// bind_dfi_clock
void
MrdimmController::bind_dfi_clock(const sc_core::sc_clock& clk)
{
    dfi_clock(clk);
}

    } // namespace Controller
} // namespace dmu