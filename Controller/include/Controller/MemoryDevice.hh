#ifndef __MEMORY_DEVICE_HH__
#define __MEMORY_DEVICE_HH__

#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_module_name.h"
#include <cstddef>
#include <memory>

#include <string>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include "sysc/kernel/sc_time.h"
#include "tlm_core/tlm_2/tlm_2_interfaces/tlm_fw_bw_ifs.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <tlm_utils/peq_with_cb_and_phase.h>

#include "Configure/Configure.hh"
namespace dmu
{
    namespace Controller
    {

class MemoryDevice : public sc_core::sc_module
{
public:

    MemoryDevice(const sc_core::sc_module_name& name, const Configure& config, const std::string& output_dir);
    SC_HAS_PROCESS(MemoryDevice);

    const Configure& _config;

    virtual tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans, tlm::tlm_phase& phase, sc_core::sc_time& delay);

    tlm_utils::simple_target_socket<MemoryDevice> tSocket;

    void PrintDfiCmd(tlm::tlm_generic_payload& trans);

    MemoryDevice(const MemoryDevice&) = delete;
    MemoryDevice(MemoryDevice&&) = delete;
    MemoryDevice& operator=(const MemoryDevice&) = delete;
    MemoryDevice& operator=(MemoryDevice&&) = delete;

    ~MemoryDevice() override;

private:
    const std::string output_dir;
    sc_core::sc_time ddr_clock_period;
    tlm_utils::peq_with_cb_and_phase<MemoryDevice> payload_event_queue;
    void pipline_method(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);

    sc_core::sc_time last_cmd_time{sc_core::SC_ZERO_TIME};

    const sc_core::sc_time phy_rdat_delay{sc_core::sc_time(62.4, sc_core::SC_NS)};
    std::ofstream outFile;
    std::ofstream outFile_dfi_cmd;

    sc_core::sc_time record_last_trans_time{sc_core::SC_ZERO_TIME};
};

    }
}

#endif