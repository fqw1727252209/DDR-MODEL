#ifndef __PORT_COMMON_HH__
#define __PORT_COMMON_HH__

#include "CHIPort/CHIUtilities.h"
#include "Common/Common.hh"

#include <systemc>
#include <ARM/TLM/arm_chi_phase.h>

namespace dmu{
    namespace Port{

enum class ReadCmdType { HPR, LPR, GPR };
enum class WriteCmdType { TPW, GPW };
enum class CMOCmdType { CMO };

static const size_t READ_CMD_TYPES_NUM = 3; // HPR, LPR, GPR
static const size_t WRITE_CMD_TYPES_NUM = 2; // TPW, GPW
static const size_t CMO_CMD_TYPES_NUM = 1;  // CMO
static const size_t SRC_ID_NUM = 23;

enum class PortCmdType
{
    HPR = 0,
    LPR = 1,
    GPR = 2,
    TPW = 3,
    GPW = 4,
    CMO = 5,
    Invalid = 6
};

enum class RetryType
{
    Write = 0,
    Read = 1,
    CMO = 2,
    Invalid = 3
};

enum class ResponseQueueType
{
    Dbid = 0, // only record dbid
    RetryAck = 1, // record retry ack response
    PcrdGrant = 2, // record pcrd grant response
    ReadReceipt_Comp = 3, // record read receipt when read hit prefetch when prefetch trans is sent to ddrc happend and record cmo comp response
    ReadReceipt = 4, // record read receipt when read trans accept while field order== 1, and record read receipt when read trans hit prefetch and this prefetch trans is sending to ddrc this cycle
    Comp = 5, // record comp response
    Invalid = 6
};

constexpr std::array<const char*, static_cast<int>(ResponseQueueType::Invalid) + 1> ResponseQueueTypeToString = {{
    "Dbid",             // Dbid = 0
    "RetryAck",         // RetryAck = 1
    "PcrdGrant",        // PcrdGrant = 2
    "ReadReceipt_Comp", // ReadReceipt_Comp = 3
    "ReadReceipt",      // ReadReceipt = 4
    "Comp",             // Comp = 5
    "Invalid"           // Invalid = 6
}};

// 辅助函数，将ResponseQueueType转换为字符串
inline std::string to_string(ResponseQueueType type) {
    auto index = static_cast<int>(type);
    if (index >= 0 && index < static_cast<int>(ResponseQueueType::Invalid) + 1) {
        return std::string(ResponseQueueTypeToString[static_cast<size_t>(index)]);
    }
    return "Unknown";
}



static unsigned RequestTypeMap(const ARM::CHI::Phase& phase)
{
    switch (phase.req_opcode)
    {
    case ARM::CHI::REQ_OPCODE_READ_NO_SNP:
    case ARM::CHI::REQ_OPCODE_READ_NO_SNP_SEP:
        return static_cast<unsigned>(RetryType::Read);
    case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL:
    case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL:
    case ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_ZERO:
        return static_cast<unsigned>(RetryType::Write);
    case ARM::CHI::REQ_OPCODE_CLEAN_SHARED:
    case ARM::CHI::REQ_OPCODE_CLEAN_SHARED_PERSIST:
        return static_cast<unsigned>(RetryType::CMO);
    default:
        SC_REPORT_ERROR("Request Type Map", "Unknown Request Type");
    }
    return static_cast<unsigned>(RetryType::Invalid);
}

static const unsigned MED_THRESHOLD = 7;
static const unsigned HIGH_THRESHOLD = 11;
static const unsigned HIGH_HIGH_THRESHOLD = 14;
static const unsigned QOS_MAX_VALUE = 15;

inline unsigned RetryTypeMap(RetryType type)
{
    switch (type)
    {
    case RetryType::Read:
        return 0;
    case RetryType::Write:
        return 1;
    case RetryType::CMO:
        return 2;
    default:
        SC_REPORT_ERROR("Retry Type Map", "Unknown Retry Type");
    }
    return static_cast<unsigned>(RetryType::Invalid);
}

inline RetryType Map2RetryType(const unsigned& type_index)
{
    if(type_index == 0)
        return RetryType::Read;
    else if(type_index == 1)
        return RetryType::Write;
    else if(type_index == 2)
        return RetryType::CMO;
    else
        SC_REPORT_ERROR("Map to Retry Type", "Invalid Type Index");
    
    return RetryType::Invalid;
}







    } // namespace Port
} // namespace dmu



#endif