#ifndef __PHY_DELAY_MODEL_HH__
#define __PHY_DELAY_MODEL_HH__

#include "Configure/Configure.hh"
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/peq_with_cb_and_phase.h>
#include "sysc/kernel/sc_module_name.h"
/* 模块功能描述：
使用PhyDelayModel模拟DDR的PHY的功能结构
1 与上游的接口
(1) PhyDelayModel的上游接Controller模块，通过tlm_utils::simple_target_socket<PhyDelayModel>, 并通过TLM接口与上游进行通信，对应于RTL的接口为DFI接口，
需要使用TLM extension机制实现对应的接口。
(2) 上游传输的payload 为tlm::tlm_generic_payload，可能一个周期会有多个payload挂携带在extension中下发，PhyDelayModel会使用异步FIFO的形式进行存储，并按照对应的顺序再添加对应的cmd delay 延迟后，
在DDR时钟域下通过IO接口下发到Device模块中，此处的功能可以类比于并串转化
(3) DFI接口会通过tlm::tlm_phase进行区分，区分CMD, RDAT, WDAT三个通道
(4) 模块里会有两个时钟域，一个时钟域为DFI时钟域，一个时钟域为DDR时钟域，RDAT数据返回时，类似于串并转化
2. 与下游的接口
PhyDelayModel的下游接Device模块，tlm_utils::simple_initiator_socket<PhyDelayModel>, 并通过TLM接口与下游进行通信，对应于RTL的接口为IO接口，需要使用TLM extension机制实现对应的接口。
3. 模块功能
(1) 模块接收来自Controller的payload，并添加延迟后，再通过IO接口下发给Device模块，
(2) payload根据TLM 接口的tlm_phase进行区分，CMD, RDAT, WDAT三个通道，三个通道对应的不同延迟m_phy_cmd_delay, m_phy_rdat_delay, m_phy_wdat_delay
(3) 根据配置的dfi ratio 确定时钟的频率比，同时也限定了DFI extension 在cmd phase时，一次性最多传输多少个payload

*/
/*
编码实现一条规则：
TLM接口需要不需要带延迟，其实TLM接口只是个function call，延迟的实现依旧还是在模块内部实现，即PhyDelayModel模块内部实现延迟，PhyDelayModel模块外部实现tlm接口
接口只实现传输，不携带延迟，除非该延迟是实现简单地路径打拍

*/

namespace dmu{
namespace Controller{
class PhyDelayModel: sc_core::sc_module{
    public:
        PhyDelayModel(const sc_core::sc_module_name& name,const Configure& config, const std::string& output_dir);
        ~PhyDelayModel();
        void dfi_pipeline_method(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);
        void io_pipeline_method(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);

        tlm_utils::simple_target_socket<PhyDelayModel> tSocket; // connect to controller
        tlm_utils::simple_initiator_socket<PhyDelayModel> iSocket; // connect to device

        tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans,
                                           tlm::tlm_phase& phase,
                                           sc_core::sc_time& delay);
        tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                           tlm::tlm_phase& phase,
                                           sc_core::sc_time& delay);

    private:
        const Configure& m_config;
        const sc_core::sc_time m_phy_cmd_delay;
        const sc_core::sc_time m_phy_rdat_delay;
        const sc_core::sc_time m_phy_wdat_delay;

        const sc_core::sc_time m_dfi_clock_period;  // get from iut module clock
        const sc_core::sc_time m_ddr_clock_period;  // this is created by the freq ratio and dfi_clock_period

        tlm_utils::peq_with_cb_and_phase<PhyDelayModel> dfi_payload_event_queue;
        tlm_utils::peq_with_cb_and_phase<PhyDelayModel> io_payload_event_queue;

};

}
}

#endif