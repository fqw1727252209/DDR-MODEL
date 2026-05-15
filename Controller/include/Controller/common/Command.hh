#ifndef __CTRL_COMMAND_H__
#define __CTRL_COMMAND_H__

#include <string>
#include <systemc>
#include <tlm>
#include <vector>
#include <tuple>

#include "Controller/common/ControllerCommon.hh"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"

namespace dmu {
namespace Controller 
{

class Command
{
public:
    enum Type: uint8_t
    {
        NOP = 0,
        WR,
        RD,
        WRA,
        RDA, // CAS command end
        ACT, // RAS command begin
        PRE,
        // REFPB,
        // RFMPB,

        PRESb,
        REFSb,
        RFMSb, // this three same bank command, will implent in same banks in all bank group, s.t 4 bg and 2 banks orgnazition
               // the command will implement in bank 0, bank 2, bank 4, bank 6;

        PREab, // rank command begin
        REFab,
        RFMab,

        // /*TODO: other cmd */
        // SRE,
        // SREF,
        // PDE,
        // PDX,
        Invalid
    };

private:
    Type type; // command type to show the command sent to DFI

public:
    Command() = delete;
    Command(Type type) : type(type) {};

    inline Type to_type() const { return type; }
    std::string to_string() const;

    bool IsBankCommand() const;
    bool IsGroupCommand() const;
    bool IsRankCommand() const;
    bool IsCASCommand() const;
    bool IsApCommand() const;
    bool IsRASCommand() const;
    bool IsRefCommand() const;
    bool IsPreBankCommand() const;

    //
    unsigned GetCommandLength(bool is_2n_mode) const;

    // 添加的操作符重载
    // bool operator==(const Command& other) const { return type == other.type; }
    // bool operator!=(const Command& other) const { return type != other.type; }
    constexpr operator uint8_t() const { return static_cast<uint8_t>(type); }
    static unsigned NumOfCommands() { return 13; }
};

enum class CmdSrc {
    RdTransaction,
    WrTransaction,
    BankTransaction,
    RankTransaction,
    Invalid
};

struct CommandTuple
{
    using Type = std::tuple<dmu::Controller::Command, unsigned, BankAddress, sc_core::sc_time, CmdSrc, tlm::tlm_generic_payload*>;
    enum Accessor
    {
        Command = 0,
        CAM_INDEX = 1,
        BaAddress = 2,
        AvailTime = 3,
        CommandSrc = 4,
        Trans = 5
    };
};

using ReadyCommands = std::vector<CommandTuple::Type>;

} // Controller
} // dmu

#endif