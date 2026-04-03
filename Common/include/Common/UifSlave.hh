#ifndef __UIF_SLAVE_HH__
#define __UIF_SLAVE_HH__

#include "Common/Common.hh"
#include "sysc/communication/sc_event_queue.h"
#include "sysc/kernel/sc_time.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <systemc>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <iostream>

#include "Common/CommonDefine.hh"
#include "Common/UifExtension.hh"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_phase.h"
#include "tlm_utils/peq_with_cb_and_phase.h"

namespace dmu {
namespace Port {

class UifSlave : public sc_core::sc_module
{
public:
    // 定义目标socket
    tlm_utils::simple_target_socket<UifSlave> target_socket;
    sc_core::sc_in<bool> dfi_clk;
    bool UIF_SLAVE_FLAG = true;
    sc_core::sc_event_queue pop_request;
    const sc_core::sc_time cycle;

    // 构造函数
    SC_HAS_PROCESS(UifSlave);
    UifSlave(sc_core::sc_module_name name, unsigned int data_width, sc_core::sc_time period)
        : sc_module(name)
        , target_socket("target_socket")
        , m_data_width(data_width)
        , peq_callback(this, &UifSlave::pipline_process)
        , dfi_clk("dfi_clk")
        , cycle(period)
    {
        // 注册非阻塞传输方法
        target_socket.register_nb_transport_fw(this, &UifSlave::nb_transport_fw);

        SC_METHOD(Send_Credit);
        sensitive << dfi_clk.neg();
        dont_initialize();
        SC_METHOD(Pop_Request);
        sensitive << pop_request;
        dont_initialize();

        credit_trans = new tlm::tlm_generic_payload();
        credit_extension = new UifSideBandExtension(uif_sideband_info);
        credit_trans->set_extension(credit_extension);
    }

    ~UifSlave()
    {
        delete credit_trans;
    }

    // 非阻塞传输前向方法
    tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans,
                                       tlm::tlm_phase& phase,
                                       sc_core::sc_time& delay)
    {
        // std::cout << "UIF Slave" << ": " << "call the nb_transport_fw() function" << std::endl;
        peq_callback.notify(trans, phase, delay);
        return tlm::TLM_ACCEPTED;
    }

    void Send_Credit()// send UIF credit to upstream
    {
        auto credit_ext = credit_trans->get_extension<UifSideBandExtension>();
        if(m_hpr_credit_send_upstream + hpr_queue.size() < hpr_queue_credit)
        {
            credit_ext->_uif_side_band_info.hpr_credit_valid = true;
            m_hpr_credit_send_upstream++;
        }
        else
        {
            credit_ext->_uif_side_band_info.hpr_credit_valid = false;
        }

        if(m_lpr_credit_send_upstream + lpr_queue.size() < lgpr_queue_credit)
        {
            credit_ext->_uif_side_band_info.lpr_credit_valid = true;
            m_lpr_credit_send_upstream++;
        }
        else
        {
            credit_ext->_uif_side_band_info.lpr_credit_valid = false;
        }

        if(m_tpw_credit_send_upstream + tpw_queue.size() < tpw_queue_credit)
        {
            credit_ext->_uif_side_band_info.tpw_credit_valid = true;
            m_tpw_credit_send_upstream++;
        }
        else {
            credit_ext->_uif_side_band_info.tpw_credit_valid = false;
        }
        
        if(credit_ext->_uif_side_band_info.hpr_credit_valid || credit_ext->_uif_side_band_info.lpr_credit_valid || credit_ext->_uif_side_band_info.tpw_credit_valid)
        {
            tlm::tlm_phase phase = UIF_CREDIT;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            DPRINT_INFO(UIF_SLAVE_FLAG, "UifSlave", "send credit to upstream");
            target_socket->nb_transport_bw(*credit_trans, phase, delay);
        }
    }

    void Pop_Request() // Pop request in round-robin maner
    {
        if(hpr_queue.size() > 0 || lpr_queue.size() > 0 || tpw_queue.size() > 0)
        {
            for(int i = 0; i < rr_size; i++)
            {
                unsigned index = (i+1 + arbit) % rr_size;
                if(index == 0)
                {
                    if(hpr_queue.size() > 0)
                    {
                        tlm::tlm_generic_payload* trans = hpr_queue.front();
                        hpr_queue.pop_front();
                        arbit = index;
                        peq_callback.notify(*trans, UIF_RDAT_BEGIN,15*cycle);
                        if(trans->get_data_length() > 32)
                        {
                            peq_callback.notify(*trans, UIF_RDAT_END,(15 + 1)*cycle);
                        }
                        else
                        {
                            peq_callback.notify(*trans, UIF_RDAT_END,15 *cycle);
                        }
                        break;
                    }
                    continue;
                }
                else if(index == 1)
                {
                    if(lpr_queue.size() > 0)
                    {
                        tlm::tlm_generic_payload* trans = lpr_queue.front();
                        lpr_queue.pop_front();
                        arbit = index;
                        peq_callback.notify(*trans, UIF_RDAT_BEGIN,15*cycle);
                        if(trans->get_data_length() > 32)
                        {
                            peq_callback.notify(*trans, UIF_RDAT_END,(15 + 1 )*cycle);
                        }
                        else
                        {
                            peq_callback.notify(*trans, UIF_RDAT_END,15*cycle);
                        }
                        break;
                    }
                    continue;
                }
                else if(index == 2)
                {
                    if(tpw_queue.size() > 0)
                    {
                        tlm::tlm_generic_payload* trans = tpw_queue.front();
                        tpw_queue.pop_front();
                        arbit = index;
                        peq_callback.notify(*trans, WR_RESPONSE_COMPLETE,15*cycle);
                        break;
                    }
                    continue;
                }
            }
        }
    }

