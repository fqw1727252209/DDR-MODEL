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
    sc_core::sc_time ddr_clock_period;
    const sc_core::sc_time tCL;
    const sc_core::sc_time tCWL;
    tlm_utils::peq_with_cb_and_phase<MemoryDevice> payload_event_queue;
    void pipline_method(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);
};

    }
}

#endif