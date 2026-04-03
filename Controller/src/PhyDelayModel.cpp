// #include "Controller/PhyDelayModel.hh"
// #include "Controller/common/ControllerCommon.hh"
// #include "Controller/common/DfiExtension.hh"
// #include "Common/StatisticExtension.hh"
// #include "sysc/kernel/sc_simcontext.h"
// #include "sysc/kernel/sc_time.h"
// #include "tlm_core/tlm_2/tlm_2_interfaces/tlm_fw_bw_ifs.h"
// #include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
// #include "tlm_core/tlm_2/tlm_generic_payload/tlm_phase.h"
// #include <cassert>

// namespace dmu{

// PhyDelayModel::PhyDelayModel(const sc_core::sc_module_name& name, const Configure& config)
// : sc_core::sc_module(name)
// , config(config)
// , phy_cmd_delay(config.mc_config.phy_cmd_delay, sc_core::SC_NS)
// , phy_rdat_delay(config.mc_config.phy_rdat_delay, sc_core::SC_NS)
// , phy_wdat_delay(config.mc_config.phy_wdat_delay, sc_core::SC_NS)
// , current_clock_period(config.mem_spec->tCK) // 使用DDR时钟周期
// , tSocket("tSocket")
// , iSocket("iSocket")
// , payload_event_queue(this, &PhyDelayModel::pipeline_method)
// {
// tSocket.register_nb_transport_fw(this, &PhyDelayModel::nb_transport_fw);
// iSocket.register_nb_transport_bw(this, &PhyDelayModel::nb_transport_bw);
// }

// tlm::tlm_sync_enum
// PhyDelayModel::nb_transport_fw(tlm::tlm_generic_payload& trans,
//                               tlm::tlm_phase& phase,
//                               sc_core::sc_time& delay)
// {
// DPRINT_INFO(PHY_DELAY_MODEL, name(), "nb_transport_fw() function has been called with phase: %s",
//             phase.get_name());
//
// // 将请求转发到事件队列进行处理
// payload_event_queue.notify(trans, phase, delay);
//
// return tlm::TLM_ACCEPTED;
// }

// tlm::tlm_sync_enum
// PhyDelayModel::nb_transport_bw(tlm::tlm_generic_payload& trans,
//                               tlm::tlm_phase& phase,
//                               sc_core::sc_time& delay)
// {
// DPRINT_INFO(PHY_DELAY_MODEL, name(), "nb_transport_bw() function has been called with phase: %s",
//             phase.get_name());
//
// // 根据不同的相位添加相应的PHY延迟
// if (phase == DFI_CMD) {
// delay += phy_cmd_delay;
// } else if (phase == DFI_RDAT_BEGIN || phase == DFI_RDAT_END) {
// delay += phy_rdat_delay;
// } else if (phase == DFI_WDAT_BEGIN || phase == DFI_WDAT_END) {
// delay += phy_wdat_delay;
// }
//
// // 时钟对齐处理
// delay = AlignAtNext(delay, current_clock_period);
//
// // 转发到前级模块
// tlm::tlm_phase bw_phase = phase;
// return tSocket->nb_transport_bw(trans, bw_phase, delay);
// }

// void
// PhyDelayModel::pipeline_method(tlm::tlm_generic_payload& trans,
//                               const tlm::tlm_phase& phase)
// {
// DPRINT_INFO(PHY_DELAY_MODEL, name(), "pipeline_method() function has been called with phase: %s",
//             phase.get_name());
//
// // 根据相位处理不同的PHY延迟和时钟对齐
// sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
//
// if (phase == DFI_CMD) {
// // 命令相位，添加PHY命令延迟并进行时钟对齐
// delay = phy_cmd_delay;
// delay = AlignAtNext(delay, current_clock_period);
// tlm::tlm_phase next_phase = phase;
// iSocket->nb_transport_fw(trans, next_phase, delay);
// }
// else if (phase == DFI_RDAT_BEGIN) {
// // 读数据开始相位，添加PHY读数据延迟并进行时钟对齐
// delay = phy_rdat_delay;
// delay = AlignAtNext(delay, current_clock_period);
// tlm::tlm_phase next_phase = phase;
// iSocket->nb_transport_bw(trans, next_phase, delay);
// }
// else if (phase == DFI_RDAT_END) {
// // 读数据结束相位，添加PHY读数据延迟并进行时钟对齐
// delay = phy_rdat_delay;
// delay = AlignAtNext(delay, current_clock_period);
// tlm::tlm_phase next_phase = phase;
// iSocket->nb_transport_bw(trans, next_phase, delay);
// }
// else if (phase == DFI_WDAT_BEGIN) {
// // 写数据开始相位，添加PHY写数据延迟并进行时钟对齐
// delay = phy_wdat_delay;
// delay = AlignAtNext(delay, current_clock_period);
// tlm::tlm_phase next_phase = phase;
// iSocket->nb_transport_fw(trans, next_phase, delay);
// }
// else if (phase == DFI_WDAT_END) {
// // 写数据结束相位，添加PHY写数据延迟并进行时钟对齐
// delay = phy_wdat_delay;
// delay = AlignAtNext(delay, current_clock_period);
// tlm::tlm_phase next_phase = phase;
// iSocket->nb_transport_fw(trans, next_phase, delay);
// }
// else {
// // 对于其他相位，直接转发，但仍需时钟对齐
// delay = AlignAtNext(delay, current_clock_period);
// tlm::tlm_phase next_phase = phase;
// iSocket->nb_transport_fw(trans, next_phase, delay);
// }
// }

// } // namespace dmu