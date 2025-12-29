#ifndef _CHI_PORT_H
#define _CHI_PORT_H

#include <systemc>
#include <tlm>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include <tlm_utils/simple_initiator_socket.h>

#include <ARM/TLM/arm_chi.h>

#include "CHIUtilities.h"
#include "MemoryManager.h"
#include "CHIPort/PortUtilities.h"
// #include "PortStruct.h"

#include <cstdint>
#include <optional>
#include <set>
#include <unordered_map>
#include <memory>

namespace dmu
{

struct DBField;
using P2C_INFO = DBField;
struct P2cFifo;
struct ResponseQueues;
struct RetryResourceManager;
struct RdataInfo;
struct WdataBufferArray;
struct DelayCommandQueue;
struct CMOResponseQueue;
struct ResourceManage;

class CHIPort : public sc_core::sc_module
{
protected:
    SC_HAS_PROCESS(CHIPort);

    // static const size_t MEMORY_SIZE = 0x10000; //4KB
    /* Backing store for memory. */
    // uint8_t mem_data[MEMORY_SIZE];//4KB

    CHIChannelState channels[CHI_NUM_CHANNELS];
    /*this the osstream to record all debug info
    */
    std::ostringstream oss;
    /* this is the basic data structure of CHI Port
    */

    // configure size should be parametered
    unsigned data_width_bytes;

    void clock_posedge();
    void clock_negedge();

    // void send_link_credit(ARM::CHI::Channel channel);
    // void send_flit(CHIChannelState& channel);

    /* Request Channel */
    std::deque<CHIFlit> rx_queue_s1;
    std::deque<CHIFlit> rx_queue_s2;
    bool grant_s1{false};
    bool grant_s2{false};
    bool grant_dcq_s1{false};
    bool grant_dcq_s2{false};

    void p2c_pop();
    void decision_req_stage();
    void decode_req_stage();
    void intf_req_stage();

    void handle_lcrdrtn_req(const CHIFlit& req_flit); // this not needed as 4 channels are do the same action, so use the lcrd field in the phase to show a credit return back
    bool handle_rdnosnp_req(const CHIFlit& req_flit);
    bool handle_wrnosnpptl_req(const CHIFlit& req_flit);
    bool handle_wrnosnpful_req(const CHIFlit& req_flit);
    // need to be solved
    void handle_pcrdrtn_req(const CHIFlit& req_flit);
    void handle_prftgt_req(const CHIFlit& req_flit);
    // DB Interface
    void Rdsent2Dramsys(const P2C_INFO& req_info);
    void Wrsent2Dramsys(const P2C_INFO& req_info);

    /*Response Channel */
    void gen_req_rsp(const CHIFlit& req_flit, const bool req_grant, const unsigned index);
    void gen_retry_rsp(const CHIFlit& req_flit);
    void gen_dcq_rsp(const CHIFlit& req_flit);
    void gen_pcrdgrant_rsp();
    bool rd_retry_enable;
    bool wr_retry_enable;

    // CHIFlit s0;
    void Rdsent2Dramsys(const CHIFlit& req_flit, const unsigned& rdatinfo_tag);

    void Wrsent2Dramsys(const CHIFlit& dat_flit);

    
    /*TLM Interface */
    tlm::tlm_sync_enum nb_transport_fw(ARM::CHI::Payload& payload, ARM::CHI::Phase& phase);
    tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& payload,
                                       tlm::tlm_phase& phase,
                                       sc_core::sc_time& bwDelay);
public:
    explicit CHIPort(const sc_core::sc_module_name& name, unsigned data_width_bits = 128);
    ~CHIPort();
    tlm_utils::peq_with_cb_and_phase<CHIPort> payloadEventQueue;
    tlm_utils::simple_initiator_socket<CHIPort> iSocket; // DB intf
    ARM::CHI::SimpleTargetSocket<CHIPort> target; //CHI intf
    sc_core::sc_in<bool> clock;

    P2cFifo* p2c_fifo;
    ResponseQueues* rsp_queue;
    ResourceManage* resource_manage_unit;
    RetryResourceManager* retry_resource_manager;
    WdataBufferArray* wdata_buffer_array;
    RdataInfo* rdata_info_queue;
    DelayCommandQueue* delay_command_queue;
    CMOResponseQueue* cmo_resp_queue;

private:
    void peqCallback(tlm::tlm_generic_payload& payload, const tlm::tlm_phase& phase);
    MemoryManager memoryManager;
    int SRC_ID{-1};

    std::optional<CHIFlit> rsp_flit_pending = {};
};

} // dmu namespace

#endif // _CHI_PORT_H
