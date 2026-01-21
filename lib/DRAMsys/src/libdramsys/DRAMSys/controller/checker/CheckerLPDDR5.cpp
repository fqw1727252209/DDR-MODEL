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

#include "CheckerLPDDR5.h"

#include "DRAMSys/common/DebugManager.h"

#include <algorithm>
#include <cmath>

using namespace sc_core;
using namespace tlm;

namespace DRAMSys
{

CheckerLPDDR5::CheckerLPDDR5(const Configuration& config) :
    memSpec(dynamic_cast<const MemSpecLPDDR5*>(config.memSpec.get()))
{
    if (memSpec == nullptr)
        SC_REPORT_FATAL("CheckerLPDDR5", "Wrong MemSpec chosen");

    // Initialize command history vectors
    lastScheduledByCommandAndBank = std::vector<ControllerVector<Bank, sc_time>>(
        Command::numberOfCommands(),
        ControllerVector<Bank, sc_time>(memSpec->banksPerChannel, scMaxTime));
    
    lastScheduledByCommandAndBankGroup = std::vector<ControllerVector<BankGroup, sc_time>>(
        Command::numberOfCommands(),
        ControllerVector<BankGroup, sc_time>(memSpec->bankGroupsPerChannel, scMaxTime));
    
    lastScheduledByCommandAndRank = std::vector<ControllerVector<Rank, sc_time>>(
        Command::numberOfCommands(),
        ControllerVector<Rank, sc_time>(memSpec->ranksPerChannel, scMaxTime));
    
    lastScheduledByCommand = std::vector<sc_time>(Command::numberOfCommands(), scMaxTime);
    lastCommandOnBus = scMaxTime;
    
    // Initialize tFAW tracking queue
    last4Activates = ControllerVector<Rank, std::queue<sc_time>>(memSpec->ranksPerChannel);

    // Initialize burst length tracking
    lastBurstLengthByCommandAndBank = ControllerVector<Command, ControllerVector<Bank, uint8_t>>(
        Command::WRA + 1, ControllerVector<Bank, uint8_t>(memSpec->banksPerChannel));

    // Pre-compute composite timing parameters
    // LPDDR5 default burst length is 16
    tBURST = memSpec->defaultBurstLength / memSpec->dataRate * memSpec->tCK;
    tBURST32 = 32 / memSpec->dataRate * memSpec->tCK;  // BL32 burst duration

    // Read to Write turnaround time
    // tRDWR = tRL + tDQSCK + tBURST - tWL + tWPRE + tRPST
    tRDWR = memSpec->tRL + memSpec->tDQSCK + tBURST - memSpec->tWL + memSpec->tWPRE + memSpec->tRPST;
    tRDWR_R = memSpec->tRL + tBURST + memSpec->tRTRS - memSpec->tWL;

    // Write to Read turnaround time (16 Bank mode)
    // tWRRD = tWL + tCK + tBURST + tWTR
    tWRRD = memSpec->tWL + memSpec->tCK + tBURST + memSpec->tWTR;
    
    // Write to Read turnaround time (8 BG mode - same bank group)
    tWRRD_L = memSpec->tWL + memSpec->tCK + tBURST + memSpec->tWTR_L;
    
    // Write to Read turnaround time (8 BG mode - different bank group)
    tWRRD_S = memSpec->tWL + memSpec->tCK + tBURST + memSpec->tWTR_S;
    
    // Write to Read turnaround time (different rank)
    tWRRD_R = memSpec->tWL + tBURST + memSpec->tRTRS - memSpec->tRL;

    // Read to Precharge time
    // tRDPRE = tRTP + tBURST - 6*tCK (LPDDR5 specific adjustment)
    tRDPRE = memSpec->tRTP + tBURST - 6 * memSpec->tCK;
    
    // Read with Auto-precharge to Activate time
    tRDAACT = memSpec->tRTP + tBURST - 8 * memSpec->tCK + memSpec->tRPpb;

    // Write to Precharge time
    // tWRPRE = tWL + tBURST + tCK + tWR + 2*tCK
    tWRPRE = memSpec->tWL + tBURST + memSpec->tCK + memSpec->tWR + 2 * memSpec->tCK;
    
    // Write with Auto-precharge to Activate time
    tWRAACT = memSpec->tWL + tBURST + memSpec->tCK + memSpec->tWR + memSpec->tRPpb;

    // Power Down timing parameters
    tACTPDEN = 3 * memSpec->tCK + memSpec->tCMDCKE;
    tPRPDEN = memSpec->tCK + memSpec->tCMDCKE;
    tRDPDEN = 3 * memSpec->tCK + memSpec->tRL + memSpec->tDQSCK + tBURST + memSpec->tRPST;
    tWRPDEN = 3 * memSpec->tCK + memSpec->tWL +
              (std::ceil(memSpec->tDQSS / memSpec->tCK) + 
               std::ceil(memSpec->tDQS2DQ / memSpec->tCK)) * memSpec->tCK +
              tBURST + memSpec->tWR;
    tWRAPDEN = 3 * memSpec->tCK + memSpec->tWL +
               (std::ceil(memSpec->tDQSS / memSpec->tCK) + 
                std::ceil(memSpec->tDQS2DQ / memSpec->tCK)) * memSpec->tCK +
               tBURST + memSpec->tWR + 2 * memSpec->tCK;
    tREFPDEN = memSpec->tCK + memSpec->tCMDCKE;
}

BankGroup CheckerLPDDR5::getBankGroupFromBank(Bank bank) const
{
    // In 8 Bank Group mode, each bank group contains 2 banks
    // Bank 0,1 -> BG 0; Bank 2,3 -> BG 1; etc.
    return BankGroup(static_cast<std::size_t>(bank) / MemSpecLPDDR5::defaultBanksPerBankGroup);
}

bool CheckerLPDDR5::isSameBankGroup(Bank bank1, Bank bank2) const
{
    return getBankGroupFromBank(bank1) == getBankGroupFromBank(bank2);
}

sc_time CheckerLPDDR5::convertToControllerTime(sc_time dramTime) const
{
    // Convert DRAM time domain to Controller time domain
    // Formula: controller_time = ceil(dram_time / tCK_Controller) * tCK_Controller
    //
    // Example with 1:2 ratio (Controller @ 400MHz, DRAM @ 800MHz):
    //   - tCK = 1.25ns (DRAM)
    //   - tCK_Controller = 2.5ns
    //   - If dramTime = 48.75ns (39 DRAM cycles)
    //   - controller_cycles = ceil(48.75ns / 2.5ns) = ceil(19.5) = 20
    //   - controller_time = 20 * 2.5ns = 50ns
    
    if (memSpec->controllerClockRatio == 1)
    {
        // 1:1 ratio - no conversion needed
        return dramTime;
    }
    
    // Calculate number of controller cycles needed (round up)
    double cycles = dramTime / memSpec->tCK_Controller;
    unsigned controller_cycles = static_cast<unsigned>(std::ceil(cycles));
    
    // Convert back to time
    return controller_cycles * memSpec->tCK_Controller;
}

unsigned CheckerLPDDR5::convertDramCyclesToControllerCycles(unsigned dramCycles) const
{
    // Convert DRAM CK cycles to Controller cycles
    // Formula: controller_cycles = ceil(dram_cycles / controllerClockRatio)
    //
    // Example with 1:2 ratio:
    //   - dramCycles = 39 (tRC in DRAM CK cycles)
    //   - controllerClockRatio = 2
    //   - controller_cycles = ceil(39 / 2) = ceil(19.5) = 20
    
    if (memSpec->controllerClockRatio == 1)
    {
        // 1:1 ratio - no conversion needed
        return dramCycles;
    }
    
    // Round up division
    return (dramCycles + memSpec->controllerClockRatio - 1) / memSpec->controllerClockRatio;
}


sc_time CheckerLPDDR5::timeToSatisfyConstraints(Command command,
                                                const tlm_generic_payload& payload) const
{
    Rank rank = ControllerExtension::getRank(payload);
    BankGroup bankGroup = ControllerExtension::getBankGroup(payload);
    Bank bank = ControllerExtension::getBank(payload);

    sc_time lastCommandStart;
    sc_time earliestTimeToStart = sc_time_stamp();

    if (command == Command::ACT)
    {
        // ACT to ACT same bank: tRC constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::ACT][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRC);

        // ACT to ACT different bank: tRRD constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::ACT][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRRD);

        // RDA to ACT same bank: tRDAACT constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::RDA][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDAACT);

        // WRA to ACT same bank: tWRAACT constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::WRA][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRAACT);

        // PREPB to ACT same bank: tRPpb constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::PREPB][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRPpb - 2 * memSpec->tCK);

        // PREAB to ACT same rank: tRPab constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRPab - 2 * memSpec->tCK);

        // Power Down Exit to ACT: tXP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);

        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXP][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);

        // REFAB to ACT same rank: tRFCab constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRFCab - 2 * memSpec->tCK);

        // REFPB to ACT same bank: tRFCpb constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::REFPB][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRFCpb - 2 * memSpec->tCK);

        // REFPB to ACT different bank: tRRD constraint (similar to ACT-ACT)
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRRD - 2 * memSpec->tCK);

