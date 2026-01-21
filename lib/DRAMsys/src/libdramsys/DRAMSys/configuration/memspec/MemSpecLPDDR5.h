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

#ifndef MEMSPECLPDDR5_H
#define MEMSPECLPDDR5_H

#include "DRAMSys/configuration/memspec/MemSpec.h"

#include <systemc>

namespace DRAMSys
{

class MemSpecLPDDR5 final : public MemSpec
{
public:
    explicit MemSpecLPDDR5(const ::DRAMSys::Config::MemSpec& memSpec);

    // LPDDR5 Bank Structure Constants
    // LPDDR5 supports 16 Banks, configurable as:
    // - 16 Bank mode: 16 independent banks
    // - 8 Bank Group mode: 8 bank groups with 2 banks each
    static constexpr unsigned defaultBanksPerRank = 16;
    static constexpr unsigned defaultBankGroupsPerRank = 8;
    static constexpr unsigned defaultBanksPerBankGroup = 2;

    // Bank Group Mode Configuration
    // When true: 8 Bank Group mode (uses tCCD_L/tCCD_S, tWTR_L/tWTR_S)
    // When false: 16 Bank mode (uses tCCD, tWTR)
    const bool bankGroupMode;

    // WCK/CK Clock Ratio (2:1 or 4:1)
    const unsigned wckCkRatio;

    // Controller to DRAM Clock Frequency Ratio
    // 1 = 1:1 (Controller runs at same frequency as DRAM CK)
    // 2 = 1:2 (Controller runs at half frequency of DRAM CK)
    // 4 = 1:4 (Controller runs at quarter frequency of DRAM CK)
    const unsigned controllerClockRatio;
    
    // Controller Clock Period (derived from tCK and controllerClockRatio)
    const sc_core::sc_time tCK_Controller;

    // Core Timing Parameters
    const sc_core::sc_time tRCD;      // Row to Column Delay - ACT to RD/WR
    const sc_core::sc_time tRAS;      // Row Active Time - ACT to PRE minimum
    const sc_core::sc_time tRPpb;     // Per-Bank Precharge Time
    const sc_core::sc_time tRPab;     // All-Bank Precharge Time
    const sc_core::sc_time tRC;       // Row Cycle Time - ACT to ACT same bank (tRAS + tRP)
    const sc_core::sc_time tRRD;      // Row to Row Delay - ACT to ACT different bank
    const sc_core::sc_time tFAW;      // Four Activate Window

    // Column Command Timing (16 Bank Mode)
    const sc_core::sc_time tCCD;      // Column to Column Delay (16 Bank mode)
    const sc_core::sc_time tWTR;      // Write to Read Turnaround (16 Bank mode)

    // Column Command Timing (8 Bank Group Mode)
    const sc_core::sc_time tCCD_L;    // Column to Column Delay Long (same bank group)
    const sc_core::sc_time tCCD_S;    // Column to Column Delay Short (different bank group)
    const sc_core::sc_time tWTR_L;    // Write to Read Long (same bank group)
    const sc_core::sc_time tWTR_S;    // Write to Read Short (different bank group)

    // Read/Write Timing Parameters
    const sc_core::sc_time tRL;       // Read Latency
    const sc_core::sc_time tWL;       // Write Latency
    const sc_core::sc_time tRTP;      // Read to Precharge
    const sc_core::sc_time tWR;       // Write Recovery Time
    const sc_core::sc_time tWPRE;     // Write Preamble
    const sc_core::sc_time tRPRE;     // Read Preamble
    const sc_core::sc_time tRPST;     // Read Postamble

    // DQS Timing Parameters
    const sc_core::sc_time tDQSCK;    // DQS to CK delay
    const sc_core::sc_time tDQSS;     // DQS to DQ skew
    const sc_core::sc_time tDQS2DQ;   // DQS to DQ delay

    // Refresh Timing Parameters
    const sc_core::sc_time tREFI;     // Refresh Interval
    const sc_core::sc_time tRFCab;    // All-Bank Refresh Cycle Time
    const sc_core::sc_time tRFCpb;    // Per-Bank Refresh Cycle Time
    const sc_core::sc_time tPBR2PBR;  // Per-Bank Refresh to Per-Bank Refresh (different bank)
    const sc_core::sc_time tPBR2ACT;  // Per-Bank Refresh to Activate

    // Power Down and Self Refresh Timing
    const sc_core::sc_time tCKE;      // Clock Enable time
    const sc_core::sc_time tXP;       // Exit Power Down time
    const sc_core::sc_time tXSR;      // Exit Self Refresh time
    const sc_core::sc_time tSR;       // Self Refresh time
    const sc_core::sc_time tCMDCKE;   // Command to CKE delay
    const sc_core::sc_time tESCKE;    // Exit Self Refresh to CKE

    // Additional Timing Parameters
    const sc_core::sc_time tPPD;      // Precharge to Precharge Delay
    const sc_core::sc_time tRTRS;     // Rank to Rank Switching
    const sc_core::sc_time tCCDMW;    // Masked Write CCD

    // Per-Bank Refresh Interval
    const sc_core::sc_time tREFIpb;   // Per-Bank Refresh Interval

    // Interface Methods
    [[nodiscard]] sc_core::sc_time getRefreshIntervalAB() const override;
    [[nodiscard]] sc_core::sc_time getRefreshIntervalPB() const override;

    [[nodiscard]] sc_core::sc_time
    getExecutionTime(Command command, const tlm::tlm_generic_payload& payload) const override;
    
    [[nodiscard]] TimeInterval
    getIntervalOnDataStrobe(Command command,
                            const tlm::tlm_generic_payload& payload) const override;

    [[nodiscard]] bool requiresMaskedWrite(const tlm::tlm_generic_payload& payload) const override;

    // Override to return controller clock period (accounts for frequency ratio)
    [[nodiscard]] sc_core::sc_time getControllerClockPeriod() const override { return tCK_Controller; }

    // Helper method to get burst duration for BL32
    [[nodiscard]] sc_core::sc_time getBurstDuration32() const;
};

} // namespace DRAMSys

#endif // MEMSPECLPDDR5_H
