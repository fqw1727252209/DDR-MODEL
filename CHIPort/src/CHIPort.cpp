#include "CHIPort/CHIPort.h"

#include "CHIPort/CHIUtilities.h"

#include "CHIPort/DBExtension.h"
#include "CHIPort/PortStruct.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
namespace dmu {
static ARM::CHI::Phase
make_response_phase(const ARM::CHI::Phase& fw_phase, const ARM::CHI::RspOpcode rsp_opcode, const unsigned dbid = 0)
{
    ARM::CHI::Phase rsp_phase;

    rsp_phase.channel = ARM::CHI::CHANNEL_RSP;

    rsp_phase.qos = fw_phase.qos;
    rsp_phase.tgt_id = fw_phase.src_id;
    rsp_phase.src_id = fw_phase.tgt_id;
    rsp_phase.txn_id = fw_phase.txn_id;
    rsp_phase.home_nid = fw_phase.tgt_id;
    rsp_phase.rsp_opcode = rsp_opcode;
    rsp_phase.dbid = dbid;

    return rsp_phase;
}
static ARM::CHI::Phase
make_read_data_phase(const ARM::CHI::Phase& fw_phase, const ARM::CHI::DatOpcode dat_opcode)
{
    // the fw phase must be req phase
    ARM::CHI::Phase dat_phase;

    dat_phase.channel = ARM::CHI::CHANNEL_DAT;

    dat_phase.qos = fw_phase.qos;
    dat_phase.tgt_id = fw_phase.return_nid;
    dat_phase.src_id = fw_phase.tgt_id;
    dat_phase.txn_id = fw_phase.return_txn_id;
    dat_phase.home_nid = fw_phase.src_id;
    dat_phase.dat_opcode = dat_opcode;
    dat_phase.resp = ARM::CHI::RESP_UC; // need ?
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
CHIPort::clock_posedge()
{
    for (const auto channel : {
             ARM::CHI::CHANNEL_REQ,
             ARM::CHI::CHANNEL_DAT,
         })
    {
        channels[channel].rx_credits_update();
    }
    //rsp channel
    if(rsp_flit_pending.has_value())
    {
        channels[ARM::CHI::CHANNEL_RSP].tx_queue.emplace_back(rsp_flit_pending.value());
        rsp_flit_pending.reset();
    }
    if(rsp_queue->HasRspPending())
    {
        rsp_flit_pending.emplace(rsp_queue->Pop(rsp_queue->Arbiter()));
    }
    delay_command_queue->check_dcq_ready();

    retry_resource_manager->update_condition_state();
    if(!rsp_queue->is_pcrd_buffer_occupied() && !retry_resource_manager->is_empty() && retry_resource_manager->pcrd_available())
    {
        auto [retry_type,qos,tgt_id] = retry_resource_manager->gen_pcrd_rsp();
        #ifdef CHIPort_TEST
        std::cout << "generated PcrdGrant Response: " << "type: " << retry_type << " src id: " <<tgt_id << " qos: " <<qos<<std::endl;
        #endif
        ARM::CHI::Payload* const payload = ARM::CHI::Payload::get_dummy();
        ARM::CHI::Phase phase;
        phase.channel = ARM::CHI::CHANNEL_RSP;

        phase.qos = qos;
        phase.rsp_opcode = ARM::CHI::RSP_OPCODE_PCRD_GRANT;
        phase.tgt_id = tgt_id;
        phase.src_id = SRC_ID; // this->SRC_ID;
        rsp_queue->Pcrd_buffer.emplace(*payload,phase);
    }

    //data channel
    if (!channels[ARM::CHI::CHANNEL_DAT].rx_queue.empty())
    {
        const CHIFlit dat_flit = channels[ARM::CHI::CHANNEL_DAT].rx_queue.front();
        channels[ARM::CHI::CHANNEL_DAT].rx_queue.pop_front();

        switch (dat_flit.phase.dat_opcode)
        {
        case ARM::CHI::DAT_OPCODE_NON_COPY_BACK_WR_DATA:
        case ARM::CHI::DAT_OPCODE_NCB_WR_DATA_COMP_ACK:
        case ARM::CHI::DAT_OPCODE_WRITE_DATA_CANCEL:
            wdata_buffer_array->receive_wdat_flit(dat_flit);
            break;
        default:
            SC_REPORT_ERROR(name(), "unexpected write data opcode received");
        }
    }


    //req channel
    {
        p2c_pop();
        decision_req_stage();
        if(!rsp_queue->blocked && rsp_queue->is_pcrd_buffer_occupied())
        {
            rsp_queue->Response_Queues.at(static_cast<std::size_t>(RespQueueType::Retry)).emplace_back(
                rsp_queue->Pcrd_buffer.value());
            rsp_queue->Pcrd_buffer.reset();
        }
        else if(rsp_queue->blocked)
        {
            rsp_queue->blocked = false;
        }
        decode_req_stage();
        intf_req_stage();
    }
    /* The other channels are inactive and cannot receive flits, so no need to process them. */
}

void
CHIPort::intf_req_stage()
{
    if (!channels[ARM::CHI::CHANNEL_REQ].rx_queue.empty())
    {
        const CHIFlit req_flit = channels[ARM::CHI::CHANNEL_REQ].rx_queue.front();
        channels[ARM::CHI::CHANNEL_REQ].rx_queue.pop_front();
        //----------------code above is the flit stage----------------//
        if(SRC_ID<0)
            SRC_ID = req_flit.phase.tgt_id;
        rx_queue_s1.emplace_back(req_flit);// s1 stage is the decode stage
    }
}

void
CHIPort::decode_req_stage()
{
    if(!rx_queue_s1.empty()){
        const CHIFlit req_flit_s1 = rx_queue_s1.front();
        // update the write condition
        // retry_resource_manager->update_condition_state();
        rx_queue_s1.pop_front();
        switch (req_flit_s1.phase.req_opcode)
        {
        case ARM::CHI::REQ_OPCODE_READ_NO_SNP:
        case ARM::CHI::REQ_OPCODE_READ_NO_SNP_SEP:
            grant_s1 = handle_rdnosnp_req(req_flit_s1);
            break;
        case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL:
            grant_s1 = handle_wrnosnpptl_req(req_flit_s1);
            break;
        case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL:
            grant_s1 = handle_wrnosnpful_req(req_flit_s1);
            break;
        case ARM::CHI::REQ_OPCODE_PCRD_RETURN:
        case ARM::CHI::REQ_OPCODE_PREFETCH_TGT:
        case ARM::CHI::REQ_OPCODE_CLEAN_SHARED:
        case ARM::CHI::REQ_OPCODE_CLEAN_SHARED_PERSIST:
        case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_ZERO:
        default:
            SC_REPORT_ERROR(name(), "unexpected request opcode received");
        }
        rx_queue_s2.emplace_back(req_flit_s1);
    }
    if(!grant_s1) // there is no reqeust enter in the stage 2
    {
        if(!delay_command_queue->is_ready())
        {
            grant_dcq_s1 = false;
            return;
        }
        if((p2c_fifo->P2C_FIFO_SIZE - p2c_fifo->size()) < (CHI_MAX_LINK_CREDITS - channels[ARM::CHI::CHANNEL_REQ].rx_credits_available))
        {
            grant_dcq_s1 = false;
            return;
        }
        grant_dcq_s1 = true;
    }
}

void 
CHIPort::decision_req_stage()
{
    grant_s2 = grant_s1;
    grant_dcq_s2 = grant_dcq_s1;
    grant_s1 = false;
    grant_dcq_s1 = false;
    if(!rx_queue_s2.empty())
    {
        const CHIFlit req_flit_s2 = rx_queue_s2.front();
        rx_queue_s2.pop_front();
        // std::cout <<sc_core::sc_time_stamp()<<" "<<name()<<":"<< " in stage s2 ,decision stage"<<std::endl;
        if(grant_s2){
            unsigned index = 0;
            /*
            do the write to p2c fifo
            do the req info store into data structure
            do the response generation
            */
            switch (req_flit_s2.phase.req_opcode)
            {
            case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL:
                index = wdata_buffer_array->allocate_dbid();
                wdata_buffer_array->allocate_wdat_buffer_entry(req_flit_s2,index);
                wdata_buffer_array->insert_ptl_id(index);
                delay_command_queue->allocate_dcq_buffer_entry(req_flit_s2,index);
                if(!req_flit_s2.phase.allow_retry)
                    resource_manage_unit->write_pcredit_dec();
                break;
            case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL:
                // except wr_ptl, all request write into p2c-fifo;
                index = wdata_buffer_array->allocate_dbid();
                wdata_buffer_array->allocate_wdat_buffer_entry(req_flit_s2,index);
                if(!req_flit_s2.phase.allow_retry)
                    resource_manage_unit->write_pcredit_dec();
                p2c_fifo->Push(req_flit_s2,index,1);
                break;
            case ARM::CHI::REQ_OPCODE_READ_NO_SNP:
            case ARM::CHI::REQ_OPCODE_READ_NO_SNP_SEP:
                index = rdata_info_queue->allocate_infotag();
                rdata_info_queue->rdata_info_buffer.emplace(index,req_flit_s2);
                if(!req_flit_s2.phase.allow_retry)
                    resource_manage_unit->read_pcredit_dec();
                p2c_fifo->Push(req_flit_s2,index,0);
                // Rdsent2Dramsys(req_flit_s2,index);
                break;
            case ARM::CHI::REQ_OPCODE_PCRD_RETURN:
            case ARM::CHI::REQ_OPCODE_PREFETCH_TGT:
            case ARM::CHI::REQ_OPCODE_CLEAN_SHARED:
            case ARM::CHI::REQ_OPCODE_CLEAN_SHARED_PERSIST:
            case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_ZERO:
            default:
                SC_REPORT_ERROR(name(), "unexpected request opcode received");
            }
            gen_req_rsp(req_flit_s2,true,index);
        }
        else
        {
            gen_req_rsp(req_flit_s2,false,0);
        }
    }
    
    if(grant_dcq_s2){
        auto head_entry = delay_command_queue->get_head();
        if(head_entry.has_value())
        {
            const CHIFlit wr_ptl_flit = head_entry.value().second;
            gen_dcq_rsp(wr_ptl_flit);
            p2c_fifo->Push(wr_ptl_flit,head_entry.value().first,1);
            delay_command_queue->Pop();
        }
        else{
            SC_REPORT_ERROR(name(), "dcq was granted but there is no legal request to be pop");
        }
    }

}

void 
CHIPort::p2c_pop()
{
    if(p2c_fifo->size() == 0)
        return;
    for(auto elem = p2c_fifo->P2CFIFO.begin(); elem != p2c_fifo->P2CFIFO.end(); elem++)
    {
        if(elem->rw_type == 0)
        {
            // std::cout << "read data from dramsys" <<std::endl;
            Rdsent2Dramsys(*elem);
            p2c_fifo->P2CFIFO.erase(elem);
            break;
        }
        else if(elem->rw_type == 1)
        {
            if(wdata_buffer_array->data_buffer.at(elem->dbid).is_entry_ready())
            {
                // std::cout << "write data from dramsys" <<std::endl;
                Wrsent2Dramsys(*elem);
                p2c_fifo->P2CFIFO.erase(elem);
                break;
            }
            else
            {
                continue;
            }
    }
        else{
            SC_REPORT_ERROR(name(), "illegal   rw type");
        }
    }
}


void 
CHIPort::gen_req_rsp(const CHIFlit& req_flit, const bool req_grant, const unsigned index)
{
    ARM::CHI::RspOpcode rsp_opcode;
    if(req_grant)
    {
        //normal: DbidResp, CompDbidResp, receipt,
        switch(req_flit.phase.req_opcode)
        {
            case ARM::CHI::REQ_OPCODE_READ_NO_SNP:
                if(req_flit.phase.order == ARM::CHI::ORDER_REQUEST_ACCEPTED)
                {
                    rsp_queue->Response_Queues.at(static_cast<std::size_t>(RespQueueType::REQ)).emplace_back(
                        req_flit.payload, make_response_phase(req_flit.phase, ARM::CHI::RSP_OPCODE_READ_RECEIPT));
                }
                break;
            case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL:
                rsp_queue->Response_Queues.at(static_cast<std::size_t>(RespQueueType::DBID)).emplace_back(
                    req_flit.payload, make_response_phase(req_flit.phase, ARM::CHI::RSP_OPCODE_COMP_DBID_RESP,
                    index));
                break;
            case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL:
                rsp_queue->Response_Queues.at(static_cast<std::size_t>(RespQueueType::DBID)).emplace_back(
                    req_flit.payload, make_response_phase(req_flit.phase, ARM::CHI::RSP_OPCODE_DBID_RESP,
                    index));
                break;
            case ARM::CHI::REQ_OPCODE_PCRD_RETURN:
            case ARM::CHI::REQ_OPCODE_PREFETCH_TGT:
            case ARM::CHI::REQ_OPCODE_CLEAN_SHARED_PERSIST:
            case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_ZERO:
            default:
                SC_REPORT_ERROR(name(), "unexpected request opcode received");
        }
    }
    else
    {
        rsp_queue->Response_Queues.at(static_cast<std::size_t>(RespQueueType::Retry)).emplace_back(
            req_flit.payload, make_response_phase(req_flit.phase, ARM::CHI::RSP_OPCODE_RETRY_ACK));
        rsp_queue->blocked = true;
        unsigned type = req_trans_type_map(req_flit.phase);
        // qos need to map to 4 qos level
        retry_resource_manager->cnt_inc(type,req_flit.phase.qos % 4,req_flit.phase.src_id);
    }
}

void 
CHIPort::gen_pcrdgrant_rsp()
{
    auto [retry_type,qos,tgt_id] = retry_resource_manager->gen_pcrd_rsp();
    #ifdef CHIPort_TEST
    std::cout << "generated PcrdGrant Response: " << "type: " << retry_type << " src id: " <<tgt_id << " qos: " <<qos<<std::endl;
    #endif
    ARM::CHI::Payload* const payload = ARM::CHI::Payload::get_dummy();
    ARM::CHI::Phase phase;
    phase.channel = ARM::CHI::CHANNEL_RSP;

    phase.qos = qos;
    phase.rsp_opcode = ARM::CHI::RSP_OPCODE_PCRD_GRANT;
    phase.tgt_id = tgt_id;
    phase.src_id = this->SRC_ID;
    rsp_queue->Pcrd_buffer.emplace(*payload,phase);
}
void 
CHIPort::gen_retry_rsp(const CHIFlit& req_flit)
{
    rsp_queue->Response_Queues.at(static_cast<std::size_t>(RespQueueType::Retry)).emplace_back(
        req_flit.payload, make_response_phase(req_flit.phase, ARM::CHI::RSP_OPCODE_RETRY_ACK));
    rsp_queue->blocked = true;
    unsigned type = req_trans_type_map(req_flit.phase);
    // qos need to map to 4 qos level
    retry_resource_manager->cnt_inc(type,req_flit.phase.qos % 4,req_flit.phase.src_id);
}
void 
CHIPort::gen_dcq_rsp(const CHIFlit& req_flit)
{
    rsp_queue->Response_Queues.at(static_cast<std::size_t>(RespQueueType::Comp)).emplace_back(
        req_flit.payload, make_response_phase(req_flit.phase, ARM::CHI::RSP_OPCODE_COMP));
}

bool 
CHIPort::handle_rdnosnp_req(const CHIFlit& req_flit)
{
    if(!req_flit.phase.allow_retry){
        return true;
    }
    else{
        // return true;
        if(req_flit.phase.qos < resource_manage_unit->rd_qos_threshold){ // request qos must higher than the read qos-threshold in
            return false;
        }
        if(req_flit.phase.qos <= rsp_queue->rtq_rd_max_qos && (p2c_fifo->size() >= p2c_fifo->P2C_FIFO_SIZE - 1 ||
           rdata_info_queue->size() >= rdata_info_queue->RDATA_INFO_SIZE -1) && rsp_queue->Response_Queues.at(static_cast<std::size_t>(RespQueueType::Retry)).size()!=0){
            return false;
        }
        if(rdata_info_queue->size() >= rdata_info_queue->RDATA_INFO_SIZE){ // rdata_info_entry must has room for new request
            return false;
        }
        if(rdata_info_queue->size() == rdata_info_queue->RDATA_INFO_SIZE -1 && rd_retry_enable){ // when there is only one buffer entry avaliable, and there is retry, the buffer will b
            return false;
        }
        if(delay_command_queue->is_timeout()){
            return false;
        }
        return true;
    }
}

bool 
CHIPort::handle_wrnosnpptl_req(const CHIFlit& req_flit)
{
    if(!req_flit.phase.allow_retry){
        return true;
    }
    else{
        // return true;
        if(delay_command_queue->size() >= delay_command_queue->DCQ_INFO_SIZE){
            return false;
        }
        if(delay_command_queue->size() == delay_command_queue->DCQ_INFO_SIZE - 1 && wr_retry_enable){
            return false;
        }
        if(req_flit.phase.qos < resource_manage_unit->wr_qos_threshold){
            return false;
        }
        if(req_flit.phase.qos <= rsp_queue->rtq_wr_max_qos){
            return false;
        }
        if(wdata_buffer_array->size() >= wdata_buffer_array->WDAT_BUFFER_SIZE){
            return false;
        }
        if(wdata_buffer_array->size() == wdata_buffer_array->WDAT_BUFFER_SIZE - 1 && wr_retry_enable){
            return false;
        }
        // since wr_partial will store into dcq, it wont compete with dcq_grant signal
        return true;
    }
}

bool 
CHIPort::handle_wrnosnpful_req(const CHIFlit& req_flit)
{
    if(!req_flit.phase.allow_retry){
        return true;
    }
    else{
        // return true;
        if(req_flit.phase.qos < resource_manage_unit->wr_qos_threshold){
            return false;
        }
        if(req_flit.phase.qos <= rsp_queue->rtq_wr_max_qos){
            return false;
        }
        if(wdata_buffer_array->size() >= wdata_buffer_array->WDAT_BUFFER_SIZE){
            return false;
        }
        if(wdata_buffer_array->size() == wdata_buffer_array->WDAT_BUFFER_SIZE - 1 && wr_retry_enable){
            return false;
        }
        if(delay_command_queue->is_timeout()){
            return false;
        }
        return true;
    }
}

void 
CHIPort::clock_negedge()
{
    /* Try to issue credits and send transactions on active channels. */
    for (const auto channel : {ARM::CHI::CHANNEL_REQ, ARM::CHI::CHANNEL_RSP, ARM::CHI::CHANNEL_DAT})
    {
        channels[channel].send_flits(channel, [this](ARM::CHI::Payload& payload, ARM::CHI::Phase& phase) {
            return target.nb_transport_bw(payload, phase);
        });
    }

}

void
CHIPort::Rdsent2Dramsys(const CHIFlit& req_flit, const unsigned& rdatinfo_tag)
{
    const unsigned data_bytes = 1 << req_flit.payload.size;
    tlm::tlm_generic_payload& payload = memoryManager.allocate(CHI_CACHE_LINE_SIZE_BYTES);
    payload.acquire();
    payload.set_address(req_flit.payload.address);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    payload.set_dmi_allowed(false);
    payload.set_byte_enable_length(CHI_CACHE_LINE_SIZE_BYTES);
    payload.set_data_length(CHI_CACHE_LINE_SIZE_BYTES);
    for (std::size_t i = 0; i < payload.get_data_length(); i++)
    {
        std::size_t byteEnableIndex = i % payload.get_byte_enable_length();
        payload.get_byte_enable_ptr()[byteEnableIndex] = TLM_BYTE_ENABLED;
    }
    payload.set_command(tlm::TLM_READ_COMMAND);
    dmu::DBIntfExtension::setAutoExtension(payload, 0, rdatinfo_tag, req_flit.phase.src_id);
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    sc_core::sc_time delay(0, sc_core::SC_NS);
    iSocket->nb_transport_fw(payload, phase, delay);
}

void 
CHIPort::Rdsent2Dramsys(const P2C_INFO& req_info)
{
    const unsigned data_bytes = 1 << req_info.payload.size;
    tlm::tlm_generic_payload& new_payload = memoryManager.allocate(CHI_CACHE_LINE_SIZE_BYTES);
    new_payload.acquire();
    new_payload.set_address(req_info.payload.address);
    new_payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    new_payload.set_dmi_allowed(false);
    new_payload.set_byte_enable_length(CHI_CACHE_LINE_SIZE_BYTES);
    new_payload.set_data_length(CHI_CACHE_LINE_SIZE_BYTES);
    for (std::size_t i = 0; i < new_payload.get_data_length(); i++)
    {
        std::size_t byteEnableIndex = i % new_payload.get_byte_enable_length();
        new_payload.get_byte_enable_ptr()[byteEnableIndex] = TLM_BYTE_ENABLED;
    }
    new_payload.set_command(tlm::TLM_READ_COMMAND);
    dmu::DBIntfExtension::setAutoExtension(new_payload,0,req_info.rdinfo_tag,req_info.src_id);
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    sc_core::sc_time delay(0,sc_core::SC_NS);
    iSocket->nb_transport_fw(new_payload, phase, delay);
}


void 
CHIPort::Wrsent2Dramsys(const CHIFlit& dat_flit)
{
    const uint64_t be = dat_flit.payload.byte_enable & transaction_valid_bytes_mask(dat_flit.payload);
    const unsigned data_bytes = 1 << dat_flit.payload.size;
    tlm::tlm_generic_payload& payload = memoryManager.allocate(CHI_CACHE_LINE_SIZE_BYTES);
    if(payload.get_data_ptr() == nullptr || payload.get_byte_enable_ptr() == nullptr)
        SC_REPORT_FATAL(name(), "write payload ptr or byte_enable_ptr is empty");
    memcpy(payload.get_data_ptr(), dat_flit.payload.data, sizeof(dat_flit.payload.data));
    payload.acquire();
    payload.set_address(dat_flit.payload.address);
    payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    payload.set_dmi_allowed(false);
    payload.set_byte_enable_length(CHI_CACHE_LINE_SIZE_BYTES);
    payload.set_data_length(CHI_CACHE_LINE_SIZE_BYTES);
    for (std::size_t i = 0; i < payload.get_data_length(); i++)
    {
        std::size_t byteEnableIndex = i % payload.get_byte_enable_length();
        if((be >> i & 1) != 0)
            payload.get_byte_enable_ptr()[byteEnableIndex] = TLM_BYTE_ENABLED;
    }
    payload.set_command(tlm::TLM_WRITE_COMMAND);
    dmu::DBIntfExtension::setAutoExtension(payload,dat_flit.phase.txn_id,0,dat_flit.phase.src_id);
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    sc_core::sc_time delay(0,sc_core::SC_NS);
    iSocket->nb_transport_fw(payload, phase, delay);
}

void
CHIPort::Wrsent2Dramsys(const P2C_INFO& req_info)
{
    const uint64_t be = req_info.payload.byte_enable & transaction_valid_bytes_mask(req_info.payload);
    const unsigned data_bytes = 1 << req_info.payload.size;
    tlm::tlm_generic_payload& new_payload = memoryManager.allocate(CHI_CACHE_LINE_SIZE_BYTES);
    if(new_payload.get_data_ptr() == nullptr || new_payload.get_byte_enable_ptr() == nullptr)
        SC_REPORT_FATAL(name(), "write payload ptr or byte_enable_ptr is empty");
    memcpy(new_payload.get_data_ptr(), req_info.payload.data, sizeof(req_info.payload.data));
    new_payload.acquire();
    new_payload.set_address(req_info.payload.address);
    new_payload.set_response_status(tlm::TLM_INCOMPLETE_RESPONSE);
    new_payload.set_dmi_allowed(false);
    new_payload.set_byte_enable_length(CHI_CACHE_LINE_SIZE_BYTES);
    new_payload.set_data_length(CHI_CACHE_LINE_SIZE_BYTES);
    for (std::size_t i = 0; i < new_payload.get_data_length(); i++)
    {
        std::size_t byteEnableIndex = i % new_payload.get_byte_enable_length();
        if ((be >> i & 1) != 0)
            new_payload.get_byte_enable_ptr()[byteEnableIndex] = TLM_BYTE_ENABLED;
    }
    new_payload.set_command(tlm::TLM_WRITE_COMMAND);
    dmu::DBIntfExtension::setAutoExtension(new_payload,req_info.dbid,0,req_info.src_id);
    tlm::tlm_phase phase = tlm::BEGIN_REQ;
    sc_core::sc_time delay(0,sc_core::SC_NS);
    iSocket->nb_transport_fw(new_payload, phase, delay);
}



void 
CHIPort::peqCallback(tlm::tlm_generic_payload& payload, const tlm::tlm_phase& phase)
{
    if (phase == tlm::END_REQ)
    {
        // std::cout<<sc_core::sc_time_stamp()<<" a transaction has sent to dramsys\r";
    }
    else if (phase == tlm::BEGIN_RESP)
    {
        tlm::tlm_phase nextPhase = tlm::END_RESP;
        sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
        iSocket->nb_transport_fw(payload, nextPhase, delay);
        if(payload.get_command() == tlm::TLM_READ_COMMAND)
        {
            if(!rdata_info_queue->rdata_info_buffer.empty())
            {
                uint16_t id_index = dmu::DBIntfExtension::getRdatInfoTag(payload);
                CHIFlit& sent_flit = rdata_info_queue->rdata_info_buffer.at(id_index);
                memcpy(sent_flit.payload.data, payload.get_data_ptr(), CHI_CACHE_LINE_SIZE_BYTES);

                ARM::CHI::Phase dat_phase = make_read_data_phase(sent_flit.phase, ARM::CHI::DAT_OPCODE_COMP_DATA);

                for (auto data_id : transaction_data_ids(sent_flit.payload, data_width_bytes))
                {
                    dat_phase.data_id = data_id;
                    channels[ARM::CHI::CHANNEL_DAT].tx_queue.emplace_back(sent_flit.payload, dat_phase);
                }
                rdata_info_queue->release_infotag(id_index);
                rdata_info_queue->rdata_info_buffer.erase(id_index);
            }
            else{
                SC_REPORT_FATAL(name(), "rdata_info_queue is empty, but there are data come back");
            }
        }
        if(payload.get_command() == tlm::TLM_WRITE_COMMAND){
            uint16_t index = dmu::DBIntfExtension::getDBID(payload);
            wdata_buffer_array->release_dbid(index);
            wdata_buffer_array->data_buffer.erase(index);
        }
        payload.release();
    }
    else
    {
        SC_REPORT_FATAL("CHIPort", "PEQ was triggered with unknown phase");
    }
}

tlm::tlm_sync_enum
CHIPort::nb_transport_fw(ARM::CHI::Payload& payload, ARM::CHI::Phase& phase)
{
    if (!channels[phase.channel].receive_flit(payload, phase))
        SC_REPORT_ERROR(name(), "flit on inactive channel received");

    return tlm::TLM_ACCEPTED;
}

tlm::tlm_sync_enum
CHIPort::nb_transport_bw(tlm::tlm_generic_payload& payload,
                         tlm::tlm_phase& phase,
                         sc_core::sc_time& bwDelay)
{
    payloadEventQueue.notify(payload, phase, bwDelay);
    return tlm::TLM_ACCEPTED;
}

CHIPort::CHIPort(const sc_core::sc_module_name& name, const unsigned data_width_bits) :
    sc_core::sc_module(name),
    data_width_bytes{data_width_bits / 8},
    target("CHIPort_target", *this, &CHIPort::nb_transport_fw, ARM::TLM::PROTOCOL_CHI_E, data_width_bits),
    clock("clock"),
    payloadEventQueue(this, &CHIPort::peqCallback),
    memoryManager(true)//,

{
    iSocket.register_nb_transport_bw(this, &CHIPort::nb_transport_bw);
    cmo_resp_queue = new CMOResponseQueue(32);
    rsp_queue = new ResponseQueues();
    p2c_fifo = new P2cFifo(32);
    rdata_info_queue = new RdataInfo(32);
    wdata_buffer_array = new WdataBufferArray(data_width_bits / 8);
    delay_command_queue = new DelayCommandQueue(wdata_buffer_array);
    resource_manage_unit = new ResourceManage(this);
    retry_resource_manager = new RetryResourceManager(this);

    SC_METHOD(clock_posedge);
    sensitive << clock.pos();
    dont_initialize();

    SC_METHOD(clock_negedge);
    sensitive << clock.neg();
    dont_initialize();

    /* We will need to issue link credits to our peer so that they can ... */
    for (const auto channel : {
             ARM::CHI::CHANNEL_REQ, /* ... send requests (e.g. ReadNoSnp) */
             ARM::CHI::CHANNEL_DAT, /* ... send write data (e.g. NonCopyBackWrData) */
         })
    {
        channels[channel].rx_credits_available = CHI_MAX_LINK_CREDITS;
    }
}

CHIPort::~CHIPort()
{
    delete p2c_fifo;
    delete rsp_queue;
    delete resource_manage_unit;
    delete retry_resource_manager;
    delete wdata_buffer_array;
    delete rdata_info_queue;
    delete delay_command_queue;
    delete cmo_resp_queue;
}

} // dmu namespace