        // Self Refresh Exit to ACT: tXSR constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::SREFEX][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tXSR - 2 * memSpec->tCK);

        // tFAW constraint: Four Activate Window
        if (last4Activates[rank].size() >= 4)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           last4Activates[rank].front() + memSpec->tFAW - 3 * memSpec->tCK);
    }

    else if (command == Command::RD || command == Command::RDA)
    {
        unsigned burstLength = ControllerExtension::getBurstLength(payload);
        assert((burstLength == 16) || (burstLength == 32)); // LPDDR5 supports BL16/BL32
        assert(burstLength <= memSpec->maxBurstLength);

        // ACT to RD same bank: tRCD constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::ACT][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRCD);

        if (memSpec->bankGroupMode)
        {
            // 8 Bank Group Mode: Use tCCD_L for same bank group, tCCD_S for different bank group
            
            // RD to RD same bank group: tCCD_L constraint
            lastCommandStart = lastScheduledByCommandAndBankGroup[Command::RD][bankGroup];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCCD_L);

            // RD to RD same rank (different bank group): tCCD_S constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::RD][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCCD_S);

            // RDA to RD same bank group: tCCD_L constraint
            lastCommandStart = lastScheduledByCommandAndBankGroup[Command::RDA][bankGroup];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCCD_L);

            // RDA to RD same rank: tCCD_S constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::RDA][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCCD_S);

            // WR to RD same bank group: tWTR_L constraint
            lastCommandStart = lastScheduledByCommandAndBankGroup[Command::WR][bankGroup];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRRD_L);

            // WR to RD same rank (different bank group): tWTR_S constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::WR][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRRD_S);

            // WRA to RD same bank group: tWTR_L constraint
            lastCommandStart = lastScheduledByCommandAndBankGroup[Command::WRA][bankGroup];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRRD_L);

            // WRA to RD same rank: tWTR_S constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::WRA][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRRD_S);
        }
        else
        {
            // 16 Bank Mode: Use tCCD for all column commands
            
            // RD to RD same rank: tCCD constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::RD][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tBURST);

            // RDA to RD same rank: tCCD constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::RDA][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tBURST);

            // WR to RD same rank: tWTR constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::WR][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRRD);

            // WRA to RD same rank: tWTR constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::WRA][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRRD);
        }

        // Cross-rank RD to RD: tBURST + tRTRS constraint
        lastCommandStart = lastScheduledByCommand[Command::RD] != lastScheduledByCommandAndRank[Command::RD][rank]
                               ? lastScheduledByCommand[Command::RD]
                               : scMaxTime;
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tBURST + memSpec->tRTRS);

        lastCommandStart = lastScheduledByCommand[Command::RDA] != lastScheduledByCommandAndRank[Command::RDA][rank]
                               ? lastScheduledByCommand[Command::RDA]
                               : scMaxTime;
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tBURST + memSpec->tRTRS);

        // Cross-rank WR to RD: tWRRD_R constraint
        lastCommandStart = lastScheduledByCommand[Command::WR] != lastScheduledByCommandAndRank[Command::WR][rank]
                               ? lastScheduledByCommand[Command::WR]
                               : scMaxTime;
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRRD_R);

        lastCommandStart = lastScheduledByCommand[Command::WRA] != lastScheduledByCommandAndRank[Command::WRA][rank]
                               ? lastScheduledByCommand[Command::WRA]
                               : scMaxTime;
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRRD_R);

        // RDA specific: WR to RDA same bank constraint for auto-precharge
        if (command == Command::RDA)
        {
            lastCommandStart = lastScheduledByCommandAndBank[Command::WR][bank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRPRE - tRDPRE);
        }

        // Power Down Exit to RD: tXP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);
    }

    else if (command == Command::WR || command == Command::WRA || 
             command == Command::MWR || command == Command::MWRA)
    {
        unsigned burstLength = ControllerExtension::getBurstLength(payload);
        assert((burstLength == 16) || (burstLength == 32)); // LPDDR5 supports BL16/BL32
        assert(burstLength <= memSpec->maxBurstLength);

        // ACT to WR same bank: tRCD constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::ACT][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRCD);

        // RD to WR same rank: tRDWR constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RD][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDWR);

        // RDA to WR same rank: tRDWR constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RDA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDWR);

        // Cross-rank RD to WR: tRDWR_R constraint
        lastCommandStart = lastScheduledByCommand[Command::RD] != lastScheduledByCommandAndRank[Command::RD][rank]
                               ? lastScheduledByCommand[Command::RD]
                               : scMaxTime;
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDWR_R);

        lastCommandStart = lastScheduledByCommand[Command::RDA] != lastScheduledByCommandAndRank[Command::RDA][rank]
                               ? lastScheduledByCommand[Command::RDA]
                               : scMaxTime;
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDWR_R);

        if (memSpec->bankGroupMode)
        {
            // 8 Bank Group Mode: Use tCCD_L for same bank group, tCCD_S for different bank group
            
            // WR to WR same bank group: tCCD_L constraint
            lastCommandStart = lastScheduledByCommandAndBankGroup[Command::WR][bankGroup];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCCD_L);

            // WR to WR same rank (different bank group): tCCD_S constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::WR][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCCD_S);

            // WRA to WR same bank group: tCCD_L constraint
            lastCommandStart = lastScheduledByCommandAndBankGroup[Command::WRA][bankGroup];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCCD_L);

            // WRA to WR same rank: tCCD_S constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::WRA][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCCD_S);
        }
        else
        {
            // 16 Bank Mode: Use tCCD for all column commands
            
            // WR to WR same rank: tCCD constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::WR][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tBURST);

            // WRA to WR same rank: tCCD constraint
            lastCommandStart = lastScheduledByCommandAndRank[Command::WRA][rank];
            if (lastCommandStart != scMaxTime)
                earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tBURST);
        }

        // Cross-rank WR to WR: tBURST + tRTRS constraint
        lastCommandStart = lastScheduledByCommand[Command::WR] != lastScheduledByCommandAndRank[Command::WR][rank]
                               ? lastScheduledByCommand[Command::WR]
                               : scMaxTime;
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tBURST + memSpec->tRTRS);

        lastCommandStart = lastScheduledByCommand[Command::WRA] != lastScheduledByCommandAndRank[Command::WRA][rank]
                               ? lastScheduledByCommand[Command::WRA]
                               : scMaxTime;
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tBURST + memSpec->tRTRS);

        // Masked Write specific: tCCDMW constraint
        if (command == Command::MWR || command == Command::MWRA)
        {
            lastCommandStart = lastScheduledByCommandAndBank[Command::WR][bank];
            if (lastCommandStart != scMaxTime)
            {
                if (lastBurstLengthByCommandAndBank[Command::WR][bank] == 32)
                    earliestTimeToStart = std::max(earliestTimeToStart, 
                                                   lastCommandStart + memSpec->tCCDMW + 8 * memSpec->tCK);
                else
                    earliestTimeToStart = std::max(earliestTimeToStart, 
                                                   lastCommandStart + memSpec->tCCDMW);
            }

            lastCommandStart = lastScheduledByCommandAndBank[Command::WRA][bank];
            if (lastCommandStart != scMaxTime)
            {
                if (lastBurstLengthByCommandAndBank[Command::WRA][bank] == 32)
                    earliestTimeToStart = std::max(earliestTimeToStart, 
                                                   lastCommandStart + memSpec->tCCDMW + 8 * memSpec->tCK);
                else
                    earliestTimeToStart = std::max(earliestTimeToStart, 
                                                   lastCommandStart + memSpec->tCCDMW);
            }
        }

        // Power Down Exit to WR: tXP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);
    }

    else if (command == Command::PREPB)
    {
        // ACT to PREPB same bank: tRAS constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::ACT][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRAS + 2 * memSpec->tCK);

        // RD to PREPB same bank: tRTP constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::RD][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDPRE);

        // WR to PREPB same bank: tWRPRE constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::WR][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRPRE);

        // PREPB to PREPB same rank: tPPD constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tPPD);

        // Power Down Exit to PREPB: tXP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);
    }
    else if (command == Command::PREAB)
    {
        // ACT to PREAB same rank: tRAS constraint (must check all banks)
        lastCommandStart = lastScheduledByCommandAndRank[Command::ACT][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRAS + 2 * memSpec->tCK);

        // RD to PREAB same rank: tRTP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RD][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDPRE);

        // RDA to PREAB same rank: tRTP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RDA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDPRE);

        // WR to PREAB same rank: tWRPRE constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::WR][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRPRE);

        // WRA to PREAB same rank: tWRPRE constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::WRA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRPRE);

        // PREPB to PREAB same rank: tPPD constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tPPD);

        // Power Down Exit to PREAB: tXP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);

        // REFPB to PREAB same rank: tRFCpb constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRFCpb);
    }

    else if (command == Command::REFAB)
    {
        // ACT to REFAB same rank: tRC constraint (all banks must be precharged)
        lastCommandStart = lastScheduledByCommandAndRank[Command::ACT][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRC + 2 * memSpec->tCK);

        // RDA to REFAB same rank: tRDPRE + tRPpb constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RDA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + tRDPRE + memSpec->tRPpb);

        // WRA to REFAB same rank: tWRPRE + tRPpb constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::WRA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + tWRPRE + memSpec->tRPpb);

        // PREPB to REFAB same rank: tRPpb constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRPpb);

        // PREAB to REFAB same rank: tRPab constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRPab);

        // Power Down Exit to REFAB: tXP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXP][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);

        // REFAB to REFAB same rank: tRFCab constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRFCab);

        // REFPB to REFAB same rank: tRFCpb constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRFCpb);

        // Self Refresh Exit to REFAB: tXSR constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::SREFEX][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXSR);
    }
    else if (command == Command::REFPB)
    {
        // ACT to REFPB same bank: tRC constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::ACT][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRC + 2 * memSpec->tCK);

        // ACT to REFPB same rank (different bank): tRRD constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::ACT][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRRD + 2 * memSpec->tCK);

        // RDA to REFPB same bank: tRDPRE + tRPpb constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::RDA][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + tRDPRE + memSpec->tRPpb);

        // WRA to REFPB same bank: tWRPRE + tRPpb constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::WRA][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + tWRPRE + memSpec->tRPpb);

        // PREPB to REFPB same bank: tRPpb constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::PREPB][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRPpb);

        // PREAB to REFPB same rank: tRPab constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRPab);

        // Power Down Exit to REFPB: tXP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);

        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXP][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);

        // REFAB to REFPB same rank: tRFCab constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRFCab);

        // REFPB to REFPB same bank: tRFCpb constraint
        lastCommandStart = lastScheduledByCommandAndBank[Command::REFPB][bank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRFCpb);

        // REFPB to REFPB different bank: tPBR2PBR constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tPBR2PBR);

        // Self Refresh Exit to REFPB: tXSR constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::SREFEX][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXSR);

        // tFAW constraint for REFPB (similar to ACT)
        if (last4Activates[rank].size() >= 4)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           last4Activates[rank].front() + memSpec->tFAW - memSpec->tCK);
    }

    else if (command == Command::PDEA)
    {
        // ACT to PDEA: tACTPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::ACT][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tACTPDEN);

        // RD to PDEA: tRDPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RD][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDPDEN);

        // RDA to PDEA: tRDPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RDA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDPDEN);

        // WR to PDEA: tWRPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::WR][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRPDEN);

        // WRA to PDEA: tWRAPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::WRA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRAPDEN);

        // PREPB to PDEA: tPRPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tPRPDEN);

        // REFPB to PDEA: tREFPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tREFPDEN);

        // PDXA to PDEA: tCKE constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCKE);
    }
    else if (command == Command::PDXA)
    {
        // PDEA to PDXA: tCKE constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDEA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCKE);
    }
    else if (command == Command::PDEP)
    {
        // RD to PDEP: tRDPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RD][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDPDEN);

        // RDA to PDEP: tRDPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RDA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tRDPDEN);

        // WRA to PDEP: tWRAPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::WRA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tWRAPDEN);

        // PREPB to PDEP: tPRPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tPRPDEN);

        // PREAB to PDEP: tPRPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tPRPDEN);

        // REFAB to PDEP: tREFPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tREFPDEN);

        // REFPB to PDEP: tREFPDEN constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + tREFPDEN);

        // PDXP to PDEP: tCKE constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXP][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCKE);

        // SREFEX to PDEP: tXSR constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::SREFEX][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXSR);
    }
    else if (command == Command::PDXP)
    {
        // PDEP to PDXP: tCKE constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDEP][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tCKE);
    }
    else if (command == Command::SREFEN)
    {
        // ACT to SREFEN: tRC constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::ACT][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + memSpec->tRC + 2 * memSpec->tCK);

        // RDA to SREFEN: max(tRDPDEN, tRDPRE + tRPpb) constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::RDA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + std::max(tRDPDEN, tRDPRE + memSpec->tRPpb));

        // WRA to SREFEN: max(tWRAPDEN, tWRPRE + tRPpb) constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::WRA][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, 
                                           lastCommandStart + std::max(tWRAPDEN, tWRPRE + memSpec->tRPpb));

        // PREPB to SREFEN: tRPpb constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRPpb);

        // PREAB to SREFEN: tRPab constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PREAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRPab);

        // PDXP to SREFEN: tXP constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::PDXP][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXP);

        // REFAB to SREFEN: tRFCab constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFAB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRFCab);

        // REFPB to SREFEN: tRFCpb constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::REFPB][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tRFCpb);

        // SREFEX to SREFEN: tXSR constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::SREFEX][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tXSR);
    }
    else if (command == Command::SREFEX)
    {
        // SREFEN to SREFEX: tSR constraint
        lastCommandStart = lastScheduledByCommandAndRank[Command::SREFEN][rank];
        if (lastCommandStart != scMaxTime)
            earliestTimeToStart = std::max(earliestTimeToStart, lastCommandStart + memSpec->tSR);
    }
    else
        SC_REPORT_FATAL("CheckerLPDDR5", "Unknown command!");

    // Check if command bus is free
    if (lastCommandOnBus != scMaxTime)
        earliestTimeToStart = std::max(earliestTimeToStart, lastCommandOnBus + memSpec->tCK);

    // Convert from DRAM time domain to Controller time domain
    // This accounts for controller:DRAM frequency ratio (1:1, 1:2, or 1:4)
    // The conversion ensures that timing constraints are satisfied when
    // the controller runs at a lower frequency than the DRAM
    return convertToControllerTime(earliestTimeToStart);
}


