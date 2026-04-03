#include "CHIPort/CHIPort.hh"
#include "ARM/TLM/arm_chi_phase.h"
#include "CHIPort/CHIUtilities.h"
#include "CHIPort/P2cFifo.hh"
#include "CHIPort/PortCommon.hh"
#include "CHIPort/RetryResourceManager.hh"
#include "CHIPort/WdataBufferArray.hh"
#include "CHIPort/PortCommon.hh"
#include "Common/Common.hh"
#include "Common/CommonDefine.hh"
#include "Common/StatisticExtension.hh"
#include "Common/UifExtension.hh"
#include "Configure/Configure.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/kernel/sc_time.h"
#include "sysc/utils/sc_report.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_phase.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace dmu{
    namespace Port{

static ARM::CHI::Phase
make_read_data_phase(const ARM::CHI::Phase& fw_phase, const ARM::CHI::DatOpcode dat_opcode)
{
    ARM::CHI::Phase dat_phase;

    dat_phase.channel = ARM::CHI::CHANNEL_DAT;

    dat_phase.qos = fw_phase.qos;
    dat_phase.tgt_id = fw_phase.return_nid;
    dat_phase.src_id = fw_phase.tgt_id;
    dat_phase.txn_id = fw_phase.return_txn_id;
    dat_phase.home_nid = fw_phase.src_id;
    dat_phase.dat_opcode = dat_opcode;
    dat_phase.resp = ARM::CHI::RESP_UC; // may be not needed
    dat_phase.dbid = fw_phase.txn_id;

    return dat_phase;
}

static ARM::CHI::Phase
make_tag_match_phase(const ARM::CHI::Phase& fw_phase)
{
    ARM::CHI::Phase rsp_phase;

    rsp_phase.channel = ARM::CHI::CHANNEL_RSP;

    rsp_phase.qos = fw_phase.qos;
    rsp_phase.tgt_id = fw_phase.src_id;
    rsp_phase.src_id = fw_phase.tgt_id;
    rsp_phase.rsp_opcode = ARM::CHI::RSP_OPCODE_TAG_MATCH;
    rsp_phase.resp = ARM::CHI::RESP_I; /* Fail */

    return rsp_phase;
}

void
CHIPort::dfi_clock_posedge()
{
    wdat_push_s2();
    wdat_decode_s1();

    req_pop_s3();
    req_decision_s2();
    //
    resp_gen_pcrd();
    req_decode_s1();
}

void
CHIPort::noc_clock_posedge()
{
    rdat_arbit_s1();
    resp_arbit_s1();
}








//rdata channel


void
CHIPort::noc_clock_negedge()
{
    for( const auto channel: {ARM::CHI::CHANNEL_REQ,ARM::CHI::CHANNEL_RSP,ARM::CHI::CHANNEL_DAT,})
    {
        channels[channel].send_flits(channel, [this](ARM::CHI::Payload& payload, ARM::CHI::Phase& phase)
        {
            return target.nb_transport_bw(payload,phase);
        });
    }
}







tlm::tlm_sync_enum
CHIPort::nb_transport_fw(ARM::CHI::Payload& payload, ARM::CHI::Phase& phase)
{
    if(!channels[phase.channel].receive_flit(payload, phase))
        SC_REPORT_ERROR(name(),"flit on activate channel received");

    return tlm::TLM_ACCEPTED;
}

tlm::tlm_sync_enum
CHIPort::nb_transport_bw(tlm::tlm_generic_payload& payload, tlm::tlm_phase& phase, sc_core::sc_time& bwDelay)
{
    payloadEventQueue.notify(payload, phase, bwDelay);
    return tlm::TLM_ACCEPTED;
}

void
CHIPort::peqCallback(tlm::tlm_generic_payload& payload, const tlm::tlm_phase& phase)
{
    if(phase == UIF_CREDIT)
    {
        UifSideBandInfo uifSideBandInfo = payload.get_extension<UifSideBandExtension>()->_uif_side_band_info;
        bool hpr_credit_valid = uifSideBandInfo.hpr_credit_valid;
        bool lpr_credit_valid = uifSideBandInfo.lpr_credit_valid;
        bool tpw_credit_valid = uifSideBandInfo.tpw_credit_valid;
        if(hpr_credit_valid)
        {
            p2cFifo->hpr_queue->ReceiveCredit();
        }
        if(lpr_credit_valid)
        {
            p2cFifo->lpr_queue->ReceiveCredit();
        }
        if(tpw_credit_valid)
        {
            p2cFifo->tpw_queue->ReceiveCredit();
        }
    }
    else if(phase == UIF_RDAT_BEGIN)
    {
        // // 从rdat—info中获取对应的CHI Flit，然后将其放入到对应的rdat buffer中
        // unsigned cmd_id = payload.get_extension<UifExtension>()->_uif_info.cmd_id;
        DPRINT_INFO(true, "CHI Port", "Get the Rdat transaction First Data");
    }
    else if(phase == UIF_RDAT_END)
    {
        // 将rdat buffer中的数据标定为读完成，同时将payload中的数据copy到对应的CHI Flit
        unsigned cmd_id = payload.get_extension<UifExtension>()->_uif_info.cmd_id;
        rdDataInfo->set_entry_data_ready(cmd_id);
        DPRINT_INFO(true, "CHI Port", "Get the Rdat transaction Last Data");
        // 输出读事务的完成时间，并将指针插入到对应的队列map中，等待rdata_info 被移除时，记录对应的读数据在CHI接口处的输出时间
    }
    else if(phase == UIF_WDAT_REQ)
    {
        // unsigned wr_cmd_id = payload.get_extension<UifExtension>()->_uif_info.cmd_id;
        // 当拍传递wdat Begin,并且根据写请求的size，决定传递的拍数，如果为2，则在下一个周期传递UIF_WDAT_END，如果为1，在在delta时间后，传递UIF_WDAT_END
        tlm::tlm_phase wdat_begin_phase = UIF_WDAT_BEGIN;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        iSocket->nb_transport_fw(payload, wdat_begin_phase, delay);
        if(payload.get_data_length() <= 32) //TODO: 32 is fixed value,应该是由uif接口的位宽 计算得出
        {
            payloadEventQueue.notify(payload, UIF_WDAT_END, sc_core::SC_ZERO_TIME);
        }
        else
        {
            payloadEventQueue.notify(payload, UIF_WDAT_END, clock_period);
        }
    }
    else if(phase == UIF_WDAT_END)
    {
        // 释放dbid，同时移除wdat buffer中的data entry
        unsigned wr_cmd_id = payload.get_extension<UifExtension>()->_uif_info.cmd_id;
        wdataBufferArray->release_dbid(wr_cmd_id);
        wdataBufferArray->erase_wdata_buffer_entry(wr_cmd_id);

        tlm::tlm_phase wdat_end_phase = UIF_WDAT_END;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        iSocket->nb_transport_fw(payload, wdat_end_phase, delay);
    }
    else if(phase == WR_RESPONSE_COMPLETE)
    {
        // 释放 tlm::tlm_generic_payload* 队列中的元素，并且输出写事务的完成时间

    }
    else
    {
        SC_REPORT_FATAL(name(), "Invalid phase");
    }
}



CHIPort::CHIPort(const sc_core::sc_module_name& name,const Configure& configure,unsigned data_width_bits,const sc_core::sc_time& clock_period) :
    sc_core::sc_module(name),
    _configure(configure),
    data_width_bytes(data_width_bits / 8),
    target("target",*this,&CHIPort::nb_transport_fw,ARM::TLM::PROTOCOL_CHI_E,data_width_bits),
    dfi_clock("dfi_clcok"),
    noc_clock("noc_clock"),
    clock_period(clock_period),
    payloadEventQueue(this, &CHIPort::peqCallback),
    memoryManager(false)
{

    wdataBufferArray = std::make_unique<WdataBufferArray>(_configure,data_width_bytes);
    rdDataInfo = std::make_unique<RdDataInfo>(_configure);
    p2cFifo = std::make_unique<P2cFifo>(_configure, *wdataBufferArray.get(), *rdDataInfo.get(),clock_period);
    responseQueues = std::make_unique<ResponseQueues>(_configure);
    retryResourceManager = std::make_unique<RetryResourceManager>(_configure,*p2cFifo.get(),*wdataBufferArray.get(),clock_period);

    SC_METHOD(dfi_clock_posedge);
    sensitive<<dfi_clock.pos();
    dont_initialize();

    SC_METHOD(noc_clock_posedge);
    sensitive<<noc_clock.pos();
    dont_initialize();

    SC_METHOD(noc_clock_negedge);
    sensitive<<noc_clock.neg();
    dont_initialize();

    iSocket.register_nb_transport_bw(this, &CHIPort::nb_transport_bw);

    for(const auto channel : {ARM::CHI::CHANNEL_REQ,ARM::CHI::CHANNEL_DAT})
    {
        channels[channel].rx_credits_available = CHI_MAX_LINK_CREDITS;
    }
}




    /*Request Channel*/
void
CHIPort::req_decode_s1()
{
    if(!channels[ARM::CHI::CHANNEL_REQ].rx_queue.empty())
    {
        if(!responseQueues->IsResponseQueueFull(ResponseQueueType::RetryAck))
        {
            CHIFlit& req_flit = channels[ARM::CHI::CHANNEL_REQ].rx_queue.front();
            req_s1.push_back(std::move(req_flit));
            channels[ARM::CHI::CHANNEL_REQ].rx_queue.pop_front();
            // DPRINT_INFO(true, "CHIPort", "Request Channel stage 1, s1 queue size: %d",req_s1.size());
        }
    }
}

void
CHIPort::req_decision_s2(){
    if(!req_s1.empty())
    {
        CHIFlit& req_flit = req_s1.front();
        req_s2.push_back(std::move(req_flit));
        req_s1.pop_front();
        // DPRINT_INFO(true, "CHIPort", "Request Channel stage 2, s2 queue size: %ld",req_s2.size());

        if(req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL ||
           req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL)
        {
            Qos qos = Qos(req_flit.phase.qos,false);
            bool req_accepted = handle_WriteNoSnp(req_flit,qos.GetQosLevel());
            if(!req_accepted)
            {
                responseQueues->InsertRetryAckResp(req_flit);
                if(qos.GetQosLevel() == PriorityClass::TPW){
                    retryResourceManager->inc_write_tpw(req_flit.phase.src_id);
                }
                else if(qos.GetQosLevel() == PriorityClass::GPW){
                    retryResourceManager->inc_write_gpw(req_flit.phase.src_id);
                }
            }
            else {
                unsigned allocated_dbid = wdataBufferArray->allocate_dbid();
                wdataBufferArray->allocate_wdata_buffer_entry(req_flit,allocated_dbid);
                responseQueues->InsertDbidResp(req_flit, allocated_dbid);
                //分配tlm::tlm_generic_payload
                // 插入到TPW队列
                tlm::tlm_generic_payload& trans = memoryManager.allocate(req_flit,false);
                p2cFifo->InsertTpwRequest(req_flit, static_cast<tlm::tlm_generic_payload*>(&trans), allocated_dbid,qos.GetQosLevel());
            }
        }
        else if(req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_READ_NO_SNP
             || req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_READ_NO_SNP_SEP)
        {
            Qos qos = Qos(req_flit.phase.qos,true);
            bool req_accepted = handle_ReadNoSnp(req_flit,qos.GetQosLevel());
            if(!req_accepted)
            {
                responseQueues->InsertRetryAckResp(req_flit);
                if(qos.GetQosLevel() == PriorityClass::GPR){
                    retryResourceManager->inc_read_gpr(req_flit.phase.src_id);
                }
                else if(qos.GetQosLevel() == PriorityClass::LPR){
                    retryResourceManager->inc_read_lpr(req_flit.phase.src_id);
                }
                else if(qos.GetQosLevel() == PriorityClass::HPR){
                    retryResourceManager->inc_read_hpr(req_flit.phase.src_id);
                }
                else{

                }
            }
            else {
                if(qos.GetQosLevel() == PriorityClass::HPR)
                {
                    //分配tlm::tlm_generic_payload
                    tlm::tlm_generic_payload& trans = memoryManager.allocate(req_flit,true);
                    // 插入到HPR队列
                    p2cFifo->InsertHprRequest(req_flit, static_cast<tlm::tlm_generic_payload*>(&trans),qos.GetQosLevel());
                }
                else if(qos.GetQosLevel() == PriorityClass::LPR || qos.GetQosLevel() == PriorityClass::GPR)
                {
                    //分配tlm::tlm_generic_payload
                    tlm::tlm_generic_payload& trans = memoryManager.allocate(req_flit,true);
                    // 插入到LPR队列
                    p2cFifo->InsertLGprRequest(req_flit, static_cast<tlm::tlm_generic_payload*>(&trans),qos.GetQosLevel());
                }
            }
        }
        else if(req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_PREFETCH_TGT)
        {

        }
        else if(req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_CLEAN_SHARED_PERSIST)
        {

        }
        else {
            SC_REPORT_ERROR(name(),"Unsupported Request Opcode");
        }
    }
}

void
CHIPort::req_pop_s3()
{
    if(!req_s2.empty())
        req_s2.pop_front();
    if(!p2cFifo->IsQueueEmpty())
    {
        // DPRINT_INFO(true, "CHIPort", "Request Channel stage 3, s3 queue empty: %d",p2cFifo->IsQueueEmpty());
        auto winning_queue = p2cFifo->GetWinningQueue();
        if(winning_queue == QueueType::HPR || winning_queue == QueueType::LPR)
        {
            auto rd_front_request = p2cFifo->GetRdFrontRequest(winning_queue);
            unsigned allocated_rdata_id = rdDataInfo->allocate_info_tag();
            CHIFlit req_flit = CHIFlit(rd_front_request.payload,rd_front_request.phase);
            rdDataInfo->allocate_info_buffer_entry(req_flit,allocated_rdata_id);
            if(req_flit.phase.order == ARM::CHI::ORDER_REQUEST_ACCEPTED)
                responseQueues->InsertReadReceiptResp(req_flit);
            // sent to downstream
            SendUifRequest(p2cFifo->GetRdFrontRequest(winning_queue),allocated_rdata_id,true);
            p2cFifo->PopRequest(winning_queue);
            p2cFifo->CreditDecrese(winning_queue);
            p2cFifo->UpdateQueueAging(winning_queue);
        }
        else if(winning_queue == QueueType::TPW)
        {
            auto wr_front_request = p2cFifo->GetWrFrontRequest();
            responseQueues->InsertCompResp(CHIFlit(wr_front_request.second.payload,wr_front_request.second.phase));
            auto front_request = p2cFifo->GetWrFrontRequest();
            auto queue_entry = front_request.second;
            auto dbid = front_request.first;
            bool is_rmw = queue_entry.is_rmw;
            SendUifRequest(queue_entry, dbid, false);
            p2cFifo->PopRequest(winning_queue);
            p2cFifo->CreditDecrese(winning_queue);
            if(is_rmw)
                p2cFifo->CreditDecrese(QueueType::LPR);
            p2cFifo->UpdateQueueAging(winning_queue);
        }
        else
        {

        }
    }
}

bool
CHIPort::handle_WriteNoSnp(const CHIFlit& flit, PriorityClass qos_level){
    // return false;
    if(!flit.phase.allow_retry)
    {
        return true;
    }
    else{
        if(qos_level == PriorityClass::TPW || qos_level == PriorityClass::GPW)
        {
            if(retryResourceManager->has_retry_cmd(PriorityClass::TPW) || retryResourceManager->has_retry_cmd(PriorityClass::GPW))
                return false;
            else
            {//
                if(retryResourceManager->is_tpw_full()) // wcq full(pre-allocated + already occupied) || wdatabuffer full
                    return false;
                else
                    return true;
            }
        }
        else
        {
            SC_REPORT_ERROR("Handle Write Transaction Retry Decision", "Invalid Priority Class: not Tpw or Gpw");
        }
    }
    return false;
}

bool
CHIPort::handle_ReadNoSnp(const CHIFlit& flit, PriorityClass qos_level){
    if(!flit.phase.allow_retry)
    {
        return true;
    }
    else{
        //based on the qos level
        if(qos_level == PriorityClass::HPR){
            if(retryResourceManager->has_retry_cmd(PriorityClass::HPR))
                return false;
            else if(retryResourceManager->is_gpr_expired_and_rd_queue_only_one_space()) //the read queue has only one buffer space, and the gpr retry queue is expired
                return false;
            else
            {//
                if(retryResourceManager->is_hpr_full()) // hpr full(pre-allocated + already occupied)
                    return false;
                else
                    return true;
            }
        }
        else if(qos_level == PriorityClass::LPR){
            if(retryResourceManager->has_retry_cmd(PriorityClass::LPR))
                return false;
            else
            {//
                if(retryResourceManager->is_lgpr_full()) // lpr full(pre-allocated + already occupied)
                    return false;
                else
                    return true;
            }
        }
        else if(qos_level == PriorityClass::GPR){
            if(retryResourceManager->has_retry_cmd(PriorityClass::GPR))
                return false;
            else
            {//
                if(retryResourceManager->is_lgpr_full()) // gpr full(pre-allocated + already occupied)
                    return false;
                else
                    return true;
            }
        }
    }
    return false;
}

bool
CHIPort::handle_CleanShardPersist(const CHIFlit& flit){
    if(!flit.phase.allow_retry)
    {
        return true;
    }
    else{
        if(!retryResourceManager->is_type_empty(RetryType::CMO))
        {
            return false;
        }
        else
        {//
            if(retryResourceManager->is_cmo_full()) // cmo full(pre-allocated + already occupied)
                return false;
            else
                return true;
        }
    }
    return false;
}

bool
CHIPort::handle_PrefetchTgt(const CHIFlit& flit){
    if(!flit.phase.allow_retry)
    {
        return true;
    }
    else{

    }
    return true;
}

/*Response Channel*/
void
CHIPort::resp_arbit_s1()
{
    if(responseQueues->HasRspPending())
    {
        int winning_queue_index = responseQueues->Arbiter();
        channels[ARM::CHI::CHANNEL_RSP].tx_queue.emplace_back(std::move(responseQueues->GetQueueFront(static_cast<uint8_t>(winning_queue_index))));
        responseQueues->QueuePop(static_cast<uint8_t>(winning_queue_index));
    }
}

void
CHIPort::resp_gen_pcrd()
{
    if(retryResourceManager->is_need_to_send_pcrd_grant())
    {
        PortCmdType pcrd_cmd_type = retryResourceManager->select_type_cmd_type();
        unsigned pcrd_tgt_id = retryResourceManager->select_src_id(pcrd_cmd_type); // when do the selection the counter has decrese same time
        ARM::CHI::Payload* pcrd_payload = ARM::CHI::Payload::get_dummy();
        ARM::CHI::Phase pcrd_phase;
        pcrd_phase.channel = ARM::CHI::Channel::CHANNEL_RSP;
        pcrd_phase.lcrd = false;
        //TODO:
        pcrd_phase.rsp_opcode = ARM::CHI::RSP_OPCODE_PCRD_GRANT;
        responseQueues->InsertPcrdGrantResp(CHIFlit(*pcrd_payload,pcrd_phase));
        // retryResourceManager->cnt_dec(pcrd_cmd_type, pcrd_tgt_id);
        retryResourceManager->send_upstream_p_credit_inc(pcrd_cmd_type);
    }
}

/*Write Data Channel*/
void
CHIPort::wdat_decode_s1()
{
    if(!channels[ARM::CHI::CHANNEL_DAT].rx_queue.empty())
    {
        CHIFlit& wdat_flit = channels[ARM::CHI::CHANNEL_DAT].rx_queue.front();
        wdat_s1.emplace_back(std::move(wdat_flit));
        channels[ARM::CHI::CHANNEL_DAT].rx_queue.pop_front();
    }
}

void
CHIPort::wdat_push_s2()
{
    if(!wdat_s1.empty())
    {
        wdataBufferArray->receive_wdata_flit(std::move(wdat_s1.front()));
        if(p2cFifo->IsTpwQueueHeadEmpty() && wdataBufferArray->HasEntryReady()) {
            p2cFifo->MoveTpwQueue2Head();
        }
        wdat_s1.pop_front();
    }
}

/*Read Data Channel*/
void
CHIPort::rdat_arbit_s1()
{
    if(rdDataInfo->has_entry_ready())
    {
        auto element = rdDataInfo->get_ready_entry();
        ARM::CHI::Phase data_phase = make_read_data_phase(element.second.phase,ARM::CHI::DAT_OPCODE_COMP_DATA);
        channels[ARM::CHI::CHANNEL_DAT].tx_queue.emplace_back(std::move(CHIFlit(element.second.payload,data_phase)));
        rdDataInfo->erase_entry(element.first);
    }
}

/*UIF send Request*/
void
CHIPort::SendUifRequest(const QueueEntry& entry, unsigned cmd_id, bool is_rd)
{
    auto trans = entry.trans;
    UifInfo uif_info;
    uif_info.qos = Qos(entry.phase.qos,is_rd);
    uif_info.expired_time = entry.expired_time;
    uif_info.is_rmw = !is_rd && (entry.payload.byte_enable != ~uint64_t(0));
    uif_info.cmd_type = is_rd ? CmdType::RD : (uif_info.is_rmw ? CmdType::RMW : CmdType::WR);
    uif_info.cmd_id = cmd_id;
    UifExtension* uif_ext = new UifExtension(uif_info);
    trans->set_extension(uif_ext);
    trans->set_address(entry.payload.address);
    trans->set_command(is_rd ? tlm::TLM_READ_COMMAND : tlm::TLM_WRITE_COMMAND);
    trans->set_data_length(entry.payload.size);
    trans->get_extension<StatisticExtension>()->RecordOutPortTime(sc_core::sc_time_stamp());
    tlm::tlm_phase req_phase = UIF_REQ;
    sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
    iSocket->nb_transport_fw(*trans, req_phase, delay);
}

    } // namespace Port
} // namespace dmu