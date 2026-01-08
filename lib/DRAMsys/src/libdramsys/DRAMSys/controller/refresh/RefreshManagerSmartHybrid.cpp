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
 * RefreshManagerSmartHybrid.cpp
 * 混合策略刷新管理器实现
 */

#include "RefreshManagerSmartHybrid.h"

#include "DRAMSys/controller/BankMachine.h"
#include "DRAMSys/controller/powerdown/PowerDownManagerIF.h"

#include <iostream>
#include <algorithm>

using namespace sc_core;
using namespace tlm;

namespace DRAMSys
{

RefreshManagerSmartHybrid::RefreshManagerSmartHybrid(
    const Configuration& config,
    ControllerVector<Bank, BankMachine*>& bankMachinesOnRank,
    PowerDownManagerIF& powerDownManager,
    Rank rank) :
    memSpec(*config.memSpec),
    powerDownManager(powerDownManager),
    // LPDDR4 Per-Bank 计数器 (banksPerRank * limit)
    maxPostponed(static_cast<int>(config.refreshMaxPostponed * memSpec.banksPerRank)),
    maxPulledin(-static_cast<int>(config.refreshMaxPulledin * memSpec.banksPerRank)),
    // [设定阈值] 当积压超过 Max 的一半时，认为进入"高负载/拥堵"状态
    // 可以通过修改这里的计算方式来调整阈值
    panicThreshold(static_cast<int>(config.refreshMaxPostponed * memSpec.banksPerRank) / 2)
{
    // LPDDR4 的基础节拍是 tREFIpb (Per-Bank Interval)
    timeForNextTrigger = getTimeForFirstTrigger(
        memSpec.tCK, memSpec.getRefreshIntervalPB(), rank, memSpec.ranksPerChannel);

    // 初始化 Per-Bank 列表
    for (auto* it : bankMachinesOnRank)
    {
        setUpDummy(refreshPayloadsPB[it], 0, rank, it->getBankGroup(), it->getBank());
        allBankMachines.push_back(it);
    }

    // 初始化 All-Bank Payload
    setUpDummy(refreshPayloadAB, 0, rank);

    remainingBankMachines = allBankMachines;
    currentIterator = remainingBankMachines.begin();

    std::cout << "[SmartHybrid] Initialized for Rank " << static_cast<unsigned>(rank)
              << "! PanicThreshold=" << panicThreshold 
              << " MaxPostponed=" << maxPostponed 
              << " BanksPerRank=" << memSpec.banksPerRank << std::endl;
}

CommandTuple::Type RefreshManagerSmartHybrid::getNextCommand()
{
    return {nextCommand, currentPayloadPtr, SC_ZERO_TIME};
}


void RefreshManagerSmartHybrid::evaluate()
{
    nextCommand = Command::NOP;
    currentPayloadPtr = nullptr;

    if (sc_time_stamp() >= timeForNextTrigger)
    {
        powerDownManager.triggerInterruption();

        if (sleeping)
            return;

        // 更新时间窗 (每次 tREFIpb)
        if (sc_time_stamp() >= timeForNextTrigger + memSpec.getRefreshIntervalPB())
        {
            timeForNextTrigger += memSpec.getRefreshIntervalPB();
            state = State::Regular;
        }

        if (state == State::Regular)
        {
            // =========================================================
            // [任务 a & b 核心逻辑]
            // =========================================================

            // 1. 判断是否进入 "Panic Mode" (高负载 -> 切换方式为 REFAB)
            if (flexibilityCounter >= panicThreshold && !inPanicMode)
            {
                inPanicMode = true;
                std::cout << "@" << sc_time_stamp() 
                          << " [SmartHybrid] Panic! Count=" << flexibilityCounter 
                          << " >= Threshold=" << panicThreshold
                          << " -> Switching to REFAB strategy." << std::endl;
            }

            if (inPanicMode)
            {
                // 策略：使用 All-Bank Refresh 快速清债
                // 前置条件：所有 Bank 必须关闭 (REFAB 要求)
                if (activatedBanks > 0)
                {
                    // [任务 b] 高于阈值 -> 强制所有 Bank Precharge
                    nextCommand = Command::PREAB;
                    currentPayloadPtr = &refreshPayloadAB;
                    std::cout << "@" << sc_time_stamp() 
                              << " [SmartHybrid] Forcing PREAB before REFAB (activatedBanks=" 
                              << activatedBanks << ")" << std::endl;
                }
                else
                {
                    // 所有 Bank 已关闭 -> 发送 REFAB
                    nextCommand = Command::REFAB;
                    currentPayloadPtr = &refreshPayloadAB;
                    std::cout << "@" << sc_time_stamp() 
                              << " [SmartHybrid] Issuing REFAB to clear debt (flexibilityCounter=" 
                              << flexibilityCounter << ")" << std::endl;
                }
                return; // 直接返回，不再处理 Per-Bank 逻辑
            }

            // 2. 普通模式 (Low Load -> 维持 REFPB 方式)
            bool forcedRefresh = (flexibilityCounter == maxPostponed);
            bool allBanksBusy = true;

            if (!skipSelection)
            {
                currentIterator = remainingBankMachines.begin();

                for (auto it = remainingBankMachines.begin(); it != remainingBankMachines.end(); it++)
                {
                    if ((*it)->isIdle())
                    {
                        currentIterator = it;
                        allBanksBusy = false;
                        break;
                    }
                }
            }

            if (allBanksBusy && !forcedRefresh)
            {
                // 都在忙，且没到强制刷新阈值 -> 选择推迟 (Postpone)
                flexibilityCounter++;
                timeForNextTrigger += memSpec.getRefreshIntervalPB();
                return;
            }

            // 找到空闲 Bank 或强制刷新 -> 准备发送 REFPB
            if ((*currentIterator)->isActivated())
            {
                nextCommand = Command::PREPB;
                currentPayloadPtr = &refreshPayloadsPB.at(*currentIterator);
                
                if (forcedRefresh)
                {
                    (*currentIterator)->block();
                    skipSelection = true;
                }
            }
            else
            {
                nextCommand = Command::REFPB;
                currentPayloadPtr = &refreshPayloadsPB.at(*currentIterator);
                std::cout << "[REFRESH] REFPB (Regular) Bank " 
                          << static_cast<std::size_t>((*currentIterator)->getBank())
                          << " at " << sc_time_stamp() 
                          << " (flexibilityCounter: " << flexibilityCounter << ")" << std::endl;

                if (forcedRefresh)
                {
                    (*currentIterator)->block();
                    skipSelection = true;
                }
            }
            return;
        }

        // Pulledin 状态处理
        bool allBanksBusy = true;

        for (auto it = remainingBankMachines.begin(); it != remainingBankMachines.end(); it++)
        {
            if ((*it)->isIdle())
            {
                currentIterator = it;
                allBanksBusy = false;
                break;
            }
        }

        if (allBanksBusy)
        {
            state = State::Regular;
            timeForNextTrigger += memSpec.getRefreshIntervalPB();
            return;
        }

        if ((*currentIterator)->isActivated())
        {
            nextCommand = Command::PREPB;
            currentPayloadPtr = &refreshPayloadsPB.at(*currentIterator);
        }
        else
        {
            nextCommand = Command::REFPB;
            currentPayloadPtr = &refreshPayloadsPB.at(*currentIterator);
            std::cout << "[REFRESH] REFPB (Pulledin) Bank " 
                      << static_cast<std::size_t>((*currentIterator)->getBank())
                      << " at " << sc_time_stamp() 
                      << " (flexibilityCounter: " << flexibilityCounter << ")" << std::endl;
        }
        return;
    }
}


void RefreshManagerSmartHybrid::update(Command command)
{
    // 1. 维护 activatedBanks 计数器 (用于 REFAB 判断)
    switch (command)
    {
    case Command::ACT:
        activatedBanks++;
        break;
    case Command::PREAB:
        activatedBanks = 0;
        break;
    case Command::PREPB:
    case Command::RDA:
    case Command::WRA:
    case Command::MWRA:
        if (activatedBanks > 0)
            activatedBanks--;
        break;
    default:
        break;
    }

    // 2. 维护刷新状态机
    switch (command)
    {
    case Command::REFPB:
        skipSelection = false;
        remainingBankMachines.erase(currentIterator);
        if (remainingBankMachines.empty())
            remainingBankMachines = allBankMachines;
        currentIterator = remainingBankMachines.begin();

        // 在 Hybrid 模式下，成功发送 REFPB 应该减少 flexibilityCounter
        // 无论是 Regular 还是 Pulledin 状态
        if (state == State::Pulledin)
        {
            flexibilityCounter--;
        }
        else
        {
            // Regular 状态发送 REFPB，切换到 Pulledin
            state = State::Pulledin;
        }

        // 检查是否达到最大 Pulledin 限制
        if (flexibilityCounter == maxPulledin)
        {
            state = State::Regular;
            timeForNextTrigger += memSpec.getRefreshIntervalPB();
        }
        break;

    case Command::REFAB:
        // [关键] 发生了 REFAB (无论是我们发的，还是外部发的)
        // 这相当于一次性完成了所有 Bank 的本轮刷新
        
        if (sleeping)
        {
            // Refresh command after SREFEX
            state = State::Regular;
            timeForNextTrigger = sc_time_stamp() + memSpec.getRefreshIntervalPB();
            sleeping = false;
        }
        else
        {
            // 正常 REFAB 完成
            state = State::Regular;
            
            // REFAB 后使用 tREFI (All-Bank 间隔) 作为下一次触发时间
            // 因为 REFAB 刷新了所有 bank，相当于完成了一轮完整刷新
            timeForNextTrigger = sc_time_stamp() + memSpec.getRefreshIntervalAB();
        }

        // 重置 Per-Bank 队列，因为大家都被刷过了
        remainingBankMachines = allBankMachines;
        currentIterator = remainingBankMachines.begin();
        skipSelection = false;

        // [关键] 既然刷了所有 Bank，大幅降低 flexibilityCounter
        // 一次 REFAB 抵消 banksPerRank 次的计数
        if (flexibilityCounter > 0)
        {
            int reduction = static_cast<int>(memSpec.banksPerRank);
            flexibilityCounter = std::max(0, flexibilityCounter - reduction);
            std::cout << "@" << sc_time_stamp() 
                      << " [SmartHybrid] REFAB completed! flexibilityCounter reduced by " 
                      << reduction << " to " << flexibilityCounter << std::endl;
        }

        // 退出 Panic 模式
        if (inPanicMode)
        {
            inPanicMode = false;
            std::cout << "@" << sc_time_stamp() 
                      << " [SmartHybrid] Exiting Panic mode, returning to REFPB strategy." 
                      << std::endl;
        }
        break;

    case Command::PDEA:
    case Command::PDEP:
        sleeping = true;
        break;

    case Command::SREFEN:
        sleeping = true;
        timeForNextTrigger = scMaxTime;
        break;

    case Command::PDXA:
    case Command::PDXP:
        sleeping = false;
        break;

    default:
        break;
    }
}

sc_time RefreshManagerSmartHybrid::getTimeForNextTrigger()
{
    return timeForNextTrigger;
}

} // namespace DRAMSys
