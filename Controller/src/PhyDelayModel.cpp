#include "Controller/PhyDelayModel.hh"
#include "Common/Common.hh"
#include "Common/CommonDefine.hh"
#include "Common/Logger.hh"
#include "Common/StatisticExtension.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Controller/common/DfiExtension.hh"
#include "Controller/common/CmdExtension.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_time.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <cassert>
#include <cstdint>
#include <sstream>
#include <string>
// #include "Common/StatisticExtension.hh"
// #include "sysc/kernel/sc_simcontext.h"
// #include "sysc/kernel/sc_time.h"
// #include "tlm_core/tlm_2/tlm_2_interfaces/tlm_fw_bw_ifs.h"
// #include "tlm_core/tlm_2/tlm_generic_payload/tlm_phase.h"
// #include <cassert>

namespace dmu{
    namespace Controller{

PhyDelayModel::PhyDelayModel(const sc_core::sc_module_name& name,const Configure& config, const std::string& output_dir)
    : sc_core::sc_module(name)
    , m_config(config)
    , tSocket((std::string(name) + "tSocket").c_str())
    , iSocket((std::string(name)+"iSocket").c_str())
    , m_ddr_clock_period(m_config.mem_spec->tCK)
    , m_dfi_clock_period(m_config.mem_spec->tCK_mc)
    , m_phy_cmd_delay(m_config.controller_config->PHY_CMD_DELAY * m_config.mem_spec->tCK_mc)
    , m_phy_rdat_delay(m_config.controller_config->PHY_RDAT_DELAY * m_config.mem_spec->tCK_mc)
    , m_phy_wdat_delay(m_config.controller_config->PHY_WDAT_DELAY * m_config.mem_spec->tCK_mc)
    , dfi_payload_event_queue(this, &PhyDelayModel::dfi_pipeline_method)
    , io_payload_event_queue(this, &PhyDelayModel::io_pipeline_method)
{
    tSocket.register_nb_transport_fw(this, &PhyDelayModel::nb_transport_fw);
    iSocket.register_nb_transport_bw(this, &PhyDelayModel::nb_transport_bw);

    Logger::getInstance(std::string(name)+"_DfiCmd").initialize(std::string(name)+"_DfiCmd",output_dir,std::string(name)+"_DfiCmd");
    Logger::getInstance(std::string(name)+"_DfiRdat").initialize(std::string(name)+"_DfiRdat",output_dir,std::string(name)+"_DfiRdat");
    Logger::getInstance(std::string(name)+"_DfiWdat").initialize(std::string(name)+"_DfiWdat",output_dir,std::string(name)+"_DfiWdat");
    Logger::getInstance(std::string(name)+"_CaCmd").initialize(std::string(name)+"_CaCmd",output_dir,std::string(name)+"_CaCmd");
    Logger::getInstance(std::string(name)+"_DqRdat").initialize(std::string(name)+"_DqRdat",output_dir,std::string(name)+"_DqRdat");
    Logger::getInstance(std::string(name)+"_DqWdat").initialize(std::string(name)+"_DqWdat",output_dir,std::string(name)+"_DqWdat");
}

PhyDelayModel::~PhyDelayModel()
{
}

tlm::tlm_sync_enum
PhyDelayModel::nb_transport_fw(tlm::tlm_generic_payload& trans,
                               tlm::tlm_phase& phase,
                               sc_core::sc_time& delay)
{
    // 将请求转发到事件队列进行处理
    dfi_payload_event_queue.notify(trans, phase, delay);
    return tlm::TLM_ACCEPTED;
}

tlm::tlm_sync_enum
PhyDelayModel::nb_transport_bw(tlm::tlm_generic_payload& trans,
                               tlm::tlm_phase& phase,
                               sc_core::sc_time& delay)
{
    if(phase != WR_RESPONSE_COMPLETE){
        if(phase == DFI_CMD){
            dfi_pipeline_method(trans,phase);
        }
        else{
            io_payload_event_queue.notify(trans, phase, delay);
        }
    }
    else{
        // 对于 WR_RESPONSE_COMPLETE, 只是起到对于写事务的结束周期提示，交由MemoryManager提示写事务已经完成
        // 此处还需要考虑，在MemoryController模块处考虑： RMW 事务的结束，返回的时候并不是对应的事务完成或者结束。
        tlm::tlm_phase next_phase = phase;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        tSocket->nb_transport_bw(trans, next_phase, delay);
    }

    return tlm::TLM_ACCEPTED;
}

void
PhyDelayModel::dfi_pipeline_method(tlm::tlm_generic_payload& trans,
                                   const tlm::tlm_phase& phase)
{
    // 根据相位处理不同的PHY延迟和时钟对齐
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;

    if (phase == DFI_CMD) {
        // 命令相位，添加PHY命令延迟并进行时钟对齐
        delay = m_phy_cmd_delay;
        tlm::tlm_phase next_phase = CA_CMD;
        auto dfi_cmd_extension = trans.get_extension<DfiCmdExtension>();
        assert(dfi_cmd_extension != nullptr && "the DFI extension is nullptr");
        assert(dfi_cmd_extension->HasCommand() && "Has No Cmd in the DfiCmdExtension");
        while(dfi_cmd_extension->HasNextCommand()){
            tlm::tlm_generic_payload* cmd_trans = dfi_cmd_extension->GetNextCommand();
            DPRINT_INFO(TOP_DEBUG,"Phy Delay Model","trans ptr:%p ",static_cast<void*>(cmd_trans));
            auto cmd_ext = cmd_trans->get_extension<CmdExtension>();
            auto stat_ext = cmd_trans->get_extension<StatisticExtension>();
            auto cmd_type = cmd_ext->GetCommand().type();
            std::ostringstream oss;
            if(cmd_type == Command::REFab || cmd_type == Command::REFsb || cmd_type == Command::RFMab || cmd_type == Command::RFMsb)
            {
                oss << std::left << "Trans Id: " << std::setw(8) << "-1"
                    << " Cmd: " << std::setw(6) << cmd_ext->GetCommand().to_string()
                    << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
                    << " Real Rank Index: "<< std::setw(3)<< cmd_ext->GetAddress().real_cid
                    << " " << cmd_ext->GetAddress()
                ;
            }
            else if(cmd_type == Command::ACT || cmd_type == Command::PRE)
            {
                oss << std::left << "Trans Id: " << std::setw(8) << (stat_ext == nullptr ? -2 : stat_ext->GetTransactionId())
                    << " Cmd: " << std::setw(6) << cmd_ext->GetCommand().to_string()
                    << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
                    << " Real Bank Index: "<< std::setw(3)<< cmd_ext->GetAddress().real_ba
                    << " " << cmd_ext->GetAddress()
                ;
            }
            else if(cmd_type == Command::RD || cmd_type == Command::RDA
                 || cmd_type == Command::WR || cmd_type == Command::WRA)
            {
                oss << std::left << "Trans Id: " << std::setw(8) << (stat_ext->GetTransactionId())
                    << " Cmd: " << std::setw(6) << cmd_ext->GetCommand().to_string()
                    << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
                    << " Real Bank Index: "<< std::setw(3)<< cmd_ext->GetAddress().real_ba
                    << " " << cmd_ext->GetAddress()
                    << " Col: " << stat_ext->GetDecodedAddress().column
                ;
            }
            else{}

            DMU_LOG_INFO_N(std::string(name())+"_DfiCmd",oss.str().c_str());
            io_payload_event_queue.notify(*cmd_trans, next_phase, delay);
        }
        dfi_cmd_extension->reset();
    }
    else if (phase == DFI_RDAT_BEGIN || phase == DFI_RDAT_END) {
        // 读数据开始相位，添加PHY读数据延迟并进行时钟对齐
        std::ostringstream oss;
        auto cmd_trans = reinterpret_cast<tlm::tlm_generic_payload*>(&trans);
        auto cmd_ext = trans.get_extension<CmdExtension>();
        auto stat_ext = cmd_trans->get_extension<StatisticExtension>();
        oss << std::left << "Trans Id: " << std::setw(8) << stat_ext->GetTransactionId()
            << " Phase: " << std::setw(15) << std::string(phase.get_name())
            << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
            << " Real Bank Index: "<< std::setw(3)<< cmd_ext->GetAddress().real_ba
            << " " << cmd_ext->GetAddress()
            ;
        delay = AlignAtNext(sc_core::sc_time_stamp(), m_dfi_clock_period) - sc_core::sc_time_stamp();
        if(phase == DFI_RDAT_BEGIN)
            stat_ext->RecordDfiDataBeginTime(sc_core::sc_time_stamp()+delay);
        if(phase == DFI_RDAT_END)
            stat_ext->RecordDfiDataEndTime(sc_core::sc_time_stamp()+delay);
        DMU_LOG_INFO_N(std::string(name())+"_DfiRdat",oss.str().c_str());
        tlm::tlm_phase next_phase = phase;
        tSocket->nb_transport_bw(trans, next_phase, delay);
    }
    else if (phase == DFI_WDAT_BEGIN) {
        // 写数据开始相位，添加PHY写数据延迟并进行时钟对齐
        std::ostringstream oss;
        auto cmd_trans = reinterpret_cast<tlm::tlm_generic_payload*>(&trans);
        auto cmd_ext = trans.get_extension<CmdExtension>();
        auto stat_ext = cmd_trans->get_extension<StatisticExtension>();
        oss << std::left << "Trans Id: " << std::setw(8) << stat_ext->GetTransactionId()
            << " Phase: " << std::setw(15) << std::string(phase.get_name())
            << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
            << " Real Bank Index: "<< std::setw(3)<< cmd_ext->GetAddress().real_ba
            << " " << cmd_ext->GetAddress()
            ;
        DMU_LOG_INFO_N(std::string(name())+"_DfiWdat",oss.str().c_str());
        stat_ext->RecordDfiDataBeginTime(sc_core::sc_time_stamp());
        delay = m_phy_wdat_delay;
        // delay = AlignAtNext(delay, current_clock_period);
        tlm::tlm_phase next_phase = DQ_WDAT_BEGIN;
        io_payload_event_queue.notify(trans, next_phase, delay);
    }
    else if (phase == DFI_WDAT_END) {
        // 写数据结束相位，添加PHY写数据延迟并进行时钟对齐
        std::ostringstream oss;
        auto cmd_trans = reinterpret_cast<tlm::tlm_generic_payload*>(&trans);
        auto cmd_ext = trans.get_extension<CmdExtension>();
        auto stat_ext = cmd_trans->get_extension<StatisticExtension>();
        oss << std::left << "Trans Id: " << std::setw(8) << stat_ext->GetTransactionId()
            << " Phase: " << std::setw(15) << std::string(phase.get_name())
            << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
            << " Real Bank Index: "<< std::setw(3)<< cmd_ext->GetAddress().real_ba
            << " " << cmd_ext->GetAddress()
            ;
        DMU_LOG_INFO_N(std::string(name())+"_DfiWdat",oss.str().c_str());
        stat_ext->RecordDfiDataEndTime(sc_core::sc_time_stamp());
        delay = m_phy_wdat_delay;
        // delay = AlignAtNext(delay, current_clock_period);
        tlm::tlm_phase next_phase = DQ_WDAT_END;
        io_payload_event_queue.notify(trans, next_phase, delay);
    }
    else {
        DPRINT_FATAL(name(), "Invalid phase %s",phase.get_name());
    }
}

void
PhyDelayModel::io_pipeline_method(tlm::tlm_generic_payload& trans,
                                  const tlm::tlm_phase& phase)
{
    if(phase == CA_CMD){
        std::ostringstream oss;
        auto cmd_trans = reinterpret_cast<tlm::tlm_generic_payload*>(&trans);
        auto cmd_ext = trans.get_extension<CmdExtension>();
        auto stat_ext = cmd_trans->get_extension<StatisticExtension>();
        oss << std::left << "Trans Id: " << std::setw(8) << (stat_ext == nullptr ? -2 : stat_ext->GetTransactionId())
            << " Cmd: " << std::setw(6) << cmd_ext->GetCommand().to_string()
            << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
            << " Real Bank Index: "<< std::setw(3)<< cmd_ext->GetAddress().real_ba
            << " " << cmd_ext->GetAddress()
            ;
        DMU_LOG_INFO_N(std::string(name())+"_CaCmd",oss.str().c_str());
        tlm::tlm_phase next_phase = phase;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        iSocket->nb_transport_fw(trans, next_phase, delay);
    }
    else if(phase == DQ_WDAT_BEGIN || phase == DQ_WDAT_END){
        std::ostringstream oss;
        auto cmd_trans = reinterpret_cast<tlm::tlm_generic_payload*>(&trans);
        auto cmd_ext = trans.get_extension<CmdExtension>();
        auto stat_ext = cmd_trans->get_extension<StatisticExtension>();
        oss << std::left << "Trans Id: " << std::setw(8) << stat_ext->GetTransactionId()
            << " Phase: " << std::setw(15) << std::string(phase.get_name())
            << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
            << " Real Bank Index: "<< std::setw(3)<< cmd_ext->GetAddress().real_ba
            << " " << cmd_ext->GetAddress()
            ;
        if(phase == DQ_WDAT_BEGIN)
            stat_ext->RecordDqDataBeginTime(sc_core::sc_time_stamp());
        if(phase == DQ_WDAT_END)
            stat_ext->RecordDqDataEndTime(sc_core::sc_time_stamp());
        DMU_LOG_INFO_N(std::string(name())+"_DqWdat",oss.str().c_str());
        tlm::tlm_phase next_phase = phase;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        iSocket->nb_transport_fw(trans, next_phase, delay);
    }
    else if(phase == DQ_RDAT_BEGIN){
        std::ostringstream oss;
        auto cmd_trans = reinterpret_cast<tlm::tlm_generic_payload*>(&trans);
        auto cmd_ext = trans.get_extension<CmdExtension>();
        auto stat_ext = trans.get_extension<StatisticExtension>();
        oss << std::left << "Trans Id: " << std::setw(8) << stat_ext->GetTransactionId()
            << " Phase: " << std::setw(15) << std::string(phase.get_name())
            << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
            << " Real Bank Index: "<< std::setw(3)<< stat_ext->GetDecodedAddress().real_ba
            << " " << stat_ext->GetDecodedAddress()
            ;
        stat_ext->RecordDqDataBeginTime(sc_core::sc_time_stamp());
        DMU_LOG_INFO_N(std::string(name())+"_DqRdat",oss.str().c_str());

        tlm::tlm_phase next_phase = DFI_RDAT_BEGIN;
        sc_core::sc_time delay = m_phy_rdat_delay;
        dfi_payload_event_queue.notify(trans, next_phase, delay);
    }
    else if (phase == DQ_RDAT_END) {
        std::ostringstream oss;
        auto cmd_trans = reinterpret_cast<tlm::tlm_generic_payload*>(&trans);
        auto cmd_ext = trans.get_extension<CmdExtension>();
        auto stat_ext = trans.get_extension<StatisticExtension>();
        oss << std::left << "Trans Id: " << std::setw(8) << stat_ext->GetTransactionId()
            << " Phase: " << std::setw(15) << std::string(phase.get_name())
            << " Time: " << std::setw(10) << sc_core::sc_time_stamp().value() << " ps"
            << " Real Bank Index: "<< std::setw(3)<< stat_ext->GetDecodedAddress().real_ba
            << " " << stat_ext->GetDecodedAddress()
            ;
        DMU_LOG_INFO_N(std::string(name())+"_DqRdat",oss.str().c_str());
        stat_ext->RecordDqDataEndTime(sc_core::sc_time_stamp());
        tlm::tlm_phase next_phase = DFI_RDAT_END;
        sc_core::sc_time delay = m_phy_rdat_delay;
        dfi_payload_event_queue.notify(trans, next_phase, delay);
    }
    else{
        DPRINT_FATAL(name(), "Invalid phase %s",phase.get_name());
    }
}

// namespace Controller
// namespace dmu