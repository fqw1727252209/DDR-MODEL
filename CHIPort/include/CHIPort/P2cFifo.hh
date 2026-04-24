#ifndef __P2C_FIFO_HH__
#define __P2C_FIFO_HH__

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <stdexcept>
#include <optional>

#include <systemc>
#include <tlm>
#include <utility>

#include "ARM/TLM/arm_chi_payload.h"
#include "ARM/TLM/arm_chi_phase.h"
#include "CHIPort/CHIUtilities.h"
#include "CHIPort/RdDataInfo.hh"
#include "CHIPort/WdataBufferArray.hh"
#include "CHIPort/PortCommon.hh"
#include "Common/CommonDefine.hh"
#include "Common/StatisticExtension.hh"
#include "Configure/Configure.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/utils/sc_report.h"
// #include "Controller/common/Configure.hh"

namespace dmu{
    namespace Port{

struct QueueEntry{
    explicit QueueEntry(tlm::tlm_generic_payload* trans, sc_core::sc_time time, ARM::CHI::Phase phase, ARM::CHI::Payload& payload, PriorityClass qos_level):
        trans(trans), expired_time(time), phase(phase), payload(payload), qos_level(qos_level)
    {
        payload.ref();
    }
    explicit QueueEntry(tlm::tlm_generic_payload* trans, sc_core::sc_time time, const CHIFlit& flit, PriorityClass qos_level):
        trans(trans), expired_time(time), phase(flit.phase), payload(flit.payload), qos_level(qos_level)
    {
        payload.ref();
    }

    QueueEntry(const QueueEntry& other):
        expired_time(other.expired_time), phase(other.phase), payload(other.payload),trans(other.trans),qos_level(other.qos_level), is_rmw(other.is_rmw)
    {
        payload.ref();
    }

    ~QueueEntry(){
        payload.unref();
    }

    inline void DecideRmw(){
        is_rmw = (payload.byte_enable != ~uint64_t(0) && trans->is_write() && phase.req_opcode == ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL);
        // is_rmw = false;
    }
    sc_core::sc_time expired_time;
    bool is_rmw{false};
    const PriorityClass qos_level;
    tlm::tlm_generic_payload* trans;
    ARM::CHI::Phase phase;
    ARM::CHI::Payload& payload;
};

class RequestQueueIf {
protected:
    sc_core::sc_time aging_expired_time{sc_core::sc_max_time()};
    unsigned credit_counter{0};
    const size_t max_queue_depth;
    const sc_core::sc_time expired_threshold;
public:
    virtual bool HasRequest() const = 0;
    virtual bool IsQueueEmpty() const = 0;
    virtual bool IsQueueExpired() const {return aging_expired_time <= sc_core::sc_time_stamp(); }
    virtual void UpdateQueueAgingTime(){
        aging_expired_time = sc_core::sc_time_stamp() + expired_threshold;
    }
    virtual bool HasCredit() const { return credit_counter > 0; }
    virtual unsigned GetCredit() const {return credit_counter; }
    virtual inline void ReceiveCredit() { credit_counter++; }
    virtual inline void ConsumeCredit() { assert(credit_counter > 0); credit_counter--; }
    virtual inline unsigned GetMaxQueueDepth() const { return max_queue_depth; }
    virtual bool IsQueueFull() const = 0;
    virtual bool IsQueueLocked() const = 0;

    //virtual void InsertRequest(QueueEntry element) = 0;
    //virtual const QueueEntry& GetRequest() = 0;

