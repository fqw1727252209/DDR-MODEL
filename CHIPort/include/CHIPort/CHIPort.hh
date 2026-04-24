#ifndef __CHI_PORT_HH__
#define __CHI_PORT_HH__

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>

#include <ARM/TLM/arm_chi.h>

#include "CHIPort/CHILink.hh"
#include "CHIPort/CHIUtilities.h"
#include "CHIPort/P2cFifo.hh"
#include "CHIPort/PortMemoryManager.hh"
#include "CHIPort/RdDataInfo.hh"
#include "CHIPort/ResponseQueues.hh"
#include "CHIPort/RetryResourceManager.hh"
#include "CHIPort/WdataBufferArray.hh"
#include "Common/Common.hh"
#include "Configure/Configure.hh"

#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>
#include <memory>

namespace dmu{
    namespace Port{

class CHIPort : public sc_core::sc_module
{
protected:
    SC_HAS_PROCESS(CHIPort);

    CHILink channels[CHI_NUM_CHANNELS];
    // CHILinkReceive ReqLink, WdatLink;
    // CHILinkTransmit RespLink, RdatLink;

    unsigned data_width_bytes;
    const sc_core::sc_time clock_period;

    void dfi_clock_posedge();
    void noc_clock_posedge();
    void noc_clock_negedge();

    /*Request Channel*/
    std::deque<CHIFlit> req_s1;
    std::deque<CHIFlit> req_s2;
    void req_decode_s1();
    void req_decision_s2();
    void req_pop_s3();

    // bool handle_Request(const CHIFlit& flit);
    bool handle_WriteNoSnp(const CHIFlit& flit, PriorityClass qos_level);
    bool handle_ReadNoSnp(const CHIFlit& flit, PriorityClass qos_level);
    bool handle_CleanShardPersist(const CHIFlit& flit);
    bool handle_PrefetchTgt(const CHIFlit& flit);

    /*Response Channel*/
    void resp_arbit_s1();

    void resp_gen_pcrd();

    /*Write Data Channel*/
    void wdat_decode_s1();
    void wdat_push_s2();
    std::deque<CHIFlit> wdat_s1;

    /*Read Data Channel*/
    void rdat_arbit_s1();

    /*TLM interface*/
    // upstream call
    tlm::tlm_sync_enum nb_transport_fw(ARM::CHI::Payload& payload, ARM::CHI::Phase& phase);
    // downstream call
    tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& payload,
                                       tlm::tlm_phase& phase,
                                       sc_core::sc_time& bwDelay);
    //UIF Interface
    void SendUifRequest(const QueueEntry& entry, unsigned cmd_id, bool is_rd);

public:
    explicit CHIPort(const sc_core::sc_module_name& name, const Configure& configure, unsigned data_width_bits, const sc_core::sc_time& clock_period);
    ~CHIPort() = default;
    tlm_utils::peq_with_cb_and_phase<CHIPort> payloadEventQueue;
    tlm_utils::simple_initiator_socket<CHIPort> iSocket; //DB intf
    ARM::CHI::SimpleTargetSocket<CHIPort> target; // CHI intf

    sc_core::sc_in<bool> noc_clock;
    sc_core::sc_in<bool> dfi_clock;

public:

private:
    void peqCallback(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);
    PortMemoryManager memoryManager;

    std::unique_ptr<WdataBufferArray> wdataBufferArray;
    std::unique_ptr<RetryResourceManager> retryResourceManager;
    std::unique_ptr<RdDataInfo> rdDataInfo;
    std::unique_ptr<P2cFifo> p2cFifo;
    std::unique_ptr<ResponseQueues> responseQueues;

    const Configure& _configure;

    int src_id;
};


    } // namespace Port
} // namespace dmu

#endif  