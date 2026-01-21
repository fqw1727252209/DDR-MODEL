/*
 * Copyright (c) 2024, RPTU Kaiserslautern-Landau
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors:
 *    LPDDR5 AC Timing Checker Implementation
 */

#include "MemSpecLPDDR5.h"

#include "DRAMSys/common/utils.h"

#include <iostream>
#include <unordered_map>

using namespace sc_core;
using namespace tlm;

namespace DRAMSys
{

// Helper function to get value with default
static unsigned getValueOrDefault(const std::unordered_map<std::string, unsigned>& entries,
                                  const std::string& key,
                                  unsigned defaultValue)
{
    auto it = entries.find(key);
    return (it != entries.end()) ? it->second : defaultValue;
}

MemSpecLPDDR5::MemSpecLPDDR5(const ::DRAMSys::Config::MemSpec& memSpec) :
    MemSpec(memSpec,
            MemoryType::LPDDR5,
            memSpec.memarchitecturespec.entries.at("nbrOfChannels"),
            1, // pseudoChannelsPerChannel
            memSpec.memarchitecturespec.entries.at("nbrOfRanks"),
            getValueOrDefault(memSpec.memarchitecturespec.entries, "nbrOfBanks", defaultBanksPerRank),
            getValueOrDefault(memSpec.memarchitecturespec.entries, "nbrOfBankGroups", defaultBankGroupsPerRank),
            getValueOrDefault(memSpec.memarchitecturespec.entries, "nbrOfBanksPerGroup", defaultBanksPerBankGroup),
            getValueOrDefault(memSpec.memarchitecturespec.entries, "nbrOfBanks", defaultBanksPerRank) *
                memSpec.memarchitecturespec.entries.at("nbrOfRanks"),
            getValueOrDefault(memSpec.memarchitecturespec.entries, "nbrOfBankGroups", defaultBankGroupsPerRank) *
                memSpec.memarchitecturespec.entries.at("nbrOfRanks"),
            memSpec.memarchitecturespec.entries.at("nbrOfDevices")),
    // Bank Group Mode Configuration
    bankGroupMode(getValueOrDefault(memSpec.memarchitecturespec.entries, "bankGroupMode", 0) != 0),
    wckCkRatio(getValueOrDefault(memSpec.memarchitecturespec.entries, "wckCkRatio", 4)),
    // Controller Clock Ratio Configuration (default 1:1)
    controllerClockRatio(getValueOrDefault(memSpec.memarchitecturespec.entries, "controllerClockRatio", 1)),
    tCK_Controller(tCK * controllerClockRatio),
    // Core Timing Parameters
    tRCD(tCK * memSpec.memtimingspec.entries.at("RCD")),
    tRAS(tCK * memSpec.memtimingspec.entries.at("RAS")),
    tRPpb(tCK * memSpec.memtimingspec.entries.at("RPPB")),
    tRPab(tCK * memSpec.memtimingspec.entries.at("RPAB")),
    tRC(tCK * memSpec.memtimingspec.entries.at("RC")),
    tRRD(tCK * memSpec.memtimingspec.entries.at("RRD")),
    tFAW(tCK * memSpec.memtimingspec.entries.at("FAW")),
    // Column Command Timing (16 Bank Mode)
    tCCD(tCK * memSpec.memtimingspec.entries.at("CCD")),
    tWTR(tCK * memSpec.memtimingspec.entries.at("WTR")),
    // Column Command Timing (8 Bank Group Mode)
    tCCD_L(tCK * getValueOrDefault(memSpec.memtimingspec.entries, "CCD_L", 
           memSpec.memtimingspec.entries.at("CCD"))),
    tCCD_S(tCK * getValueOrDefault(memSpec.memtimingspec.entries, "CCD_S",
           memSpec.memtimingspec.entries.at("CCD"))),
    tWTR_L(tCK * getValueOrDefault(memSpec.memtimingspec.entries, "WTR_L",
           memSpec.memtimingspec.entries.at("WTR"))),
    tWTR_S(tCK * getValueOrDefault(memSpec.memtimingspec.entries, "WTR_S",
           memSpec.memtimingspec.entries.at("WTR"))),
    // Read/Write Timing Parameters
    tRL(tCK * memSpec.memtimingspec.entries.at("RL")),
    tWL(tCK * memSpec.memtimingspec.entries.at("WL")),
    tRTP(tCK * memSpec.memtimingspec.entries.at("RTP")),
    tWR(tCK * memSpec.memtimingspec.entries.at("WR")),
    tWPRE(tCK * memSpec.memtimingspec.entries.at("WPRE")),
    tRPRE(tCK * getValueOrDefault(memSpec.memtimingspec.entries, "RPRE", 0)),
    tRPST(tCK * memSpec.memtimingspec.entries.at("RPST")),
    // DQS Timing Parameters
    tDQSCK(tCK * memSpec.memtimingspec.entries.at("DQSCK")),
    tDQSS(tCK * memSpec.memtimingspec.entries.at("DQSS")),
    tDQS2DQ(tCK * memSpec.memtimingspec.entries.at("DQS2DQ")),
    // Refresh Timing Parameters
    tREFI(tCK * memSpec.memtimingspec.entries.at("REFI")),
    tRFCab(tCK * memSpec.memtimingspec.entries.at("RFCAB")),
    tRFCpb(tCK * memSpec.memtimingspec.entries.at("RFCPB")),
    tPBR2PBR(tCK * getValueOrDefault(memSpec.memtimingspec.entries, "PBR2PBR",
             memSpec.memtimingspec.entries.at("RFCPB"))),
    tPBR2ACT(tCK * getValueOrDefault(memSpec.memtimingspec.entries, "PBR2ACT",
             memSpec.memtimingspec.entries.at("RFCPB"))),
    // Power Down and Self Refresh Timing
    tCKE(tCK * memSpec.memtimingspec.entries.at("CKE")),
    tXP(tCK * memSpec.memtimingspec.entries.at("XP")),
    tXSR(tCK * memSpec.memtimingspec.entries.at("XSR")),
    tSR(tCK * memSpec.memtimingspec.entries.at("SR")),
    tCMDCKE(tCK * memSpec.memtimingspec.entries.at("CMDCKE")),
    tESCKE(tCK * memSpec.memtimingspec.entries.at("ESCKE")),
    // Additional Timing Parameters
    tPPD(tCK * memSpec.memtimingspec.entries.at("PPD")),
    tRTRS(tCK * memSpec.memtimingspec.entries.at("RTRS")),
    tCCDMW(tCK * getValueOrDefault(memSpec.memtimingspec.entries, "CCDMW",
           memSpec.memtimingspec.entries.at("CCD") * 2)),
    // Per-Bank Refresh Interval
    tREFIpb(tCK * getValueOrDefault(memSpec.memtimingspec.entries, "REFIPB",
            memSpec.memtimingspec.entries.at("REFI") / defaultBanksPerRank))
{
    // Set command lengths in cycles for LPDDR5
    // LPDDR5 uses 2-cycle commands on the CA bus
    commandLengthInCycles[Command::ACT] = 2;
    commandLengthInCycles[Command::PREPB] = 2;
    commandLengthInCycles[Command::PREAB] = 2;
    commandLengthInCycles[Command::RD] = 2;
    commandLengthInCycles[Command::RDA] = 2;
    commandLengthInCycles[Command::WR] = 2;
    commandLengthInCycles[Command::MWR] = 2;
    commandLengthInCycles[Command::WRA] = 2;
    commandLengthInCycles[Command::MWRA] = 2;
    commandLengthInCycles[Command::REFAB] = 2;
    commandLengthInCycles[Command::REFPB] = 2;
    commandLengthInCycles[Command::SREFEN] = 2;
    commandLengthInCycles[Command::SREFEX] = 2;
    commandLengthInCycles[Command::PDEA] = 2;
    commandLengthInCycles[Command::PDXA] = 2;
    commandLengthInCycles[Command::PDEP] = 2;
    commandLengthInCycles[Command::PDXP] = 2;

    // Calculate memory size
    uint64_t deviceSizeBits =
        static_cast<uint64_t>(banksPerRank) * rowsPerBank * columnsPerRow * bitWidth;
    uint64_t deviceSizeBytes = deviceSizeBits / 8;
    memorySizeBytes = deviceSizeBytes * ranksPerChannel * numberOfChannels;

    // Print memory configuration
    std::cout << headline << std::endl;
    std::cout << "Memory Configuration:" << std::endl << std::endl;
    std::cout << " Memory type:           "
              << "LPDDR5" << std::endl;
    std::cout << " Memory size in bytes:  " << memorySizeBytes << std::endl;
    std::cout << " Channels:              " << numberOfChannels << std::endl;
    std::cout << " Ranks per channel:     " << ranksPerChannel << std::endl;
    std::cout << " Banks per rank:        " << banksPerRank << std::endl;
    std::cout << " Bank groups per rank:  " << groupsPerRank << std::endl;
    std::cout << " Banks per bank group:  " << banksPerGroup << std::endl;
    std::cout << " Bank group mode:       " << (bankGroupMode ? "8 BG" : "16 Bank") << std::endl;
    std::cout << " WCK/CK ratio:          " << wckCkRatio << ":1" << std::endl;
    std::cout << " Rows per bank:         " << rowsPerBank << std::endl;
    std::cout << " Columns per row:       " << columnsPerRow << std::endl;
    std::cout << " Device width in bits:  " << bitWidth << std::endl;
    std::cout << " Device size in bits:   " << deviceSizeBits << std::endl;
    std::cout << " Device size in bytes:  " << deviceSizeBytes << std::endl;
    std::cout << " Devices per rank:      " << devicesPerRank << std::endl;
    std::cout << " Default burst length:  " << defaultBurstLength << std::endl;
    std::cout << std::endl;
}

sc_time MemSpecLPDDR5::getRefreshIntervalAB() const
{
    return tREFI;
}

sc_time MemSpecLPDDR5::getRefreshIntervalPB() const
{
    return tREFIpb;
}

sc_time MemSpecLPDDR5::getBurstDuration32() const
{
    // BL32 burst duration is twice the default BL16 burst duration
    return burstDuration * 2;
}

sc_time MemSpecLPDDR5::getExecutionTime(Command command,
                                        [[maybe_unused]] const tlm_generic_payload& payload) const
{
    if (command == Command::PREPB)
        return tRPpb + tCK;

    if (command == Command::PREAB)
        return tRPab + tCK;

    if (command == Command::ACT)
        return tRCD + tCK;

    if (command == Command::RD)
        return tRL + tDQSCK + burstDuration + tCK;

    if (command == Command::RDA)
        return burstDuration + tRTP + tRPpb;

    if (command == Command::WR || command == Command::MWR)
        return tWL + tDQSS + tDQS2DQ + burstDuration + tCK;

    if (command == Command::WRA || command == Command::MWRA)
        return tWL + burstDuration + tWR + tRPpb;

    if (command == Command::REFAB)
        return tRFCab + tCK;

    if (command == Command::REFPB)
        return tRFCpb + tCK;

    if (command == Command::SREFEN)
        return tCKE;

    if (command == Command::SREFEX)
        return tXSR;

    if (command == Command::PDEA || command == Command::PDEP)
        return tCKE;

    if (command == Command::PDXA || command == Command::PDXP)
        return tXP;

    SC_REPORT_FATAL("MemSpecLPDDR5::getExecutionTime",
                    "command not known or command doesn't have a fixed execution time");
    throw;
}

TimeInterval
MemSpecLPDDR5::getIntervalOnDataStrobe(Command command,
                                       [[maybe_unused]] const tlm_generic_payload& payload) const
{
    if (command == Command::RD || command == Command::RDA)
        return {tRL + tDQSCK + tCK, tRL + tDQSCK + burstDuration + tCK};

    if (command == Command::WR || command == Command::WRA || command == Command::MWR ||
        command == Command::MWRA)
        return {tWL + tDQSS + tDQS2DQ + tCK, tWL + tDQSS + tDQS2DQ + burstDuration + tCK};

    SC_REPORT_FATAL("MemSpecLPDDR5::getIntervalOnDataStrobe",
                    "Method was called with invalid argument");
    throw;
}

bool MemSpecLPDDR5::requiresMaskedWrite(const tlm::tlm_generic_payload& payload) const
{
    return !allBytesEnabled(payload);
}

} // namespace DRAMSys