    virtual void PopRequest() = 0;
    virtual std::size_t GetQueueSize() const = 0;
    virtual ~RequestQueueIf() = default;
    RequestQueueIf(size_t max_queue_depth,const sc_core::sc_time& expired_threshold):max_queue_depth(max_queue_depth), expired_threshold(expired_threshold){};
};

// should build 3 request queue, all the queue is a fifo/deque

// hpr is high level: hpr queue is aging
// lpr is high level: 1.lpr queue is aging
//                 or 2. exits timeout gpr cmd in lpr queue
// tpw is high level: 1.tpw queue is aging
//                 or 2. exits timeout gpw cmd in tpw queue
//


// state info signal: gpr_go2_critical: 1.gpr timeout but no lpr credit
//                                   or 2.gpw timeout and gpw is rmw request,but no lpr credit
//                   gpw_go2_critical: 1.gpw timeout but no tpw credit

// pa switch fast --> this is a configure param
// if true: rd hold:hpr or lpr queue has credit and request for uif
//          wr hold:tpw queue has credit and request for uif
// if false: rd hold: rd queue has credit and request for uif
//           wr hold: wr queue has credit

// state machine:
//  IDLE    <--------->    RD
//  <-->        WR      <-->
// IDLE:
// if(rd queue has req and credit) --> switch to RD
// else if(wr queue has req and credit) -> switch to WR
// else -> keep IDLE

// RD:
// if(rd queue has high priority and credit) -> keep RD
// else if(wr queue has high priority and credit) -> switch to WR
// else if(rd hold) --> keep RD
// else if(wr queue has req and credit) --> switch to WR
// else --> switch to IDLE

// WR:
// if(wr queue has high priority and credit) -> keep WR
// else if(rd queue has high priority and credit || hpr has req and credit) --> Switch to RD
// else if(wr hold) --> keep WR
// else if(lpr queue has req and credit) -> switch to RD
// else -> switch to IDLE

// Note:
// rmw request will not considered be request if lpr has no credit
// rmw will not call the credit decrease 1

template<typename T>
class LprQueue : public RequestQueueIf {
private:
    std::deque<T> request_queue;
    const RdDataInfo& rd_info;
public:
    explicit LprQueue(size_t max_queue_depth, const sc_core::sc_time& expired_threshold, unsigned min_buffer_size, const RdDataInfo& rd_info) 
    : RequestQueueIf(max_queue_depth, expired_threshold)
    , min_buffer_size(min_buffer_size)
    , rd_info(rd_info)
    {}
    bool HasRequest() const override { return !request_queue.empty() && !rd_info.IsFull(); }
    bool IsQueueEmpty() const override { return request_queue.empty(); }

    bool IsQueueFull() const override { return request_queue.size() >= max_queue_depth; }
    bool IsQueueLocked() const override { return false;}
    void InsertRequest(T element) { request_queue.emplace_back(element); }
    const T& GetRequest() { return request_queue.front(); }
    void PopRequest() override { request_queue.pop_front(); }
    std::size_t GetQueueSize() const override { return request_queue.size(); }
    
    // used for gpr command
    bool HasExpiredCmd() const {
        for(auto& entry: request_queue) 
        {
            if(entry.qos_level == PriorityClass::GPR && entry.expired_time >= sc_core::sc_time_stamp()) 
                return true;
        }
        return false;
    }
private:
    const unsigned min_buffer_size;
};

template<typename T>
class HprQueue : public RequestQueueIf {
private:
    std::deque<T> request_queue;
    const RdDataInfo& rd_info;
public:
    explicit HprQueue(size_t max_queue_depth, const sc_core::sc_time& expired_threshold, unsigned min_buffer_size, const RdDataInfo& rd_info) 
    :   RequestQueueIf(max_queue_depth, expired_threshold)
    ,   min_buffer_size(min_buffer_size)
    ,   rd_info(rd_info)
    {}
    bool HasRequest() const override { return !request_queue.empty() && !rd_info.IsFull(); }
    bool IsQueueEmpty() const override { return request_queue.empty(); }
    
    bool IsQueueFull() const override { return request_queue.size() >= max_queue_depth; }
    bool IsQueueLocked() const override { return false; }
    void InsertRequest(T element) { request_queue.emplace_back(element); }
    const T& GetRequest() {return request_queue.front();}
    void PopRequest() override { request_queue.pop_front(); }
    std::size_t GetQueueSize() const override { return request_queue.size(); }

private:
    const unsigned min_buffer_size;
};

class WdataBufferArray;
template<typename T>
class TpwQueue : public RequestQueueIf {
private:
    std::map<unsigned,T> request_queue;
    std::optional<std::pair<unsigned,T>> head_entry;
    const WdataBufferArray& wdata_buffer;
public:
    explicit TpwQueue(size_t max_queue_depth, const sc_core::sc_time& expired_threshold, const WdataBufferArray& wdata_buffer) 
    :   RequestQueueIf(max_queue_depth, expired_threshold)
    ,   wdata_buffer(wdata_buffer)
    {}

    ~TpwQueue() override{
        DPRINT_INFO(false,"Port Tpw Queue","remain size:%ld, remain credit:%d, head has value:%s, queue head is rmw:%s",
            request_queue.size(),
            this->GetCredit()
            ,!this->IsHeadEmpty() ? "true" : "false"
        ,!this->IsHeadEmpty()&&IsRMWRequest() ? "true" : "false"
    );
    }
    bool HasRequest() const override { return !IsQueueEmpty() && !IsHeadEmpty();}
    bool IsQueueEmpty() const override { return request_queue.empty() && !head_entry.has_value(); }

