#ifndef __PORT_UTILITIES
#define __PORT_UTILITIES


#include <ARM/TLM/arm_chi_payload.h>
#include <ARM/TLM/arm_chi_phase.h>

namespace dmu{

enum class RetryType: unsigned
{
    Write   = 0,
    Read    = 1,
    CMO     = 2,
    Invalid = 3
};

enum class RespQueueType :std::size_t{
    DBID    = 0x0, // only for WriteNoSnpPartial(DBIDResp) WriteNoSnpFull(CompDBIDResp)
    CRP     = 0x1, // for cmo and other transaction
    Comp    = 0x2, // generate when the dcq entry put into p2c fifo
    REQ     = 0x3, // only for order=1 ReadNoSnp
    Retry   = 0x4, // for RetryAck or PCrdGrant
    Invalid = 0x5
};



inline unsigned req_trans_type_map(const ARM::CHI::Phase& phase)
{
    switch(phase.req_opcode)
    {
        case ARM::CHI::REQ_OPCODE_READ_NO_SNP:
        case ARM::CHI::REQ_OPCODE_READ_NO_SNP_SEP:
            return static_cast<unsigned>(RetryType::Read);
        case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL:
        case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL:
        case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_ZERO:
            return static_cast<unsigned>(RetryType::Write);
        case ARM::CHI::REQ_OPCODE_CLEAN_SHARED_PERSIST:
        case ARM::CHI::REQ_OPCODE_CLEAN_SHARED:
            return static_cast<unsigned>(RetryType::CMO);
        default:
            // SC_REPORT_ERROR(name(), "unexpected request opcode received");
            return static_cast<unsigned>(RetryType::Invalid);
    }
}


static unsigned med_threshold = 7;
static unsigned high_threshold = 11;
static unsigned veryhigh_threshold = 14;
inline unsigned qos_level_map(unsigned qos)
{
    if(qos>=0 && qos < med_threshold)
        return 0;
    else if(qos < high_threshold)
        return 1;
    else if(qos < veryhigh_threshold)
        return 2;
    else
        return 3;
}


inline unsigned retry_type2unsigned(RetryType type)
{
    switch(type)
    {
        case RetryType::Write:
            return 0;
        case RetryType::Read:
            return 1;
        case RetryType::CMO:
            return 2;
        default:
            return 3; // invalid type_index
    }
}

inline RetryType unsigned2retry_type(unsigned type_index)
{
    if(type_index == 0)
        return RetryType::Write;
    else if(type_index == 1)
        return RetryType::Read;
    else if(type_index == 2)
        return RetryType::CMO;
    else
        std::cerr<<"Invalid Retry Type Index"<<std::endl;
        std::abort();
}

} // namespace dmu

#endif