    void pipline_process(tlm::tlm_generic_payload& payload, const tlm::tlm_phase& phase)
    {
        if (phase == UIF_REQ)
        {
            auto uif_extension = payload.get_extension<UifExtension>();
            PriorityClass priority_class = uif_extension->GetQosLevel();
            if(priority_class == PriorityClass::HPR)
            {
                DPRINT_INFO(UIF_SLAVE_FLAG, "UIF Slave", "Get request: HPR, addr: 0x%llx", payload.get_address());
                hpr_queue.push_back(&payload);
                m_hpr_credit_send_upstream--;
                pop_request.notify(10*cycle);
            }
            else if(priority_class == PriorityClass::LPR || priority_class == PriorityClass::GPR)
            {
                DPRINT_INFO(UIF_SLAVE_FLAG, "UIF Slave", "Get request: LPR or GPR, addr: 0x%llx", payload.get_address());
                lpr_queue.push_back(&payload);
                m_lpr_credit_send_upstream--;
                pop_request.notify(10*cycle);
            }
            else if(priority_class == PriorityClass::TPW || priority_class == PriorityClass::GPW)
            {
                DPRINT_INFO(UIF_SLAVE_FLAG, "UIF Slave", "Get request: TPW or GPW, addr: 0x%llx", payload.get_address());
                tpw_queue.push_back(&payload);
                m_tpw_credit_send_upstream--;
                auto uif_ext = payload.get_extension<UifExtension>();
                // uif_ext->SetWrDatRequestIndex();
                peq_callback.notify(payload, UIF_WDAT_REQ, cycle);
                pop_request.notify(10*cycle);
            }
            else {
                DPRINT_ERROR("UIF Slave", "UIF Slave get an unknown priority class");
            }
        }
        else if(phase == UIF_WDAT_REQ)
        {
            tlm::tlm_phase sending_phase = phase;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            target_socket->nb_transport_bw(payload, sending_phase, delay);
        }
        else if(phase == UIF_WDAT_BEGIN)
        {
            auto uif_extension = payload.get_extension<UifExtension>();
            DPRINT_INFO(UIF_SLAVE_FLAG, "UIF Slave", "Get write data request: %d", uif_extension->_uif_info.cmd_id);
        }
        else if(phase == UIF_WDAT_END)
        {
            auto uif_extension = payload.get_extension<UifExtension>();
            DPRINT_INFO(UIF_SLAVE_FLAG, "UIF Slave", "Get write data request: %d", uif_extension->_uif_info.cmd_id);
        }
        else if(phase == UIF_RDAT_BEGIN)
        {
            tlm::tlm_phase sending_phase = phase;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            target_socket->nb_transport_bw(payload, sending_phase, delay);
        }
        else if(phase == UIF_RDAT_END)
        {
            tlm::tlm_phase sending_phase = phase;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            target_socket->nb_transport_bw(payload, sending_phase, delay);
        }
        else if(phase == WR_RESPONSE_COMPLETE)
        {
            tlm::tlm_phase sending_phase = phase;
            sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
            target_socket->nb_transport_bw(payload, sending_phase, delay);
        }
        else
        {
            DPRINT_FATAL("UifSlave", "Invalid phase: %s", phase.get_name() );
        }
    }

private:
    unsigned int m_data_width;
    tlm_utils::peq_with_cb_and_phase<UifSlave> peq_callback;

    std::deque<tlm::tlm_generic_payload*> hpr_queue;
    std::deque<tlm::tlm_generic_payload*> lpr_queue;
    std::deque<tlm::tlm_generic_payload*> tpw_queue;

    int arbit{-1};
    const unsigned rr_size{3};

    unsigned m_hpr_credit_send_upstream{0};
    unsigned m_lpr_credit_send_upstream{0};
    unsigned m_tpw_credit_send_upstream{0};

    const unsigned rd_queue_credit{64};
    const unsigned hpr_queue_credit{32};
    const unsigned lgpr_queue_credit{rd_queue_credit - hpr_queue_credit};
    const unsigned tpw_queue_credit{64};

    tlm::tlm_generic_payload* credit_trans;
    UifSideBandExtension* credit_extension;
    //m_hpr_credit_send_upstream + hpr_queue.size()
    UifSideBandInfo uif_sideband_info;
    // 处理事务的核心方法
};

}} // namespace dmu::Port

#endif // UIF_SLAVE_H