    bool IsQueueFull() const override { return GetQueueSize() >= max_queue_depth; }
    bool IsQueueLocked() const override { return false; }
    void InsertRequest(T element, unsigned dbid) {request_queue.emplace(dbid,element);}

    bool IsRMWRequest() const {return head_entry.value().second.is_rmw;}
    const std::pair<unsigned, T>& GetRequest() {return head_entry.value();}

    void PopRequest() override {
        head_entry.reset();
        Move2Head();
    }
    bool IsHeadEmpty() const {return !head_entry.has_value();}
    
    void Move2Head() 
    {
        assert(!head_entry.has_value());
        for(auto iter = request_queue.begin();iter != request_queue.end();iter++)
        {
            if(wdata_buffer.IsEntryReady(iter->first)) 
            {
                iter->second.DecideRmw();
                head_entry.emplace(iter->first,iter->second);
                request_queue.erase(iter);
                return;
            }
        }
    }

size_t GetQueueSize() const override { return request_queue.size() + ((head_entry.has_value()) ? 1 : 0); }

    // used for gpr command
    bool HasExpiredCmd() const {
        for(auto& entry: request_queue)
        {
            if(entry.second.qos_level == PriorityClass::GPW && entry.second.expired_time >= sc_core::sc_time_stamp() )
                return true;
        }
        return false;
    }
};




enum QueueType{LPR,HPR,TPW,NONE};
class PaArbiter
{
public:
    enum State{IDLE,RD,WR} state{IDLE};
    explicit PaArbiter(const LprQueue<QueueEntry>& lpr_queue, const HprQueue<QueueEntry>& hpr_queue, const TpwQueue<QueueEntry>& tpw_queue,bool switch_fast)
      : lpr_queue(lpr_queue)
      , hpr_queue(hpr_queue)
      , tpw_queue(tpw_queue)
      , switch_fast(switch_fast)
    {}
private:
    const LprQueue<QueueEntry>& lpr_queue;
    const HprQueue<QueueEntry>& hpr_queue;
    const TpwQueue<QueueEntry>& tpw_queue;
    const bool switch_fast;

    bool IsRdHold() const; // Is Rd in hold state
    bool IsWrHold() const; // Is Wr in hold state
    bool IsHprHighPriority() {return hpr_queue.IsQueueExpired() && hpr_queue.HasCredit() && hpr_queue.HasRequest();}
    bool IsLprHighPriority() {return (lpr_queue.IsQueueExpired() || lpr_queue.HasExpiredCmd()) && lpr_queue.HasCredit() && lpr_queue.HasRequest();}
    bool IsTpwHighPriority() {
        return (tpw_queue.IsQueueExpired()|| tpw_queue.HasExpiredCmd()) && tpw_queue.HasCredit() && tpw_queue.HasRequest();
    }
    bool IsRdHighPriority(){
        return (IsHprHighPriority() || IsLprHighPriority());
    }

    bool IsWrHighPriority(){
        return (IsTpwHighPriority());
    }
    bool HasRdRequest() {
        return (lpr_queue.HasRequest() && lpr_queue.HasCredit()) || (hpr_queue.HasRequest() && hpr_queue.HasCredit());
    }
    bool HasWrRequest() {
        // tpw队列有请求，有信用，并且 tpw的请求不是刚好是"rmw请求但此时 lpr队列没有信用"
        return tpw_queue.HasRequest() && tpw_queue.HasCredit() && (!(tpw_queue.IsRMWRequest() && !lpr_queue.HasCredit()));
    }
public:
    void UpdateState();
    QueueType GetWinningQueue();
    bool IsGprCritical() const;
    bool IsGpwCritical() const;
};



class P2cFifo
{
public:
    // explicit P2cFifo(const Configure& configure);
    explicit P2cFifo(const Configure& configure,const WdataBufferArray& wdata_buffer_array, const RdDataInfo& rd_data_info,const sc_core::sc_time port_clock_period);

public:
    std::unique_ptr<LprQueue<QueueEntry>> lpr_queue;
    std::unique_ptr<HprQueue<QueueEntry>> hpr_queue;
    std::unique_ptr<TpwQueue<QueueEntry>> tpw_queue;

    std::unique_ptr<PaArbiter> pa_arbiter;

    const unsigned lgpr_min_depth;
    const unsigned hpr_min_depth;

    const unsigned Rd_queue_depth;
    const unsigned Wr_queue_depth;

    const sc_core::sc_time queue_expired_threshold;
    const bool switch_fast;

public:
    QueueType GetWinningQueue() {
        pa_arbiter->UpdateState();
        return pa_arbiter->GetWinningQueue();
    }

