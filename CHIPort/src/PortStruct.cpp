#include "CHIPort/PortStruct.h"
#include <cstring>
#include <algorithm>
#include <cassert>
namespace dmu {

DBField::DBField(const CHIFlit& req_flit)
// :db_addr(req_flit.payload.address),
// offset(req_flit.payload.address & ~CHI_CACHE_LINE_ADDRESS_MASK),
// numb_bytes(1 << req_flit.payload.size),
// is_wrnosnp0(req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_ZERO),
// qos(req_flit.phase.qos),
// // mpam(req_flit.payload.mpam),
// src_id(req_flit.phase.src_id),
// is_pref_rdtype(req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_PREFETCH_TGT),
// payload(req_flit.payload)
: DBField(req_flit, 0, 0)
{
    // payload.ref();
}

DBField::DBField(const CHIFlit& req_flit, const unsigned& buffer_index)
: DBField(req_flit, buffer_index, 0)
{
    // payload.ref();
}
DBField::DBField(const CHIFlit& req_flit, const unsigned& buffer_index, const unsigned& rw_type)
: db_addr(req_flit.payload.address),
offset(req_flit.payload.address & ~CHI_CACHE_LINE_ADDRESS_MASK),
numb_bytes(1 << req_flit.payload.size),
is_wrnosnp0(req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_ZERO),
qos(req_flit.phase.qos),
// mpam(req_flit.payload.mpam),
src_id(req_flit.phase.src_id),
is_pref_rdtype(req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_PREFETCH_TGT),
payload(req_flit.payload),
buffer_index(buffer_index),
rw_type(rw_type)
{
    // std::cout << "in DBField construct: " << rw_type << " " << rw_type << std::endl;
    payload.ref();
}
DBField::~DBField()
{
    payload.unref();
}

DBField::DBField(const DBField& other)
: db_addr(other.db_addr),
offset(other.offset),
numb_bytes(other.numb_bytes),
is_wrnosnp0(other.is_wrnosnp0),
qos(other.qos),
// mpam(req_flit.payload.mpam),
src_id(other.src_id),
is_pref_rdtype(other.is_pref_rdtype),
payload(other.payload),
buffer_index(other.buffer_index),
rw_type(other.rw_type)
{
}

/* Response */
ResponseQueues::ResponseQueues()
{
    Response_Queues.reserve(5);
}

int
ResponseQueues::Arbiter()
{
    int queue_index = -1;
    for(int i=0; i<5; ++i) {
        int index = ((winner_queue_index+1) + i) % 5;
        if(!Response_Queues.at(index).empty()){
            queue_index = index;
            break;
        }
    }
    // when do the arbiter, the queue_index must not be negative
    winner_queue_index = queue_index;
    // assert(queue_index != -1);
    return queue_index;
}

bool 
ResponseQueues::HasRspPending()
{
    for(auto Response_Queue : Response_Queues)
    {
        if(!Response_Queue.empty()){
            return true;
        }
    }
    return false;
};

void 
ResponseQueues::Push(const CHIFlit& rsp_flit, int index)
{
    // qos_record_queue.at(trans_type_map(rsp_flit.phase)).insert(rsp_flit.phase.qos);
    Response_Queues.at(index).emplace_back(rsp_flit);
};
const CHIFlit 
ResponseQueues::Pop(int index)
{
    CHIFlit resp_flit = Response_Queues.at(index).front();
    Response_Queues.at(index).pop_front();
    return resp_flit;
};



/* RetryResourceManager */
RetryResourceManager::RetryResourceManager(CHIPort* CHIPort_ptr)
: 
CHIPort_ptr(CHIPort_ptr)
{
    for(unsigned i=0; i<3; ++i)
    {
        Qos_Srcid_Matrix initial_matrix;
        initial_matrix.resize(4, std::vector<unsigned>(11, 0)); // qos-srcid // the number of src id should be configured
        qos_srcid_matrixs.emplace_back(initial_matrix);
    }
    type_timeout_counter.resize(3, 0);
    type_qos_timeout_counters.resize(3, 0);
    last_win_srcid_s.resize(3, -1);
}
RetryResourceManager::~RetryResourceManager()
{
    #ifdef CHIPort_TEST
    for(auto& matrix: qos_srcid_matrixs)
    {
        for(auto& srcid_vector: matrix)
        {
            for(auto& cnt_value: srcid_vector)
            {
                std::cout << "\t" << cnt_value;
            }
            std::cout << std::endl;
        }
        std::cout << "------------------------------" << std::endl;
    }
    #endif
}
std::optional<unsigned> 
RetryResourceManager::QosSelection(const unsigned& type_index)
{
    assert(!is_type_empty(type_index));
    auto qos_srcid_matrix = qos_srcid_matrixs.at(type_index);
    unsigned& timeout_counter = type_qos_timeout_counters.at(type_index);

    if(timeout_counter == qos_timeout_threshold)
    {
        for(int i = 0; i < qos_srcid_matrix.size(); ++i)
        {
            auto src_id_cnts = qos_srcid_matrix.at(i);
            for(auto& cnts: src_id_cnts)
            {
                if(cnts > 0)
                    return i;
            }
        }
        timeout_counter = 0; // reset the timer to 0
    }
    else{
        for(int i = qos_srcid_matrix.size() - 1; i >= 0; --i)
        {
            assert(i>=0);
            auto src_id_cnts = qos_srcid_matrix.at(i);
            for(auto& cnts: src_id_cnts)
            {
                if(cnts > 0)
                    return i;
            }
        }
    }
    return std::nullopt;
}
std::optional<unsigned> 
RetryResourceManager::SrcIDArbiter(const unsigned& type_index, const unsigned& qos)
{
    assert(type_index < 3 && qos < 4);
    auto src_id_cnts = qos_srcid_matrixs.at(type_index).at(qos);
    auto& last_win_src_id = last_win_srcid_s.at(type_index);
    for(int i=0; i<src_id_cnts.size(); ++i)
    {
        unsigned index = (i + last_win_src_id + 1) % (src_id_cnts.size());
        if(src_id_cnts.at(index)!=0)
        {
            last_win_src_id = index;
            return std::make_optional(index);
        }
    }
    return std::nullopt;
}
void 
RetryResourceManager::cnt_inc(unsigned type, unsigned qos, unsigned src_id)//when retry, counter dec
{
    assert(type < 3 && qos < 4 && src_id < 11);
    assert(qos < qos_srcid_matrixs.at(type).size());
    ++qos_srcid_matrixs.at(type).at(qos).at(src_id);
    #ifdef CHIPort_TEST
    for(auto& matrix: qos_srcid_matrixs)
    {
        for(auto& srcid_vector: matrix)
        {
            for(auto& cnt_value: srcid_vector)
            {
                std::cout << "\t" << cnt_value;
            }
            std::cout << std::endl;
        }
        std::cout << "------------------------------" << std::endl;
    }
    #endif
}

void 
RetryResourceManager::cnt_dec(unsigned type, unsigned qos, unsigned src_id)//when P-credit granted, counter dec
{
    assert(type < 3 && qos < 4 && src_id < 11);
    assert(qos_srcid_matrixs.at(type).at(qos).at(src_id) >= 0);
    --qos_srcid_matrixs.at(type).at(qos).at(src_id);
    assert(qos_srcid_matrixs.at(type).at(qos).at(src_id) < 128 );
    #ifdef CHIPort_TEST
    for(auto& matrix: qos_srcid_matrixs)
    {
        for(auto& srcid_vector: matrix)
        {
            for(auto& cnt_value: srcid_vector)
            {
                std::cout << "\t" << cnt_value;
            }
            std::cout << std::endl;
        }
        std::cout << "------------------------------" << std::endl;
    }
    #endif
}

bool 
RetryResourceManager::is_empty() const
{
    for(int type=0; type<qos_srcid_matrixs.size(); ++type)
    {
        if(!is_type_empty(type))
            return false;
    }
    return true;
}

bool 
RetryResourceManager::is_type_empty(unsigned type) const
{
    assert(type < 3);
    auto srcid_qos_matrix = qos_srcid_matrixs.at(type);
    for(auto src_id_vector: srcid_qos_matrix)
    {
        for(auto srcid_qos_cnt_element : src_id_vector)
        {
            if(srcid_qos_cnt_element > 0)
                return false;
        }
    }
    return true;
}

void 
RetryResourceManager::update_wr_condition()
{
    const unsigned type_index = 0;
    if(!(!is_type_empty(type_index) && CHIPort_ptr->rsp_queue->is_rsp_retry_avail()))
    {
        wr_condition = false;
        return;
    }
    // if(CHIPort_ptr->resource_manage_unit->wr_qos_threshold > 15 && )
    if(!(CHIPort_ptr->resource_manage_unit->get_rm_crq_level() < CHIPort_ptr->cmo_resp_queue->CRQ_SIZE)) // not the right condition
    {
        wr_condition = false;
        return;
    }
    // if(!())
    if(!(CHIPort_ptr->p2c_fifo->size() < CHIPort_ptr->p2c_fifo->P2C_FIFO_SIZE))
    {
        wr_condition = false;
        return;
    }
    // if()
    if(!(CHIPort_ptr->resource_manage_unit->get_rm_dcq_level() < CHIPort_ptr->delay_command_queue->DCQ_INFO_SIZE))
    {
        wr_condition = false;
        return;
    }
    // if(!(CHIPort_ptr->resource_manage_unit->get_rm_dcq_level() == CHIPort_ptr->delay_command_queue->DCQ_INFO_SIZE -1))
    // {
    //     wr_condition = false;
    //     return;
    // }

    if(!(CHIPort_ptr->resource_manage_unit->get_rm_wdq_level() < CHIPort_ptr->wdata_buffer_array->WDAT_BUFFER_SIZE))
    {
        wr_condition = false;
        return;
    }
    // if(!((CHIPort_ptr->resource_manage_unit->wdb_ptr->size() == CHIPort_ptr->resource_manage_unit->wdb_ptr->WDAT_BUFFER_SIZE - 1) && ())))
    // {
    //     wr_condition = false;
    //     return;
    // }
    wr_condition = true;
    return;
}
void 
RetryResourceManager::update_rd_condition()
{
    const unsigned type_index = 1;
    if(!(!is_type_empty(type_index) && CHIPort_ptr->rsp_queue->is_rsp_retry_avail()))
    {
        rd_condition = false;
        return;
    }
    // prority qos
    // if(!())
    if(!(CHIPort_ptr->resource_manage_unit->get_rdat_info_occupancy_total() < CHIPort_ptr->rdata_info_queue->RDATA_INFO_SIZE))
    {
        rd_condition = false;
        return;
    }
    // if((CHIPort_ptr->resource_manage_unit->rdata_ptr->size() == CHIPort_ptr->resource_manage_unit->rdata_ptr->RDATA_INFO_SIZE -1) && )
    // {
    //     rd_condition = false;
    //     return;
    // }
    rd_condition = true;
    return;
}
void 
RetryResourceManager::update_cmo_condition()
{
    const unsigned type_index = 2;
    if(!(!is_type_empty(type_index) && CHIPort_ptr->rsp_queue->is_rsp_retry_avail()))
    {
        cmo_condition = false;
        return;
    }
    if(!(CHIPort_ptr->resource_manage_unit->get_rm_crq_level() < CHIPort_ptr->cmo_resp_queue->CRQ_SIZE))
    {
        cmo_condition = false;
        return;
    }
    // if()
    cmo_condition = true;
    return;
}
void 
RetryResourceManager::update_condition_state()
{
    update_wr_condition();
    update_rd_condition();
    update_cmo_condition();
}

std::optional<unsigned> 
RetryResourceManager::get_type_max_qos(unsigned type_index) const
{
    auto& qos_srcid_matrix = qos_srcid_matrixs.at(type_index);
    for(int i = qos_srcid_matrix.size() -1; i>=0; --i)
    {
        assert(i>=0);
        auto& src_id_vector = qos_srcid_matrix.at(i);
        for(auto& src_id_cnts: src_id_vector)
        {
            if(src_id_cnts > 0)
                return i;
        }
    }
    return std::nullopt;
}

void
RetryResourceManager::state_update()
{
    state = next_state;
    switch(state)
    {
        case State::Write_Grant:
            if(rd_condition && ( (get_type_max_qos(1) > get_type_max_qos(0) && get_type_max_qos(1) > get_type_max_qos(2) && type_timeout_counter.at(2) < req_type_timeout_threshold)
            || (!wr_condition && !cmo_condition) || type_timeout_counter.at(1) >= req_type_timeout_threshold))
                next_state = State::Read_Grant;
            else if(cmo_condition && ((get_type_max_qos(2) > get_type_max_qos(0)) || !wr_condition || type_timeout_counter.at(2) >= req_type_timeout_threshold ) )
                next_state = State::CMO_Grant;
            else
                next_state = State::Write_Grant;
            break;
        case State::Read_Grant:
            if(cmo_condition && ((get_type_max_qos(2) > get_type_max_qos(0) && get_type_max_qos(2) > get_type_max_qos(1) && type_timeout_counter.at(0) < req_type_timeout_threshold))
            || (!wr_condition && !rd_condition) || type_timeout_counter.at(2) >= req_type_timeout_threshold)
                next_state = State::CMO_Grant;
            else if(wr_condition && ((get_type_max_qos(0) > get_type_max_qos(1)) || !rd_condition || type_timeout_counter.at(0) >= req_type_timeout_threshold))
                next_state = State::Write_Grant;
            else
                next_state = State::Read_Grant;
            break;
        case State::CMO_Grant:
            if(wr_condition && ((get_type_max_qos(0) > get_type_max_qos(1) && get_type_max_qos(0) > get_type_max_qos(2) && type_timeout_counter.at(1) < req_type_timeout_threshold))
            || (!rd_condition && !cmo_condition) || type_timeout_counter.at(0) >= req_type_timeout_threshold)
                next_state = State::Write_Grant;
            else if(rd_condition && ((get_type_max_qos(1) > get_type_max_qos(2)) || !cmo_condition || type_timeout_counter.at(1) >= req_type_timeout_threshold))
                next_state = State::Read_Grant;
            else
                next_state = State::CMO_Grant;
            break;
        default:
            break;
    }
}
std::tuple<unsigned, unsigned, unsigned>
RetryResourceManager::gen_pcrd_rsp()
{
    state_update();
    unsigned retry_type = 3;
    if(next_state == State::Write_Grant)
    {
        retry_type = 0;
    }
    else if(next_state == State::Read_Grant)
    {
        retry_type = 1;
    }
    else if(next_state == State::CMO_Grant)
    {
        retry_type = 2;
    }
    else
    {
        std::cerr << "Invalid Retry Type index" << std::endl;
        std::abort();
    }
    auto qos_selected = QosSelection(retry_type).value();
    unsigned src_id_selected = SrcIDArbiter(retry_type, qos_selected).value();
    cnt_dec(retry_type, qos_selected, src_id_selected);
    return std::make_tuple(retry_type, qos_selected, src_id_selected);
}



WdataBufferEntry::WdataBufferEntry(const unsigned data_width_bytes)
{
    memset(data_words, 255, 64);
    this->beat_count = 64 / data_width_bytes; // cache line data width_bytes = 16
}

WdataBufferEntry::WdataBufferEntry(const CHIFlit& req_flit, const unsigned data_width_bytes)
{
    memset(data_words, 255, 64);
    const unsigned size_bytes = 1 << req_flit.payload.size;
    this->beat_count = (size_bytes <= data_width_bytes) ? 1 : size_bytes / data_width_bytes;
}
WdataBufferArray::WdataBufferArray(const unsigned data_width_bytes)
:data_width_bytes(data_width_bytes)
{
    for(uint16_t i=0; i< WDAT_BUFFER_SIZE; ++i)
    {
        unallocated_dbid.insert(i);
    }
}
const uint16_t 
WdataBufferArray::allocate_dbid()
{
    uint16_t id = *unallocated_dbid.begin();
    unallocated_dbid.erase(unallocated_dbid.begin());
    return id;
}
void 
WdataBufferArray::allocate_wdat_buffer_entry(const CHIFlit& req_flit, const unsigned& dbid)
{
    data_buffer.emplace(dbid, WdataBufferEntry(req_flit, this->data_width_bytes));
}

void 
WdataBufferArray::receive_wdat_flit(const CHIFlit& dat_flit)
{
    uint16_t& beat_count_remaining = data_buffer.at(dat_flit.phase.txn_id).beat_count;
    beat_count_remaining--;
    assert(beat_count_remaining >= 0);
    // asset(beat_count_remaining > 4);
    if(beat_count_remaining == 0)
    {
        memcpy((data_buffer.at(dat_flit.phase.txn_id).data_words), dat_flit.payload.data, sizeof(dat_flit.payload.data));
        // return true;
    }
    // return false;
}


RdataInfo::RdataInfo(unsigned config_size)
:RDATA_INFO_SIZE(config_size)
{
    for(uint16_t i=0; i< RDATA_INFO_SIZE; ++i)
    {
        unused_rdata_info_id.insert(i);
    }
}

uint16_t 
RdataInfo::allocate_infotag()
{
    assert(!unused_rdata_info_id.empty());
    uint16_t id = *unused_rdata_info_id.begin();
    unused_rdata_info_id.erase(unused_rdata_info_id.begin());
    return id;
}

void 
RdataInfo::release_infotag(uint16_t id)
{
    unused_rdata_info_id.insert(id); 
}//need to check id is in 0-31


DelayCommandQueue::DelayCommandQueue(WdataBufferArray* wdb_ptr)
: 
wdb_ptr(wdb_ptr)
{
}

void 
DelayCommandQueue::allocate_dcq_buffer_entry(const CHIFlit& req_flit, const unsigned& dbid)
{
    dcq_info_buffer.emplace(dbid, req_flit);
}

std::optional<std::pair<unsigned, CHIFlit>> 
DelayCommandQueue::get_head()
{
    return queue_head_;
}

void 
DelayCommandQueue::Pop()
{
    queue_head_.reset();
    dcq_timeout = false;
    timeout_counter = 0;
    dcq_has_ready = false;
}

void 
DelayCommandQueue::move2head(const unsigned& dbid, const CHIFlit& dcq_flit)
{
    queue_head_.emplace(dbid, dcq_flit);
    dcq_info_buffer.erase(dbid);
    wdb_ptr->allocated_ptl_dbid.erase(dbid);
}

void 
DelayCommandQueue::check_dcq_ready()
{
    dcq_timeout = timeout_counter > config_timeout_num;
    if(dcq_has_ready || queue_head_.has_value())
    {
        timeout_counter = dcq_timeout ? timeout_counter : ++timeout_counter;
        return;
    }
    for(auto& elem: dcq_info_buffer)
    {
        if(wdb_ptr->data_buffer.at(elem.first).is_entry_ready())
        {
            dcq_has_ready = true;
            move2head(elem.first, elem.second);
            return;
        }
    }
}



ResourceManage::
ResourceManage(CHIPort* CHIPort_ptr)
: 
CHIPort_ptr(CHIPort_ptr)
{}

} // namespace dmu