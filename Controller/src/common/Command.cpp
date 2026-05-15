#include <array>
#include <iostream>
#include <sstream>
#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"

namespace dmu
{
    namespace Controller
    {

std::string
Command::to_string() const
{
    static std::array<std::string, Command::Type::Invalid> stringOfCommand = {
        "NOP",
        "WR",
        "RD",
        "WRA",
        "RDA", // CAS command end

        "ACT", // RAS command begin
        "PRE",

        "PREsb",
        "REFsb",
        "RFMsb",  // this three same bank command, will implent in same banks in all bank group, s.t 4 bg and 2 banks orgnazition
                  // the command will implement in bank 0, bank 2, bank 4, bank 6;

        "PREab", // rank command begin
        "REFab",
        "RFMab",
        // "SRE",
        // "SREF",
        // "PDE",
        // "PDX"
    };
    return stringOfCommand[type];
}

bool
Command::IsBankCommand() const
{
    return type <= Type::PRE;
}

bool
Command::IsGroupCommand() const
{
    return type >= Type::PREsb && type <= Type::RFMsb;
}

bool
Command::IsRankCommand() const
{
    return type >= Type::PREab && type <= Type::RFMab;
}

bool
Command::IsCASCommand() const
{
    return type <= Type::RDA && type >= Type::WR;
}

bool
Command::IsApCommand() const
{
    return type == Type::RDA || type == Type::WRA;
}

bool
Command::IsRASCommand() const
{
    return type >= Type::ACT && type <= Type::PRE;
}

bool
Command::IsPreBankCommand() const
{
    return type == Type::ACT || type == Type::PRE;
}

bool
Command::IsRefCommand() const
{
    return type == Type::REFab || type == Type::REFsb || type == Type::RFMab || type == Type::RFMsb;
}

unsigned
Command::GetCommandLength(bool is_2n_mode) const
{
    if(!is_2n_mode)
    {
        switch(type)
        {
            case Command::ACT:
            case Command::WR:
            case Command::WRA:
            case Command::RD:
            case Command::RDA:
                return 2;
            default:
                return 1;
        }
    }
    else
    {
        switch(type)
        {
            case Command::ACT:
            case Command::WR:
            case Command::WRA:
            case Command::RD:
            case Command::RDA:
                return 4;
            default:
                return 2;
        }
    }
}

    } // namespace Controller
} // namespace dmu