    bool IsTpwQueueHeadEmpty() const { return tpw_queue->IsHeadEmpty(); }
    void MoveTpwQueue2Head() { tpw_queue->Move2Head(); }
    inline bool IsLprQueueFull() const { return lpr_queue->IsQueueFull(); }
    inline bool IsHprQueueFull() const { return hpr_queue->IsQueueFull(); }
    inline bool IsTpwQueueFull() const { return tpw_queue->IsQueueFull(); }
    bool IsQueueEmpty() const {return !lpr_queue->HasRequest() && !hpr_queue->HasRequest() && !tpw_queue->HasRequest(); }

    unsigned GetRdQueueSize() const { return lpr_queue->GetQueueSize() + hpr_queue->GetQueueSize(); }
    bool IsRdQueueRemainOneSpace() const { return GetRdQueueSize() == Rd_queue_depth - 1; }

    void InsertHprRequest(const CHIFlit& req_flit, tlm::tlm_generic_payload* trans,PriorityClass qos_level)
    {
        sc_core::sc_time expired_time = sc_core::sc_max_time();//sc_core::sc_time_stamp() + sc_core::sc_time(100, sc_core::SC_NS);
        trans->get_extension<StatisticExtension>()->RecordInPortTime(req_flit.m_entering_port_time);
        hpr_queue->InsertRequest(QueueEntry(trans,expired_time,req_flit,qos_level));
    }
    
    void InsertLGprRequest(const CHIFlit& req_flit, tlm::tlm_generic_payload* trans,PriorityClass qos_level)
    {
        trans->get_extension<StatisticExtension>()->RecordInPortTime(req_flit.m_entering_port_time);
        sc_core::sc_time expired_time = sc_core::sc_max_time();//sc_core::sc_time_stamp() + sc_core::sc_time(100, sc_core::SC_NS);
        lpr_queue->InsertRequest(QueueEntry(trans,expired_time,req_flit,qos_level));
    }
    
    void InsertTpwRequest(const CHIFlit& req_flit, tlm::tlm_generic_payload* trans, unsigned dbid,PriorityClass qos_level)
    {
        trans->get_extension<StatisticExtension>()->RecordInPortTime(req_flit.m_entering_port_time);
        sc_core::sc_time expired_time = sc_core::sc_max_time();//sc_core::sc_time_stamp() + sc_core::sc_time(100, sc_core::SC_NS);
        tpw_queue->InsertRequest(QueueEntry(trans,expired_time,req_flit,qos_level),dbid);
    }

    const QueueEntry& GetRdFrontRequest(QueueType queue_type)
    {
        if(queue_type == QueueType::LPR)
            return lpr_queue->GetRequest();
        else if(queue_type == QueueType::HPR)
            return hpr_queue->GetRequest();
        else
            SC_REPORT_ERROR("CHIPort p2c fifo get rd request", "Invalid queue type");
            
        std::abort();
    }

    const std::pair<unsigned,QueueEntry>& GetWrFrontRequest()
    {
        return tpw_queue->GetRequest();
    }

    void PopRequest(QueueType queue_type)
    {
        if(queue_type == QueueType::LPR)
            lpr_queue->PopRequest();
        else if(queue_type == QueueType::HPR)
            hpr_queue->PopRequest();
        else if(queue_type == QueueType::TPW)
            tpw_queue->PopRequest();
        else
            SC_REPORT_ERROR("CHIPort p2c fifo pop request", "Invalid queue type");
    }

    void CreditDecrese(QueueType queue_type)
    {
        if(queue_type == QueueType::LPR)
            lpr_queue->ConsumeCredit();
        else if(queue_type == QueueType::HPR)
            hpr_queue->ConsumeCredit();
        else if(queue_type == QueueType::TPW)
            tpw_queue->ConsumeCredit();
        else
            SC_REPORT_ERROR("CHIPort p2c fifo credit decrease", "Invalid queue type");
    }

    void UpdateQueueAging(QueueType queue_type)
    {
        if(queue_type == QueueType::LPR)
            lpr_queue->UpdateQueueAgingTime();
        else if(queue_type == QueueType::HPR)
            hpr_queue->UpdateQueueAgingTime();
        else if(queue_type == QueueType::TPW)
            tpw_queue->UpdateQueueAgingTime();
        else
            SC_REPORT_ERROR("CHIPort p2c fifo update queue aging", "Invalid queue type");

    }

};




    } // namespace Port
} // namespace dmu

#endif