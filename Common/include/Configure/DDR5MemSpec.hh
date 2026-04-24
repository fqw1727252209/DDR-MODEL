#ifndef __DDR5_MEM_SPEC_HH__
#define __DDR5_MEM_SPEC_HH__

#include <iomanip>
#include <iostream>
#include <algorithm>

#include <systemc>

#include "Configure/LoadMemSpec.hh"

namespace dmu{
    namespace Controller{

static uint64_t AcTimingRounding(double AcTimingInNs, double tCKInNs, bool IsMin)
{
    uint64_t nCK;
    if(AcTimingInNs == 0)
    {
        nCK = 0;
    }
    else {
        if(IsMin)
        {
            nCK = static_cast<uint64_t>(
                ( (static_cast<uint64_t>(AcTimingInNs * 1000)*997) / (static_cast<uint64_t>(tCKInNs * 1000)) + 1000 )
                / 1000
            );
        }
        else
        {
            nCK = static_cast<uint64_t>(
                (static_cast<uint64_t>(AcTimingInNs * 1000)) / (static_cast<uint64_t>(tCKInNs * 1000))
            );
        }
    }
    return nCK;
}

// based on the ac timing and freq_ratio, transalted timing parameter into MC clk
static uint64_t Tranform2McClk(uint64_t ddr_clk, unsigned freq_ratio)
{
    double result = static_cast<double>(ddr_clk) / static_cast<double>(freq_ratio);

    return static_cast<uint64_t>(std::ceil(result));
}


enum class RefModeTypeDDR5{
    Normal,
    FGR,
    Invalid
};

enum class CmdMode{
    cmd_1_N,
    cmd_2_N,
    Invaid
};

class DDR5MemSpec
{
public:
    DDR5MemSpec& operator=(const DDR5MemSpec&) = delete;
    DDR5MemSpec& operator=(DDR5MemSpec&&) = delete;
    virtual ~DDR5MemSpec() = default;

    const bool Is_3DS;

    const unsigned NumOfChannels; // how many dmus in whole soc
    const unsigned NumOfSubChannels; // one dmu has how many sub-channels
    //device info
    const unsigned TotalNumOfBanksPerDevice; // in a device, the number of bank
    const unsigned NumOfBankGroupsPerDevice; // in a device, the number of bank groups
    const unsigned NumOfRows; // in a device, one bank has such number of rows
    const unsigned NumOfColumns; // one row has one such number of columns
    const unsigned NumOfBanksPerBg; // in a device, bank group has such number of banks
    const unsigned width; // memory device width, x4 is a sdram address vist 4 memory array in a device( address 4-bit )
    const unsigned DataRate; // for ddr5 is 2 data in a clock

    const unsigned BurstLenth; // normal bust lenth, for ddr5 is 16
    const unsigned maxBurstLenth;

    // const unsigned DataBusWidth; // one channel port how many pins to the dimm // internal compute
    // const unsigned BytesPerBurst; // inter compute
    // refresh mode: 1: normal 2: FGR
    RefModeTypeDDR5 RefMode;
    // CMD mode: 1-N, 2-N
    CmdMode cmd_mode;

    const unsigned FreqRatio;

    //SpeedBins
    // Clock
    const double fCKMHz;
    const sc_core::sc_time tCK;
    const sc_core::sc_time tCL;
    const sc_core::sc_time tCWL;
    const sc_core::sc_time tRCD;
    const sc_core::sc_time tRP;
    const sc_core::sc_time tRASmin;

    const unsigned CL;
    const unsigned nRCD;
    const unsigned nRP;
    const unsigned CWL;


    //DataAcTiming
    const sc_core::sc_time tRPRE;
    const sc_core::sc_time tRPST;
    const sc_core::sc_time tWPRE;
    const sc_core::sc_time tWPST;
    const sc_core::sc_time Read_DQS_Offset;

    const unsigned nRPRE;
    const unsigned nRPST;
    const unsigned nWPRE;
    const unsigned nWPST;
    const unsigned nRead_DQS_Offset;

    // RFM related param
    const unsigned RAAIMT;
    const unsigned RAAMMT;
    const unsigned RAADEC;

    //Memory type property
    const std::string memory_id;
    const std::string memory_type;

    //internal computed
    const sc_core::sc_time tBurst;


    // MC clock domain
    const sc_core::sc_time tCK_mc;
    const sc_core::sc_time tRCD_mc;
    const sc_core::sc_time tRASmin_mc;
    const sc_core::sc_time tRP_mc;
    const sc_core::sc_time tCL_mc;
    const sc_core::sc_time tCWL_mc;

protected:

    DDR5MemSpec(const DDR5MemConfig& mem_spec_base);
    DDR5MemSpec(const DDR5MemSpec&) = default;
    DDR5MemSpec(DDR5MemSpec&&) = default;
};


    } // Controller
} // dmu

#endif