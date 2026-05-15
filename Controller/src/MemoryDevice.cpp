#include "Common/Common.hh"
#include "Controller/common/CmdExtension.hh"
#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Controller/MemoryDevice.hh"
#include "Common/CommonDefine.hh"
#include "Controller/common/DfiExtension.hh"
#include "Common/StatisticExtension.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_time.h"
#include "sysc/utils/sc_report.h"
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
, ddr_clock_period(config.mem_spec->tCK)
, tCL(config.mem_spec->tCL)
, tCWL(config.mem_spec->tCWL)
, tSocket((std::string(name) + "_tSocket").c_str())
, payload_event_queue(this, &MemoryDevice::pipline_method)
{
    
    tSocket.register_nb_transport_fw(this, &MemoryDevice::nb_transport_fw);
}


tlm::tlm_sync_enum
MemoryDevice::nb_transport_fw(tlm::tlm_generic_payload &trans, tlm::tlm_phase &phase, sc_core::sc_time &delay)
{
    payload_event_queue.notify(trans,phase,delay);
    return tlm::TLM_ACCEPTED;
}
void
MemoryDevice::pipline_method(tlm::tlm_generic_payload &trans, const tlm::tlm_phase& phase)
{
    if(phase == CA_CMD){
        auto cmd_ext = trans.get_extension<CmdExtension>();
        auto trans_cmd = cmd_ext->GetCommand();
        if(trans_cmd == Command::ACT || trans_cmd == Command::PRE ||
           trans_cmd == Command::REFab || trans_cmd == Command::REFSb ||
           trans_cmd == Command::RFMab || trans_cmd == Command::RFMSb ||
           trans_cmd == Command::PREab || trans_cmd == Command::PRESb
        ){
            // do nothing
            
        }
        else if(trans_cmd == Command::RD || trans_cmd == Command::RDA){
            sc_core::sc_time delay = tCL;
            payload_event_queue.notify(trans,DQ_RDAT_BEGIN,delay);
        }
        else if(trans_cmd == Command::WR || trans_cmd == Command::WRA){
            // do nothing unless there is a real memory storage requirement
        }
        else{
            SC_REPORT_FATAL("MemoryDevice", "Invalid Command get in transaction");
        }
        cmd_ext->PopCommand(); // pop command from command queue
    }
    else if(phase == DQ_RDAT_BEGIN){
        sc_core::sc_time delay2 = _config.mem_spec->tBurst //- _config.mem_spec->tCK
        ;
        tlm::tlm_phase phase2 = DQ_RDAT_END;
        payload_event_queue.notify(trans,phase2,delay2);

        tlm::tlm_phase rdat_phase = phase;
        sc_core::sc_time rdat_delay = sc_core::SC_ZERO_TIME;
        tSocket->nb_transport_bw(trans, rdat_phase, rdat_delay);
    }
    else if(phase == DQ_RDAT_END){
        tlm::tlm_phase rdat_phase = phase;
        sc_core::sc_time rdat_delay = sc_core::SC_ZERO_TIME;
        tSocket->nb_transport_bw(trans, rdat_phase, rdat_delay);
    }
    else if(phase == DQ_WDAT_BEGIN){
        // do nothing unless there is a real memory storage requirement
    }
    else if(phase == DQ_WDAT_END){
        // do nothing unless there is a real memory storage requirement
        // do data write
        tlm::tlm_phase phase = WR_RESPONSE_COMPLETE;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        tSocket->nb_transport_bw(trans, phase, delay);
    }
    else{
        std::string ss = "Invalid phase get in pipeline method: " + std::string(phase.get_name());
        SC_REPORT_FATAL("MemoryDevice", ss.c_str());
    }
}



MemoryDevice::~MemoryDevice()
{
}



    }
}