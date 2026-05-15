#ifndef __DDR5_MEM_SPEC_HH__
#define __DDR5_MEM_SPEC_HH__

#include <algorithm>
#include <iomanip>
#include <iostream>

#include <systemc>

#include "Configure/LoadMemSpec.hh"

namespace dmu {
namespace Controller {

static uint64_t AcTimingRounding(double AcTimingInNs, double tCKInNs,
                                 bool IsMin) {
  uint64_t nCK;
  if (AcTimingInNs == 0) {
    nCK = 0;
  } else {
    if (IsMin) {
      nCK = static_cast<uint64_t>(
          ((static_cast<uint64_t>(AcTimingInNs * 1000) * 997) /
               (static_cast<uint64_t>(tCKInNs * 1000)) +
           1000) /
          1000);
    } else {
      nCK = static_cast<uint64_t>((static_cast<uint64_t>(AcTimingInNs * 1000)) /
                                  (static_cast<uint64_t>(tCKInNs * 1000)));
    }
  }
  return nCK;
}

// based on the ac timing and freq_ratio, transalted timing parameter into MC
// clk
static uint64_t Tranform2McClk(uint64_t ddr_clk, unsigned freq_ratio) {
  double result =
      static_cast<double>(ddr_clk) / static_cast<double>(freq_ratio);

  return static_cast<uint64_t>(std::ceil(result));
}

enum class RefModeTypeDDR5 { Normal, FGR, Invalid };

enum class CmdMode { cmd_1_N, cmd_2_N, Invaid };

class DDR5MemSpec {
public:
  DDR5MemSpec &operator=(const DDR5MemSpec &) = delete;
  DDR5MemSpec &operator=(DDR5MemSpec &&) = delete;
  virtual ~DDR5MemSpec() = default;

  // DDR5的独特属性
  constexpr static const unsigned SubChannelBitWidth = 32; // DataBusWidth
  constexpr static const unsigned BytesPerBurstBeat = 32 / 8;
  // device info
  const unsigned
      NumOfBankGroupsPerDevice; // in a device, the number of  bank groups
  const unsigned NumOfRows;    // in a device , one bank has such number of rows
  const unsigned NumOfColumns; // one row has one such number of columns
  const unsigned NumOfBanksPerBankGroup; // in a device, bank group has such
                                         // number of banks
  const unsigned
      DeviceWidth; // memory device width , x4 is a sdram address vist 4 memory
                   // array in a device( address 4-bit )
  const unsigned DataRate; // for ddr5 is 2 data in a clock
  //
  const unsigned
      NumOfBanksPerDevice; // in a device , the number of banks,  the parameter
                           // is computed based on the NumOfBankGroupsPerDevice
                           // * NumOfBanksPerBankGroup
  //
  const bool Is_3DS;
  // const bool Is_MRDIMM;
  const unsigned NumOfChannels; // 所有的配置 只需要考虑一个sub-channel中的实现,
                                // 这一参数的具体值固定为1
  const unsigned
      NumOfSubChannelsPerChannel; // for ddr5 this parameter is fixed to 2
  const unsigned
      NumOfPseudoChannelsPerSubChannel; // for mrdimm this parameter is 2, in
                                        // rdimm/udimm this parameter is 1
  const unsigned
      NumOfPhysicalRanksPerPseudoChannel; // one pseudo channel has such number
                                          // of physical ranks
  const unsigned NumOfLogicalRanksPerPhysicalRank; // one physical rank has such
                                                   // number of logical ranks
  const unsigned
      NumOfDevicesPerLogicalRank; // one logical rank has such number of
                                  // devices:  SubChannelBitWidth / DeviceWidth

  // internal computed parameters
  const unsigned NumOfSubChannels;            // 这个参数没有任何实际意义
  const unsigned NumOfDevicesPerPhysicalRank; // one physical rank has such
                                              // number of devices
  const unsigned NumOfDevicesPerPseudoChannel;
  const unsigned NumOfDevicesPerSubChannel;
  const unsigned NumOfDevicesPerChannel;

  const unsigned NumOfLogicalRanksPerPseudoChannel;
  const unsigned NumOfPhysicalRanksPerSubChannel; // one Sub-channel has such
                                                  // number of physical ranks
  const unsigned NumOfPhysicalRanksPerChannel;
  const unsigned NumOfLogicalRanksPerSubChannel; // one channel has such number
                                                 // of logical ranks
  const unsigned NumOfLogicalRanksPerChannel;

  const unsigned NumOfBankGroupsPerLogicalRank;
  const unsigned NumOfBankGroupsPerPhysicalRank;
  const unsigned NumOfBankGroupsPerPseudoChannel;
  const unsigned NumOfBankGroupsPerSubChannel;
  const unsigned NumOfBankGroupsPerChannel;

  const unsigned
      NumOfBanksPerLogicalRank; // one logical rank has such number of banks,
                                // and this value is same with banks in a device
  const unsigned NumOfBanksPerPhysicalRank;
  const unsigned NumOfBanksPerPseudoChannel;
  const unsigned NumOfBanksPerSubChannel;
  const unsigned NumOfBanksPerChannel;

  //
  const unsigned BurstLenth; // normal bust lenth , for ddr5 is 16
  const unsigned maxBurstLenth;

  // refresh mode: 1: normal 2. FNG
  RefModeTypeDDR5 RefMode;
  // CMD mode: 1-N, 2-N
  CmdMode cmd_mode;

  const unsigned FreqRatio;

  // SpeedBins
  const double fCKMHz; // DRAM Clock Frequency
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

  // DataAcTiming
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

  // Memory type property
  const std::string memory_id;
  const std::string memory_type;

  // internal computed
  const sc_core::sc_time tBurst;

  // MC clock domain
  const sc_core::sc_time tCK_mc;
  const sc_core::sc_time tRCD_mc;
  const sc_core::sc_time tRASmin_mc;
  const sc_core::sc_time tRP_mc;
  const sc_core::sc_time tCL_mc;
  const sc_core::sc_time tCWL_mc;

protected:
  DDR5MemSpec(const DDR5MemConfig &mem_spec_base);
  DDR5MemSpec(const DDR5MemSpec &) = default;
  DDR5MemSpec(DDR5MemSpec &&) = default;
};

} // namespace Controller
} // namespace dmu

#endif