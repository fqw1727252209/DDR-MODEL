#ifndef __COMMON_HH__
#define __COMMON_HH__

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cstdint>

#include <systemc>
#include <tlm>

#include "sysc/kernel/sc_time.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_phase.h"

namespace dmu {

// phase declare
DECLARE_EXTENDED_PHASE(UIF_REQ);
DECLARE_EXTENDED_PHASE(UIF_CREDIT);
DECLARE_EXTENDED_PHASE(UIF_WDAT_REQ);
DECLARE_EXTENDED_PHASE(UIF_WDAT_BEGIN);
DECLARE_EXTENDED_PHASE(UIF_WDAT_END);
DECLARE_EXTENDED_PHASE(WR_RESPONSE_COMPLETE);
DECLARE_EXTENDED_PHASE(UIF_RDAT_BEGIN);
DECLARE_EXTENDED_PHASE(UIF_RDAT_END);

enum class PriorityClass{
    HPR,
    LPR,
    GPR,
    TPW,
    GPW,
    Invalid
};
// 辅助函数: 将PriorityClass枚举转换为字符串 (使用数组方式)
inline std::string toString(const PriorityClass& priorityClass) {
    static constexpr std::array<std::string_view, 6> priorityStrings = {
        "HPR", "LPR", "GPR", "TPW", "GPW", "Invalid"
    };

    size_t index = static_cast<size_t>(priorityClass);
    if (index < priorityStrings.size()) {
        return std::string(priorityStrings[index]);
    }
    return "Unknown";
}

enum class CmdType{
    RD,
    WR,
    MWR, // not used
    RMW,
    Invalid
};

class Qos
{
public:
    /*
    below rd level 0 is LPR, above rd level 1 is HPR, between two levels is GPR
    below wr level is TPW, above is GPW
    */
private:
    uint8_t _qos_value;
    PriorityClass _qos_priority;

    inline static uint8_t RD_QOS_LEVEL_0 = 8;
    inline static uint8_t RD_QOS_LEVEL_1 = 14;// above level 1 is HPR
    inline static uint8_t WR_QOS_LEVEL = 12;

public:
    explicit Qos(uint8_t qos_value, bool is_rd): _qos_value(qos_value)
    {
        if(is_rd)
        {
            if(qos_value < RD_QOS_LEVEL_0)
            {
                _qos_priority = PriorityClass::GPR;
            }
            if(qos_value >= RD_QOS_LEVEL_0 && qos_value < RD_QOS_LEVEL_1)
            {
                _qos_priority = PriorityClass::LPR;
            }
            if(qos_value >= RD_QOS_LEVEL_1)
            {
                _qos_priority = PriorityClass::HPR;
            }
        }
        else
        {
            if(qos_value < WR_QOS_LEVEL)
            {
                _qos_priority = PriorityClass::TPW;
            }
            if(qos_value >= WR_QOS_LEVEL)
            {
                _qos_priority = PriorityClass::GPW;
            }
        }
        assert(qos_value >= 0 && qos_value <= 15);
    }

    Qos():_qos_value(0),_qos_priority(PriorityClass::Invalid){}
    inline uint8_t GetQosValue() const{ return _qos_value;}
    inline PriorityClass GetQosLevel() const{ return _qos_priority;}
    static void ConfigureQosMap(uint8_t rd_qos_level_0 = 8, uint8_t rd_qos_level_1 = 14, uint8_t wr_qos_level = 12)
    {
        RD_QOS_LEVEL_0 = rd_qos_level_0;
        RD_QOS_LEVEL_1 = rd_qos_level_1;
        WR_QOS_LEVEL = wr_qos_level;
    }
};

enum class DramCommand{
    ACT,
    RD,
    WR,
    RDA,
    WRA,
    REFab,
    REFsb,
    RFMab,
    RFMsb,
    PREab,
    PREsb,
    PRE,
    Invalid
};

// 将DramCommand枚举转换为字符串的函数
inline std::string to_string(DramCommand cmd) {
    constexpr std::array<std::string_view, static_cast<size_t>(DramCommand::Invalid)+1> commandStrings = {
        "ACT",      // 0
        "RD",       // 1
        "WR",       // 2
        "RDA",      // 3
        "WRA",      // 4
        "REFab",    // 5
        "REFsb",    // 6
        "RFMab",    // 7
        "RFMsb",    // 8
        "PREab",    // 9
        "PREsb",    // 10
        "PREpb",    // 11
        "Invalid"   // 12
    };

    const auto index = static_cast<std::underlying_type_t<DramCommand>>(cmd);
    if (index < commandStrings.size()) {
        return std::string(commandStrings[index]);
    }
    std::cerr << "Invalid DramCommand value: " << static_cast<int>(cmd) << std::endl;
    std::abort();
    return "Unknown";
}

}

#endif