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

#ifndef CHECKERLPDDR5_H
#define CHECKERLPDDR5_H

#include "DRAMSys/configuration/Configuration.h"
#include "DRAMSys/configuration/memspec/MemSpecLPDDR5.h"
#include "DRAMSys/controller/checker/CheckerIF.h"

#include <queue>
#include <vector>

namespace DRAMSys
{

class CheckerLPDDR5 final : public CheckerIF
{
public:
    explicit CheckerLPDDR5(const Configuration& config);
    
    [[nodiscard]] sc_core::sc_time
    timeToSatisfyConstraints(Command command,
                             const tlm::tlm_generic_payload& payload) const override;
    
    void insert(Command command, const tlm::tlm_generic_payload& payload) override;

    // Helper method to query Bank Group mode
    [[nodiscard]] bool isBankGroupMode() const { return memSpec->bankGroupMode; }

private:
    const MemSpecLPDDR5* memSpec;

    // Command history records by Bank
    std::vector<ControllerVector<Bank, sc_core::sc_time>> lastScheduledByCommandAndBank;
    
    // Command history records by Bank Group (for 8 BG mode)
    std::vector<ControllerVector<BankGroup, sc_core::sc_time>> lastScheduledByCommandAndBankGroup;
    
    // Command history records by Rank
    std::vector<ControllerVector<Rank, sc_core::sc_time>> lastScheduledByCommandAndRank;
    
    // Global command history
    std::vector<sc_core::sc_time> lastScheduledByCommand;
    
    // Last command on bus timestamp
    sc_core::sc_time lastCommandOnBus;

    // Burst length tracking for BL16/BL32 support
    ControllerVector<Command, ControllerVector<Bank, uint8_t>> lastBurstLengthByCommandAndBank;

    // Four Activate Window (tFAW) tracking queue
    ControllerVector<Rank, std::queue<sc_core::sc_time>> last4Activates;

    // Maximum time constant
    const sc_core::sc_time scMaxTime = sc_core::sc_max_time();
    
    // Pre-computed composite timing parameters
    sc_core::sc_time tBURST;      // Burst transfer time (BL16)
    sc_core::sc_time tBURST32;    // Burst transfer time (BL32)
    sc_core::sc_time tRDWR;       // Read to Write turnaround time (same rank)
    sc_core::sc_time tRDWR_R;     // Read to Write turnaround time (different rank)
    sc_core::sc_time tWRRD;       // Write to Read turnaround time (16 Bank mode)
    sc_core::sc_time tWRRD_L;     // Write to Read turnaround time (same bank group, 8 BG mode)
    sc_core::sc_time tWRRD_S;     // Write to Read turnaround time (different bank group, 8 BG mode)
    sc_core::sc_time tWRRD_R;     // Write to Read turnaround time (different rank)
    sc_core::sc_time tRDPRE;      // Read to Precharge time
    sc_core::sc_time tRDAPRE;     // Read with Auto-precharge to Precharge time
    sc_core::sc_time tRDAACT;     // Read with Auto-precharge to Activate time
    sc_core::sc_time tWRPRE;      // Write to Precharge time
    sc_core::sc_time tWRAPRE;     // Write with Auto-precharge to Precharge time
    sc_core::sc_time tWRAACT;     // Write with Auto-precharge to Activate time
    
    // Power Down timing parameters
    sc_core::sc_time tACTPDEN;    // Activate to Power Down Entry
    sc_core::sc_time tPRPDEN;     // Precharge to Power Down Entry
    sc_core::sc_time tRDPDEN;     // Read to Power Down Entry
    sc_core::sc_time tWRPDEN;     // Write to Power Down Entry
    sc_core::sc_time tWRAPDEN;    // Write with Auto-precharge to Power Down Entry
    sc_core::sc_time tREFPDEN;    // Refresh to Power Down Entry

    // Helper method to get Bank Group from Bank (for 8 BG mode)
    [[nodiscard]] BankGroup getBankGroupFromBank(Bank bank) const;
    
    // Helper method to check if two banks are in the same Bank Group
    [[nodiscard]] bool isSameBankGroup(Bank bank1, Bank bank2) const;
    
    // Helper method to convert DRAM clock cycles to Controller clock cycles
    // This accounts for the controller:DRAM frequency ratio (1:1, 1:2, or 1:4)
    // Formula: controller_cycles = ceil(dram_cycles / controllerClockRatio)
    [[nodiscard]] sc_core::sc_time convertToControllerTime(sc_core::sc_time dramTime) const;
    
    // Helper method to convert DRAM CK cycles to Controller cycles
    [[nodiscard]] unsigned convertDramCyclesToControllerCycles(unsigned dramCycles) const;
};

} // namespace DRAMSys

#endif // CHECKERLPDDR5_H
