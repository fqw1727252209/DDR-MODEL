// # ifndef __PHY_DELAY_MODEL_HH__
// # define __PHY_DELAY_MODEL_HH__

// #include "Configure/Configure.hh"
// #include <systemc>
// #include <tlm>
// #include <tlm_utils/simple_target_socket.h>
// #include <tlm_utils/simple_initiator_socket.h>
// #include <tlm_utils/peq_with_cb_and_phase.h>

// namespace dmu{

// class PhyDelayModel: public sc_core::sc_module
// {
// public:
//     PhyDelayModel(const sc_core::sc_module_name& name, const Configure& config);

//     ~PhyDelayModel()= default;
//     tlm_utils::simple_target_socket<PhyDelayModel> tSocket;
//     tlm_utils::simple_initiator_socket<PhyDelayModel> iSocket;

//     tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans,
//                                        tlm::tlm_phase& phase,
//                                        sc_core::sc_time& delay);

//     tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
//                                        tlm::tlm_phase& phase,
//                                        sc_core::sc_time& delay);

// private:
//     void pipeline_method(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);

//     const Configure& config;

//     // PHY延迟参数
//     const sc_core::sc_time phy_cmd_delay;
//     const sc_core::sc_time phy_rdat_delay;
//     const sc_core::sc_time phy_wdat_delay;

//     // 当前时钟信息
//     const sc_core::sc_time current_clock_period;

//     // 事件队列用于处理延迟
//     tlm_utils::peq_with_cb_and_phase<PhyDelayModel> payload_event_queue;
// };



// }



// # endif