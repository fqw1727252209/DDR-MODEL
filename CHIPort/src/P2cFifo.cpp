#include "CHIPort/P2cFifo.hh"
#include "CHIPort/RdDataInfo.hh"
#include "CHIPort/WdataBufferArray.hh"
#include "sysc/utils/sc_report.h"
// #include "Controller/common/Configure.hh"
#include <memory>

namespace dmu{
    namespace Port{

bool
PaArbiter::IsRdHold() const
{
    if(switch_fast)
    {
        return (lpr_queue.HasRequest() && lpr_queue.HasCredit()) || (hpr_queue.HasRequest() && hpr_queue.HasCredit());
    }
    else {
        return lpr_queue.HasCredit() || hpr_queue.HasCredit();
    }
}
bool
PaArbiter::IsWrHold() const{
    if(switch_fast)
    {
        return tpw_queue.HasRequest() && tpw_queue.HasCredit();
    }
    else {
        return tpw_queue.HasCredit();
    }
}

void
PaArbiter::UpdateState()
{
    switch (state) {
        case IDLE:
            if (HasRdRequest())
                state = RD;
            else if (HasWrRequest())
                state = WR;
            else
                state = IDLE;
            break;

        case RD:
            if (IsRdHighPriority())
                state = RD;
            else if (IsWrHighPriority())
                state = WR;
            else if (IsRdHold())
                state = RD;
            else if (HasWrRequest())
                state = WR;
            else
                state = IDLE;
            break;

        case WR:
            if (IsWrHighPriority())
                state = WR;
            else if ((IsRdHighPriority()) || (hpr_queue.HasRequest() && hpr_queue.HasCredit()))
                state = RD;
            else if (IsWrHold())
                state = WR;
            else if (lpr_queue.HasRequest() && lpr_queue.HasCredit())
                state = RD;
            else
                state = IDLE;
            break;
        default:
            SC_REPORT_ERROR("Pa arbiter", "Invalid state");
    }
}

QueueType
PaArbiter::GetWinningQueue() {
    if (state == RD)
    {
        if(IsHprHighPriority() && hpr_queue.HasCredit())
            return HPR;
        else if(IsLprHighPriority() && lpr_queue.HasCredit())
            return LPR;
        else if(hpr_queue.HasRequest() && hpr_queue.HasCredit() && hpr_queue.IsQueueLocked())
            return HPR;
        else if(lpr_queue.HasRequest() && lpr_queue.HasCredit() && lpr_queue.IsQueueLocked())
            return LPR;
        else if(hpr_queue.HasRequest() && hpr_queue.HasCredit())
            return HPR;
        else if(lpr_queue.HasRequest() && lpr_queue.HasCredit())
            return LPR;
        else
            return NONE;
    }
    else if(state == WR)
    {
        if(tpw_queue.HasRequest() && tpw_queue.HasCredit() && tpw_queue.IsQueueLocked())
            return TPW;
        else if(tpw_queue.HasRequest() && tpw_queue.HasCredit())
            return TPW;
        else
            return TPW;
    }
    else
        return NONE;
}

bool
PaArbiter::IsGprCritical() const{
    if( (lpr_queue.HasExpiredCmd() && !lpr_queue.HasCredit()) ||
        (tpw_queue.HasExpiredCmd() && !lpr_queue.HasCredit()) )
    {
        return true;
    }
    else {
        return false;
    }
}

bool
PaArbiter::IsGpwCritical() const {
    if(tpw_queue.HasExpiredCmd() && !tpw_queue.HasCredit())
    {
        return true;
    }
    else {
        return false;
    }
}



P2cFifo::P2cFifo(const Configure& configure, const WdataBufferArray& wdata_buffer_array, const RdDataInfo& rd_data_info, const sc_core::sc_time port_clock_period)
: Rd_queue_depth(configure.controller_config->RD_CQ_DEPTH)
, Wr_queue_depth(configure.controller_config->WR_CQ_DEPTH)
, hpr_min_depth(configure.controller_config->MIN_HPR_QUEUE_DEPTH)
, lgpr_min_depth(configure.controller_config->MIN_LGPR_QUEUE_DEPTH)
, queue_expired_threshold(configure.controller_config->PORT_AGING_INIT * port_clock_period)//
, switch_fast(configure.controller_config->PA_RDWR_SWITCH_FAST)
{
    hpr_queue = std::make_unique<HprQueue<QueueEntry>>(Rd_queue_depth - hpr_min_depth,queue_expired_threshold,hpr_min_depth,rd_data_info);

    lpr_queue = std::make_unique<LprQueue<QueueEntry>>(Rd_queue_depth - lgpr_min_depth,queue_expired_threshold,lgpr_min_depth,rd_data_info);

    tpw_queue = std::make_unique<TpwQueue<QueueEntry>>(Wr_queue_depth, queue_expired_threshold,wdata_buffer_array);

    pa_arbiter = std::make_unique<PaArbiter>(*lpr_queue.get(), *hpr_queue.get(), *tpw_queue.get(),switch_fast);
}




    }
}