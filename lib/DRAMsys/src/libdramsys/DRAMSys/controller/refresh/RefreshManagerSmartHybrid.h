/*
 * Copyright (c) 2019, RPTU Kaiserslautern-Landau
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
 * Author: SmartHybrid Refresh Manager
 */

/**
 * RefreshManagerSmartHybrid.h
 * 混合策略刷新管理器 (针对 LPDDR4)
 * - 低负载: 使用 Per-Bank Refresh (REFPB)，减少干扰
 * - 高负载: 切换到 All-Bank Refresh (REFAB) 并强制 Precharge，快速还债
 */

#ifndef REFRESHMANAGERSMARTHYBRID_H
#define REFRESHMANAGERSMARTHYBRID_H

#include "DRAMSys/configuration/Configuration.h"
#include "DRAMSys/configuration/memspec/MemSpec.h"
#include "DRAMSys/controller/checker/CheckerIF.h"
#include "DRAMSys/controller/refresh/RefreshManagerIF.h"

#include <list>
#include <systemc>
#include <tlm>
#include <vector>
#include <unordered_map>

namespace DRAMSys
{

class BankMachine;
class PowerDownManagerIF;

class RefreshManagerSmartHybrid final : public RefreshManagerIF
{
public:
    RefreshManagerSmartHybrid(const Configuration& config,
                              ControllerVector<Bank, BankMachine*>& bankMachinesOnRank,
                              PowerDownManagerIF& powerDownManager,
                              Rank rank);

    CommandTuple::Type getNextCommand() override;
    void evaluate() override;
    void update(Command command) override;
    sc_core::sc_time getTimeForNextTrigger() override;

private:
    enum class State
    {
        Regular,
        Pulledin
    } state = State::Regular;

    const MemSpec& memSpec;
    PowerDownManagerIF& powerDownManager;

    // 用于 Per-Bank 模式的 Payload
    std::unordered_map<BankMachine*, tlm::tlm_generic_payload> refreshPayloadsPB;
    // 用于 All-Bank 模式的 Payload
    tlm::tlm_generic_payload refreshPayloadAB;

    sc_core::sc_time timeForNextTrigger = sc_core::sc_max_time();
    Command nextCommand = Command::NOP;
    tlm::tlm_generic_payload* currentPayloadPtr = nullptr;

    // Per-Bank 状态追踪
    std::list<BankMachine*> remainingBankMachines;
    std::list<BankMachine*> allBankMachines;
    std::list<BankMachine*>::iterator currentIterator;

    // 全局状态追踪 (用于 All-Bank 判断)
    int activatedBanks = 0;
    int flexibilityCounter = 0;
    const int maxPostponed;
    const int maxPulledin;

    // [任务关键] 切换到 REFAB + 强制 Precharge 的阈值
    const int panicThreshold;
    
    // Panic 模式状态标记，确保一次 Panic 只发一次 REFAB
    bool inPanicMode = false;

    bool sleeping = false;
    bool skipSelection = false;

    const sc_core::sc_time scMaxTime = sc_core::sc_max_time();
};

} // namespace DRAMSys

#endif // REFRESHMANAGERSMARTHYBRID_H
