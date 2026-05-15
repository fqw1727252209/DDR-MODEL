#include <cstddef>
#include <systemc>
#include <tlm>
#include "Controller/common/ControllerCommon.hh"

namespace dmu{

namespace Controller{

template <typename T, size_t N>
static const char* enum_value_to_string(T value, const char* const (&names)[N])
{
    return value>=0 && value < N ? names[value] : "???";
}








BankAddress::BankAddress()
: bank(0)
, bankgroup(0)
, cid(0)
, cs(0)
, pseudo_ch(0)
, sub_ch(0)
, ch(0)
, real_ba(0)
, real_bg(0)
, real_cid(0)
{}

BankAddress::BankAddress(unsigned ch, unsigned sub_ch,unsigned pch_ch,unsigned cs, unsigned cid, unsigned real_cid)
: bank(0)
, bankgroup(0)
, cid(cid)
, cs(cs)
, pseudo_ch(pch_ch)
, sub_ch(sub_ch)
, ch(ch)
, real_ba(0)
, real_bg(0)
, real_cid(real_cid)
{
    is_rank_address = true;
}

BankAddress::BankAddress(const DecodedAddress& sdram_addr)
: bank(sdram_addr.bank)
, bankgroup(sdram_addr.bankgroup)
, cid(sdram_addr.cid)
, cs(sdram_addr.cs)
, pseudo_ch(sdram_addr.pseudo_channel)
, sub_ch(sdram_addr.sub_channel)
, ch(sdram_addr.channel)
, real_ba(sdram_addr.real_ba)
, real_bg(sdram_addr.real_bg)
, real_cid(sdram_addr.real_cid)
{}

void
BankAddress::SetBankAddrees(const DecodedAddress& sdram_addr)
{
    bank = sdram_addr.bank;
    bankgroup = sdram_addr.bankgroup;
    cid = sdram_addr.cid;
    cs = sdram_addr.cs;
    pseudo_ch = sdram_addr.pseudo_channel;
    sub_ch = sdram_addr.sub_channel;
    ch = sdram_addr.channel;
    real_ba = sdram_addr.real_ba;
    real_bg = sdram_addr.real_bg;
    real_cid = sdram_addr.real_cid;
}

void
BankAddress::ResetBankAddress()
{
    bank = 0;
    bankgroup = 0;
    cid = 0;
    cs = 0;
    pseudo_ch = 0;
    sub_ch = 0;
    ch = 0;
    real_ba = 0;
    real_bg = 0;
    real_cid = 0;
}

std::ostream& operator<<(std::ostream& os, const BankAddress& ba)
{
    os << "BankAddress {\t"
       << "  ch: " << ba.ch << "\t"
       << "  sub_ch:"<<ba.sub_ch << "\t"
       << "  pse_ch:"<<ba.pseudo_ch<<"\t"
       << "  cs: " << ba.cs << "\t"
       << "  cid: " << ba.cid << "\t"
       << "  bankgroup: " << ba.bankgroup << "\t"
       << "  bank: " << ba.bank << "\t"
       << "  real_ba: "<< ba.real_ba << "\t"
       << "  real_bg: "<< ba.real_bg << "\t"
       << "  real_cid: "<< ba.real_cid << "\t"
       << "}";
    return os;
}


bool IsFullCycle(sc_core::sc_time time, sc_core::sc_time CycleTime)
{
    return AlignAtNext(time, CycleTime) == time; // decide whether the time is a full cycle
}

sc_core::sc_time AlignAtNext(sc_core::sc_time time, sc_core::sc_time alignment)
{
    return std::ceil(time / alignment) * alignment; // get the next aliged-Cycle time
}

}
} // dmu