void CheckerLPDDR5::insert(Command command, const tlm_generic_payload& payload)
{
    Rank rank = ControllerExtension::getRank(payload);
    BankGroup bankGroup = ControllerExtension::getBankGroup(payload);
    Bank bank = ControllerExtension::getBank(payload);

    // Convert MWR to WR and MWRA to WRA for tracking purposes
    if (command == Command::MWR)
        command = Command::WR;
    else if (command == Command::MWRA)
        command = Command::WRA;

    PRINTDEBUGMESSAGE("CheckerLPDDR5",
                      "Changing state on bank " + std::to_string(static_cast<std::size_t>(bank)) +
                          " command is " + command.toString());

    // Record command timestamp in all relevant history vectors
    lastScheduledByCommandAndBank[command][bank] = sc_time_stamp();
    lastScheduledByCommandAndBankGroup[command][bankGroup] = sc_time_stamp();
    lastScheduledByCommandAndRank[command][rank] = sc_time_stamp();
    lastScheduledByCommand[command] = sc_time_stamp();

    // Update last command on bus timestamp
    lastCommandOnBus = sc_time_stamp() + memSpec->getCommandLength(command) - memSpec->tCK;

    // Update tFAW window queue for ACT and REFPB commands
    if (command == Command::ACT || command == Command::REFPB)
    {
        if (last4Activates[rank].size() == 4)
            last4Activates[rank].pop();
        last4Activates[rank].push(lastCommandOnBus);
    }

    // Record burst length for CAS commands (for BL16/BL32 support)
    if (command.isCasCommand())
    {
        unsigned burstLength = ControllerExtension::getBurstLength(payload);
        lastBurstLengthByCommandAndBank[command][bank] = burstLength;
    }
}

} // namespace DRAMSys
