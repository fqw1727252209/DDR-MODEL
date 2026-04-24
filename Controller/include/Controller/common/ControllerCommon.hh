#ifndef __CONTROLLER_COMMON_HH__
#define __CONTROLLER_COMMON_HH__

#include "Common/Common.hh"
#include "Configure/AddressDecoder.hh"

namespace dmu{
    namespace Controller{

DECLARE_EXTENDED_PHASE(DFI_CMD);
DECLARE_EXTENDED_PHASE(DFI_WDAT_BEGIN);
DECLARE_EXTENDED_PHASE(DFI_WDAT_END);
DECLARE_EXTENDED_PHASE(DFI_RDAT_BEGIN);
DECLARE_EXTENDED_PHASE(DFI_RDAT_END);

enum class GlobalRdWrState
{
    Rd, // rd row cmd allowed, rd col cmd allowed;
    Wr, // wr row cmd allowed, wr col cmd allowed;
    // RdPending,// in wr mode, but has rd cmd need to be sent
    // WrPending,// in rd mode, but has wr cmd need to be sent
    Rd2Wr, //rd col cmd allowed, rd row cmd not allowed
    Wr2Rd, //wr col cmd allowed, wr row cmd not allowed
    Invalid
};

inline std::string PrintState(GlobalRdWrState state)
{
    if(state == GlobalRdWrState::Rd)
        return "Rd";
    else if(state == GlobalRdWrState::Wr)
        return "Wr";
    else if(state == GlobalRdWrState::Rd2Wr)
        return "Rd2Wr";
    else if(state == GlobalRdWrState::Wr2Rd)
        return "Wr2Rd";
    else
        return "Invalid";
}


enum class AddrCollisionType{
    No_Collision,
    RAW, // cause write flush
    RARMW, // if rd exsits, first cause rd flush until rd cmd granted, then cause write flush until write cmd granted
    WAW,
    WARMW,
    WAR,
    RMWARMW,
    RMWAW,
    RMWAR,
    Invalid
};

static std::array<std::string, static_cast<size_t>(AddrCollisionType::Invalid)+1> AddrCollisionTypeStr = {
    "No_Collision",
    "RAW",
    "RARMW",
    "WAW",
    "WARMW",
    "WAR",
    "RMWARMW",
    "RMWAW",
    "RMWAR",
    "Invalid"
};



// this class record the middle stage update cam index,
// should be based on bsc level, if one bsc do some action, then need to update the bsc's ntt,
// rd and wr are both managered seperately
// why need the NttTemp?
// as in RTL design, the update stage is splited into 2-stage,
// first, updated event and get the updated and matchest cam index,
// second, update the matchest cam index into ntt;
enum class UpdateType: uint8_t{
    CmdExe = 0, // for 
    Pre_Act, // 
    NewCmdStore,
    BscAllocate,
    BrokenTerminate,
    WrCombine, // only write has
    Invalid
};

static std::array<std::string, static_cast<size_t>(UpdateType::Invalid)+1> UpdateTypeStr = {
    "CmdExe",
    "Pre_Act",
    "NewCmdStore",
    "BscAllocate",
    "BrokenTerminate",
    "WrCombine",
    "Invalid"
};

class BankAddress{
public:
    unsigned bank;
    unsigned bankgroup;
    unsigned cid;
    unsigned cs;
    unsigned ch;
    unsigned real_ba;
    unsigned real_bg;
    unsigned real_cid;

    BankAddress();

    BankAddress(unsigned ch, unsigned cs, unsigned cid, unsigned real_cid);

    BankAddress(const DecodedAddress& sdram_addr);

    void SetBankAddrees(const DecodedAddress& sdram_addr);

    void ResetBankAddress();

    bool is_rank_address{false};

    friend std::ostream& operator<<(std::ostream& os, const BankAddress& ba);

};

// struct UifSideBandInfo
// {
//     //Port -> DDRC
//     bool uif_gpr_go2critical{false};
//     bool uif_gpw_go2critical{false};
//     //DDRC -> Port
//     unsigned tpw_credit{0};
//     unsigned lpr_credit{0};
//     unsigned hpr_credit{0};

//     bool tpw_credit_valid{false};
//     bool lpr_credit_valid{false};
//     bool hpr_credit_valid{false};
// };


bool IsFullCycle(sc_core::sc_time time, sc_core::sc_time CycleTime);
sc_core::sc_time AlignAtNext(sc_core::sc_time time, sc_core::sc_time alignment);

    }
}
#endif