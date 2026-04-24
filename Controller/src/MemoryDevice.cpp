#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Controller/MemoryDevice.hh"
#include "Common/CommonDefine.hh"
#include "Controller/common/DfiExtension.hh"
#include "Common/StatisticExtension.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_time.h"
#include "tlm_core/tlm_2/tlm_2_interfaces/tlm_fw_bw_ifs.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_phase.h"
#include <cassert>
#include <iomanip>
#include <sys/types.h>

namespace dmu{
    namespace Controller{


MemoryDevice::MemoryDevice(const sc_core::sc_module_name& name, const Configure& config, const std::string& output_dir)
: sc_core::sc_module(name)
, _config(config)
, output_dir(output_dir)
, phy_rdat_delay(config.controller_config->PHY_RDAT_DELAY * config.mem_spec->tCK)
, ddr_clock_period(config.mem_spec->tCK)
, tSocket((std::string(name) + "_tSocket").c_str())
, payload_event_queue(this, &MemoryDevice::pipline_method)
, outFile(output_dir+"/"+"TransInfo.txt")
, outFile_dfi_cmd(output_dir+"/"+"DfiCmd.txt")
{

    tSocket.register_nb_transport_fw(this, &MemoryDevice::nb_transport_fw);
    if (!outFile.is_open()) {
        outFile.open(output_dir+"/"+"TransInfo.txt", std::ios::out | std::ios::trunc);
    }
    if (!outFile_dfi_cmd.is_open()) {
        outFile_dfi_cmd.open(output_dir+"/"+"DfiCmd.txt", std::ios::out | std::ios::trunc);
    }

}

void
MemoryDevice::PrintDfiCmd(tlm::tlm_generic_payload& trans)
{
    outFile_dfi_cmd << std::left<<"Trans Id: " <<std::setw(8)<< trans.get_extension<StatisticExtension>()->GetTransactionId()
    << " Cmd: " << std::setw(6)<< trans.get_extension<DfiExtension>()->GetCommand().to_string()
    << " Time: " << std::setw(10)<< sc_core::sc_time_stamp().value() << " ps"
    << " Last Cmd Delay: " << std::setw(10)<<( (last_cmd_time == sc_core::SC_ZERO_TIME) ? 0 : (sc_core::sc_time_stamp() - last_cmd_time).value() ) << " ps"
    << " Real Bank Index: "<< std::setw(3)<< trans.get_extension<StatisticExtension>()->GetDecodedAddress().real_ba
    << " "<<trans.get_extension<StatisticExtension>()->GetDecodedAddress()
    << " "<<std::endl;
    last_cmd_time = sc_core::sc_time_stamp();
}


tlm::tlm_sync_enum
MemoryDevice::nb_transport_fw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay)
{
    // DPRINT_INFO(DEVICE, "MemoryDevice nb_transport_fw", "Trans Id: %d, Receive Cmd: %s, ",trans.get_extension<StatisticExtension>()->GetTransactionId(),trans.get_extension<DfiExtension>()->GetCommand().to_string().c_str());
    payload_event_queue.notify(trans,phase,delay);
    return tlm::TLM_ACCEPTED;
}
void
MemoryDevice::pipline_method(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase)
{
    if(phase == DFI_CMD)
    {
        auto dfi_ext = trans.get_extension<DfiExtension>();
        Command::Type dfi_cmd_type = (dfi_ext->GetCommand()).to_type();
        if(dfi_cmd_type == Command::ACT || dfi_cmd_type == Command::PRE)
        {
            PrintDfiCmd(trans);
        }
        else if(dfi_cmd_type == Command::RD || dfi_cmd_type == Command::RDA)
        {
            PrintDfiCmd(trans);
            sc_core::sc_time delay1 = _config.mem_spec->tCL;
            tlm::tlm_phase phase1 = DFI_RDAT_BEGIN;
            payload_event_queue.notify(trans,phase1,delay1);
        }
        else if(dfi_cmd_type == Command::WR || dfi_cmd_type == Command::WRA)
        {
            PrintDfiCmd(trans);
        }
        else if(dfi_cmd_type == Command::REFab || dfi_cmd_type == Command::REFsb || dfi_cmd_type == Command::RFMab || dfi_cmd_type == Command::RFMsb)
        {
            outFile_dfi_cmd << std::left<<"Trans Id: " <<std::setw(8)<< -1
            << " Cmd: " << std::setw(6)<< trans.get_extension<DfiExtension>()->GetCommand().to_string()
            << " Time: " << std::setw(10)<< sc_core::sc_time_stamp().value() << " ps"
            << " Last Cmd Delay: " << std::setw(10)<<( (last_cmd_time == sc_core::SC_ZERO_TIME) ? 0 : (sc_core::sc_time_stamp() - last_cmd_time).value() ) << " ps"
            << " Real Rank Index: "<< std::setw(3)<< trans.get_extension<DfiExtension>()->GetAddress().real_cid
            << " "<<trans.get_extension<DfiExtension>()->GetAddress()
            << " "<<std::endl;
            last_cmd_time = sc_core::sc_time_stamp();
        }
        dfi_ext->PopCommand();
        //if cmd is RD or RDA, will call a function nb_transport_bw
        // Phase is DFI_RDAT_BEGIN with delay is tCL;
        // Phase is DFI_RDAT_END with delay is tCL + tBurstLenth
    }
    else if(phase == DFI_RDAT_BEGIN)
    {

        // DPRINT_INFO(DEVICE, "MemoryDevice", "Trans Id: %d, Rdata Begin",trans.get_extension<StatisticExtension>()->GetTransactionId());
        trans.get_extension<StatisticExtension>()->RecordDfiDataBeginTime(sc_core::sc_time_stamp());
        sc_core::sc_time delay2 = _config.mem_spec->tBurst - _config.mem_spec->tCK;
        tlm::tlm_phase phase2 = DFI_RDAT_END;
        payload_event_queue.notify(trans,phase2,delay2);
        tlm::tlm_phase rdat_phase = phase;
        sc_core::sc_time rdat_delay = phy_rdat_delay;
        tSocket->nb_transport_bw(trans, rdat_phase, rdat_delay);
    }
    else if(phase == DFI_RDAT_END)
    {
        record_last_trans_time = sc_core::sc_time_stamp() + _config.mem_spec->tCK;
        trans.get_extension<StatisticExtension>()->RecordDfiDataEndTime(sc_core::sc_time_stamp() + _config.mem_spec->tCK);
        tlm::tlm_phase rdat_phase = phase;
        sc_core::sc_time rdat_delay = phy_rdat_delay;
        tSocket->nb_transport_bw(trans, rdat_phase, rdat_delay);
        // DPRINT_INFO(DEVICE, "MemoryDevice", "Trans Id: %d, Rdata End",trans.get_extension<StatisticExtension>()->GetTransactionId());
        trans.get_extension<StatisticExtension>()->PrintStatistics(outFile);
    }
    else if(phase == DFI_WDAT_BEGIN)
    {
        trans.get_extension<StatisticExtension>()->RecordDfiDataBeginTime(sc_core::sc_time_stamp());
        // DPRINT_INFO(false,"MemoryDevice", "Wdata Begin");
    }
    else if(phase == DFI_WDAT_END)
    {
        trans.get_extension<StatisticExtension>()->RecordDfiDataEndTime(sc_core::sc_time_stamp() + _config.mem_spec->tCK);
        // DPRINT_INFO(false,"MemoryDevice", "Wdata End");
        trans.get_extension<StatisticExtension>()->PrintStatistics(outFile);
    }
    else
    {
        DPRINT_FATAL("MemoryDevice", "Invalid phase");
    }
}


MemoryDevice::~MemoryDevice()
{
    if(outFile.is_open())
    {
        outFile.close();
    }
    if(outFile_dfi_cmd.is_open())
    {
        outFile_dfi_cmd.close();
    }
}


    }
}