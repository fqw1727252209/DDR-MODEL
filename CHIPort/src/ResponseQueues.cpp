#include "CHIPort/ResponseQueues.hh"
#include "CHIPort/PortCommon.hh"
#include "Configure/Configure.hh"
#include "sysc/utils/sc_report.h"

namespace dmu{
    namespace Port{

ResponseQueues::ResponseQueues(const Configure& configure)
: Max_Dbid_Queue_size(configure.controller_config->RSP0_FIFO_DEPTH)
, Max_RetryAck_Queue_size(configure.controller_config->RSP1_FIFO_DEPTH)
, Max_PcrdGrant_Queue_size(configure.controller_config->RSP2_FIFO_DEPTH)
, Max_ReadReceipt_Comp_Queue_size(configure.controller_config->RSP3_FIFO_DEPTH)
, Max_ReadReceipt_Queue_size(configure.controller_config->RSP4_FIFO_DEPTH)
, Max_Comp_Queue_size(configure.controller_config->RSP5_FIFO_DEPTH)
{
    response_queues.reserve(Queue_Type_Num);
}

bool
ResponseQueues::IsResponseQueueFull(const ResponseQueueType& type)
{
    if(type == ResponseQueueType::Dbid)
    {
        return response_queues[static_cast<unsigned>(type)].size() >= Max_Dbid_Queue_size;
    }
    else if(type == ResponseQueueType::RetryAck)
    {
        return response_queues[static_cast<unsigned>(type)].size() >= Max_RetryAck_Queue_size;
    }
    else if(type == ResponseQueueType::PcrdGrant)
    {
        return response_queues[static_cast<unsigned>(type)].size() >= Max_PcrdGrant_Queue_size;
    }
    else if(type == ResponseQueueType::ReadReceipt_Comp)
    {
        return response_queues[static_cast<unsigned>(type)].size() >= Max_ReadReceipt_Comp_Queue_size;
    }
    else if(type == ResponseQueueType::ReadReceipt)
    {
        return response_queues[static_cast<unsigned>(type)].size() >= Max_ReadReceipt_Queue_size;
    }
    else if(type == ResponseQueueType::Comp)
    {
        return response_queues[static_cast<unsigned>(type)].size() >= Max_Comp_Queue_size;
    }
    else
    {
        SC_REPORT_ERROR("Class ResponseQueues", "IsResponseQueueFull get invalid queue type");
        return false;
    }
}

    } //
} //