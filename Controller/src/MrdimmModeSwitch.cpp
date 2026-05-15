#include "Controller/MrdimmModeSwitch.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Common/CommonDefine.hh"
#include "Common/logger.hh"
#include "sysc/kernel/sc_simcontext.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <sstream>
namespace dmu{
    namespace Controller{

/*
模式切换规则：
1.读模式下：
    (1) CCT存在 rd命令是 expired 并且 page-hit 并且 不满足tRCD时序，是处于Activing 状态，保持读模式
    (2) CCT存在 wr命令是 expired 并且 page-hit 并且 不满足tRCD时序，是处于Activing 状态，切换写模式
    (3) CCT存在 rd命令是 地址冲突 并且 page-hit 并且 不满足tRCD时序，是处于Activing 状态，保持读模式
    (4) CCT存在 wr命令是 地址冲突 并且 page-hit 并且 不满足tRCD时序，是处于Activing 状态，切换写模式
    (5) 两个 pc 中的 cam 都是 hpr/lpr critical 并且 page-hit 并且 不满足tRCD时序，是处于Activing 状态，保持读模式
    (6) 两个 pc 中的 cam 都是 tpw critical 并且 page-hit 并且 满足tRCD时序，已经是处于active 状态了，切换写模式
    (7) pc0 和 pc1的 CCT 中可执行读命令数量  >= RD_CNT_THR, 保持读模式
    (8) pc0 和 pc1的 CCT 中可执行写命令数量  >= WR_CNT_THR, 切换写模式
    (9) pc0 和 pc1的 CCT 中有效命令数量  < RD_CNT_THR,并且 >0
        9.1 pc1 的 CCT 中有效命令数量 < WR_CNT_THR, 可以提前为 写方向的命令 开行
        9.2 pc0 的 CCT 中有效命令数量 < WR_CNT_THR, 可以提前为 写方向的命令 开行
    (10) pc0 和 pc1的 CCT中有效 命令数量 < WR_CNT_THR, 并且 >0
        10.1 pc0和 pc1 可以提前为写方向命令进行开行
        10.2 如果此时switch_to_wr_delay 时间满足，切换到写模式，不满足，维持读模式
    (11) pc 0 或者 pc 1 任何一个伪通道存在 cct有可执行的rd 命令，保持读模式
    (12) pc 0 或者 pc 1 任何一个伪通道存在 cct有可执行的wr 命令
        12.1 写Cam数量大于SW_WR_CAM_THR ，切换到写方向，并且为写方向进行开行，否则保持读方向
    (13) pc 0 和 pc 1 都不存在 cct 的rd命令
        13.1 至少一个伪通道存在 cct的wr命令，切换到写方向，否则判断prefer write，如果使能prefer write，切换到写方向，否则保持读方向
    (14) pc 0 和 pc 1只有一个伪通道存在 rd 命令
        14.1 如果pc0和pc1 都存在写命令，此时为写方向进行开行，并且判断Wr Cam的深度大于SW_WR_CAM_THR
    (15) 保持读方向

2.写模式下：
    (1) CCT存在 rd命令时 expired 并且 page-hit 并且 不满足tRCD时序，是处于Activing 状态，切换读模式
    (2) CCT存在 wr命令是 expired 并且 page-hit 并且 不满足tRCD时序，是处于Activing 状态，保持写模式
    (3) CCT存在 rd命令是 地址冲突 并且 page-hit 并且 不满足tRCD时序，是处于Activing 状态，切换读模式
    (4) CCT存在 wr命令是 地址冲突 并且 page-hit 满足tRCD时序，是处于Actived 状态，保持写模式
    (5) CCT存在 rd命令是 (lpr或者hpr) critical 并且 page-hit 并且 不满足tRCD时序，是处于Activing 状态，切换读模式
    (6) CCT存在 wr命令是 tpw critical bingqie 满足tRCD时序，是处于Actived 状态，保持写模式
    (7) CCT Wr avail 命令数量 都 >= WR_CNT_THR, 保持写模式
    (8) CCT Rd avail 命令数量 都 >= RD_CNT_THR, 切换读模式
    (9) CCT Wr avail 命令数量 都 < WR_CNT_THR, 并且 >0,
        9.1 如果 某个PC 满足 <RD_THR_CNT, 提前为该PC进行读方向开行
    (10) CCT Rd avail 命令数量 都 > RD_CNT_THR, 并且 >0: 则
        10.1 切换读模式
        10.2 PC0和PC1 的读方向都进行提前开行
    (11) PC 0 或者 pc1 存在可执行的 wr cmd, 保持写模式
    (12) pc0 或者 pc1存在可执行的 rd cmd, 切换读模式
    (13) pc0 和 pc1 不存在 有效的wr cmd
        13.1 pc0或者pc1 存在有效的rd cmd，切换到读模式，否则：
            13.1.1 prefer write 生效 ， 维持写模式，否则切换到读模式
    (14) pc0 和 pc1 都存在有效的wr cmd, 维持写模式，否则：
        14.1 pc0和pc1 都存在有效的rd cmd, 切换到读模式, pc0和pc1 都为读方向提前开行, 否则
            14.1.1 维持写模式
*/

MrdimmModeSwitch::MrdimmModeSwitch(const Configure& config, scheduler_vec& pch_scheduler, bankslice_manager_vec& pch_bank_manager)
: m_config(config)
, m_pch_schedulers(pch_scheduler)
, m_pch_bank_managers(pch_bank_manager)
, RD_CNT_THR_EN(config.controller_config->RD_CNT_THR_EN)
, RD_CNT_THR(config.controller_config->RD_CNT_THR)
, WR_CNT_THR_EN(config.controller_config->WR_CNT_THR_EN)
, WR_CNT_THR(config.controller_config->WR_CNT_THR)
, SW_WR_DELAY_EN(config.controller_config->SW_WR_DELAY_EN)
, SW_WR_DELAY(config.controller_config->SW_WR_DELAY * config.mem_spec->tCK_mc)
, PREFER_WR(config.controller_config->PREFER_WR)
, SW_WR_CAM_THR_EN(config.controller_config->SW_WR_CAM_THR_EN)
, SW_WR_CAM_THR(config.controller_config->SW_WR_CAM_THR)
, switch_to_write_delay_ready_time(sc_core::sc_max_time())
{

};

void
MrdimmModeSwitch::ResetSchedulerInfo()
{
    unsigned pch_num = m_config.mem_spec->NumOfPseudoChannelsPerSubChannel;

    advance_prepare_wr.clear();
    advance_prepare_wr.resize(pch_num, false);

    advance_prepare_rd.clear();
    advance_prepare_rd.resize(pch_num, false);

    pch_expired_rd_page_hit_activing.clear();
    pch_expired_rd_page_hit_activing.resize(pch_num, false);

    pch_expired_wr_page_hit_activing.clear();
    pch_expired_wr_page_hit_activing.resize(pch_num, false);

    pch_flush_rd_page_hit_activing.clear();
    pch_flush_rd_page_hit_activing.resize(pch_num, false);

    pch_flush_wr_page_hit_activing.clear();
    pch_flush_wr_page_hit_activing.resize(pch_num, false);

    pch_flush_wr_page_hit_actived.clear();
    pch_flush_wr_page_hit_actived.resize(pch_num, false);

    pch_rd_critical_page_hit_activing.clear();
    pch_rd_critical_page_hit_activing.resize(pch_num, false);

    pch_wr_critical_page_hit_actived.clear();
    pch_wr_critical_page_hit_actived.resize(pch_num, false);

    pch_rd_ntt_avail_cmd_num.clear();
    pch_rd_ntt_avail_cmd_num.resize(pch_num, 0);

    pch_wr_ntt_avail_cmd_num.clear();
    pch_wr_ntt_avail_cmd_num.resize(pch_num, 0);

    pch_rd_ntt_valid_cmd_num.clear();
    pch_rd_ntt_valid_cmd_num.resize(pch_num, 0);

    pch_wr_ntt_valid_cmd_num.clear();
    pch_wr_ntt_valid_cmd_num.resize(pch_num, 0);

    pch_wr_cam_cmd_num.clear();
    pch_wr_cam_cmd_num.resize(pch_num, 0);
}

void
MrdimmModeSwitch::GetSchedulerInfo()
{
    for(size_t pch_index=0; pch_index< m_config.mem_spec->NumOfPseudoChannelsPerSubChannel;pch_index++)
    {
        m_pch_bank_managers[pch_index]->UpdateCriticalState();
        pch_expired_rd_page_hit_activing[pch_index] = m_pch_bank_managers[pch_index]->HasRdNttCmdPageHitExpiredActiving();
        pch_expired_wr_page_hit_activing[pch_index] = m_pch_bank_managers[pch_index]->HasWrNttCmdPageHitExpiredActiving();
        pch_flush_rd_page_hit_activing[pch_index] = m_pch_bank_managers[pch_index]->HasRdNttCmdPageHitFlushActiving();
        pch_flush_wr_page_hit_activing[pch_index] = m_pch_bank_managers[pch_index]->HasWrNttCmdPageHitFlushActiving();
        pch_flush_wr_page_hit_actived[pch_index] = m_pch_bank_managers[pch_index]->HasWrNttCmdPageHitFlush();

        pch_rd_critical_page_hit_activing[pch_index] = m_pch_bank_managers[pch_index]->HasHprLprCriticalPageHitActiving();
        pch_wr_critical_page_hit_actived[pch_index] = m_pch_bank_managers[pch_index]->HasTpwCriticalPageHitActived();

        pch_rd_ntt_avail_cmd_num[pch_index] = m_pch_bank_managers[pch_index]->GetRdNttAvailRdCmdNum();
        pch_wr_ntt_avail_cmd_num[pch_index] = m_pch_bank_managers[pch_index]->GetWrNttAvailWrCmdNum();

        pch_rd_ntt_valid_cmd_num[pch_index] = m_pch_bank_managers[pch_index]->GetRdNttValidRdCmdNum();
        pch_wr_ntt_valid_cmd_num[pch_index] = m_pch_bank_managers[pch_index]->GetWrNttValidWrCmdNum();

        pch_wr_cam_cmd_num[pch_index] = m_pch_bank_managers[pch_index]->GetWrCamValidCmdNum();

    }

    std::ostringstream oss;
    oss << "\nSchedulerInfo:";
    for (size_t pch_index = 0; pch_index < m_config.mem_spec->NumOfPseudoChannelsPerSubChannel; pch_index++)
    {
        oss << "\nPCH[" << pch_index << "]: "
            << "ValidNtt=(RD=" << pch_rd_ntt_valid_cmd_num[pch_index]
            << " WR=" << pch_wr_ntt_valid_cmd_num[pch_index] << ") "
            << "AvailCmd=(RD=" << pch_rd_ntt_avail_cmd_num[pch_index]
            << " WR=" << pch_wr_ntt_avail_cmd_num[pch_index] << ") "
            << "ValidCAM=(RD=" << m_pch_bank_managers[pch_index]->GetRdCamValidCmdNum()
            << " WR=" << pch_wr_cam_cmd_num[pch_index] << ") "
            << "Critical=(HPR=" << m_pch_bank_managers[pch_index]->IsHprCritical()
            << " LPR=" << m_pch_bank_managers[pch_index]->IsLprCritical()
            << " TPW=" << m_pch_bank_managers[pch_index]->IsTpwCritical() << ") ";
    }
    DMU_LOG_INFO_NF("MrdimmModeSwitch", "%s", oss.str().c_str());
}

void
MrdimmModeSwitch::UpdateGlobalState()
{
    ResetSchedulerInfo();
    GetSchedulerInfo();

    bool has_expired_rd_page_hit_activing = HasAnyTrue(pch_expired_rd_page_hit_activing);
    bool has_expired_wr_page_hit_activing = HasAnyTrue(pch_expired_wr_page_hit_activing);
    bool has_flush_rd_page_hit_activing = HasAnyTrue(pch_flush_rd_page_hit_activing);
    bool has_flush_wr_page_hit_activing = HasAnyTrue(pch_flush_wr_page_hit_activing);
    bool has_flush_wr_page_hit_actived = HasAnyTrue(pch_flush_wr_page_hit_actived);
    bool has_rd_critical_page_hit_activing = HasAnyTrue(pch_rd_critical_page_hit_activing);
    bool has_wr_critical_page_hit_actived = HasAnyTrue(pch_wr_critical_page_hit_actived);

    bool both_rd_cct_avail_num_high = !pch_rd_ntt_avail_cmd_num.empty() && std::all_of(pch_rd_ntt_avail_cmd_num.begin(),
                                        pch_rd_ntt_avail_cmd_num.end(),
                                        [rd_high_threshold = RD_CNT_THR](unsigned num)
                                        { return num >= rd_high_threshold; });

    bool both_wr_cct_avail_num_high = !pch_wr_ntt_avail_cmd_num.empty() && std::all_of(pch_wr_ntt_avail_cmd_num.begin(),
                                        pch_wr_ntt_avail_cmd_num.end(),
                                        [wr_high_threshold = WR_CNT_THR](unsigned num)
                                        { return num >= wr_high_threshold; } );

    bool both_rd_cct_avail_num_medium = !pch_rd_ntt_avail_cmd_num.empty() && std::all_of(pch_rd_ntt_avail_cmd_num.begin(),
                                        pch_rd_ntt_avail_cmd_num.end(),
                                        [rd_high_threshold = RD_CNT_THR, rd_low_threshold = 0](unsigned num)
                                        { return num > rd_low_threshold && num < rd_high_threshold; });

    bool both_wr_cct_avail_num_medium = !pch_wr_ntt_avail_cmd_num.empty() && std::all_of(pch_wr_ntt_avail_cmd_num.begin(),
                                        pch_wr_ntt_avail_cmd_num.end(),
                                        [wr_high_threshold = WR_CNT_THR, wr_low_threshold = 0](unsigned num)
                                        { return num > wr_low_threshold && num < wr_high_threshold; });

    bool has_rd_cct_avail_cmd = std::any_of(pch_rd_ntt_avail_cmd_num.begin(),
                                        pch_rd_ntt_avail_cmd_num.end(),
                                        [](unsigned num)
                                        { return num > 0; });

    bool has_wr_cct_avail_cmd = std::any_of(pch_wr_ntt_avail_cmd_num.begin(),
                                        pch_wr_ntt_avail_cmd_num.end(),
                                        [](unsigned num)
                                        { return num > 0; });

    bool both_rd_cct_no_valid_cmd = !pch_rd_ntt_valid_cmd_num.empty() &&
                                    std::all_of(pch_rd_ntt_valid_cmd_num.begin(),
                                    pch_rd_ntt_valid_cmd_num.end(),
                                    [](unsigned num)
                                    { return num == 0; }
                                    );

    bool has_wr_cct_valid_cmd = std::any_of(pch_wr_ntt_valid_cmd_num.begin(),
                                    pch_wr_ntt_valid_cmd_num.end(),
                                    [](unsigned num)
                                    { return num > 0; });

    bool both_wr_cct_no_valid_cmd = !pch_wr_ntt_valid_cmd_num.empty() &&
                                    std::all_of(pch_wr_ntt_valid_cmd_num.begin(),
                                    pch_wr_ntt_valid_cmd_num.end(),
                                    [](unsigned num)
                                    { return num == 0; });

    bool has_rd_cct_valid_cmd = std::any_of(pch_rd_ntt_valid_cmd_num.begin(),
                                    pch_rd_ntt_valid_cmd_num.end(),
                                    [](unsigned num)
                                    { return num > 0; });

    bool only_one_pch_has_rd_valid_cmd = (std::count_if(pch_rd_ntt_valid_cmd_num.begin(),
                                    pch_rd_ntt_valid_cmd_num.end(),
                                    [](unsigned num) { return num > 0; }) == 1);

    bool both_wr_cct_has_valid_cmd = !pch_wr_ntt_valid_cmd_num.empty() &&
                                    std::all_of(pch_wr_ntt_valid_cmd_num.begin(),
                                    pch_wr_ntt_valid_cmd_num.end(),
                                    [](unsigned num)
                                    { return num > 0; });

    bool both_rd_cct_has_valid_cmd = !pch_rd_ntt_valid_cmd_num.empty() &&
                                    std::all_of(pch_rd_ntt_valid_cmd_num.begin(),
                                    pch_rd_ntt_valid_cmd_num.end(),
                                    [](unsigned num)
                                    { return num > 0; }
                                    );

    bool both_wr_cmd_num_above_threshold = std::all_of(pch_wr_cam_cmd_num.begin(),
                                    pch_wr_cam_cmd_num.end(),
                                    [wr_cam_threshold = this->SW_WR_CAM_THR](unsigned num)
                                    { return num > wr_cam_threshold; });

    GlobalRdWrState old_state = current_state;
    unsigned num_pch = m_config.mem_spec->NumOfPseudoChannelsPerSubChannel;

    DMU_LOG_INFO_NF("MrdimmModeSwitch",
        "[MrdimmModeSwitch] UpdateGlobalState start: old_state=%s, "
        "expired_rd_hit_activing=%d expired_wr_hit_activing=%d flush_rd_hit_activing=%d flush_wr_hit_activing=%d flush_wr_hit_actived=%d "
        "rd_critical_hit_activing=%d wr_critical_hit_actived=%d both_rd_avail_high=%d both_wr_avail_high=%d both_rd_avail_medium=%d both_wr_avail_medium=%d "
        "has_rd_avail=%d has_wr_avail=%d both_rd_no_valid=%d has_wr_valid=%d both_wr_no_valid=%d has_rd_valid=%d "
        "one_pch_rd_valid=%d both_wr_valid=%d both_rd_valid=%d both_wr_cam_above=%d",
        PrintState(old_state).c_str(),
        has_expired_rd_page_hit_activing, has_expired_wr_page_hit_activing,
        has_flush_rd_page_hit_activing, has_flush_wr_page_hit_activing,
        has_flush_wr_page_hit_actived, has_rd_critical_page_hit_activing,
        has_wr_critical_page_hit_actived, both_rd_cct_avail_num_high,
        both_wr_cct_avail_num_high, both_rd_cct_avail_num_medium,
        both_wr_cct_avail_num_medium, has_rd_cct_avail_cmd,
        has_wr_cct_avail_cmd, both_rd_cct_no_valid_cmd,
        has_wr_cct_valid_cmd, both_wr_cct_no_valid_cmd,
        has_rd_cct_valid_cmd, only_one_pch_has_rd_valid_cmd,
        both_wr_cct_has_valid_cmd, both_rd_cct_has_valid_cmd,
        both_wr_cmd_num_above_threshold);

    if(current_state == GlobalRdWrState::Rd)
    {
        if(has_expired_rd_page_hit_activing){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit has_expired_rd_page_hit_activing, state %s -> Rd",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
        else if(has_expired_wr_page_hit_activing){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit has_expired_wr_page_hit_activing, state %s -> Wr",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
        else if(has_flush_rd_page_hit_activing){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit has_flush_rd_page_hit_activing, state %s -> Rd",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
        else if(has_flush_wr_page_hit_activing){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit has_flush_wr_page_hit_activing, state %s -> Wr",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
        else if(has_rd_critical_page_hit_activing){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit has_rd_critical_page_hit_activing, state %s -> Rd",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
        else if(has_wr_critical_page_hit_actived){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit has_wr_critical_page_hit_actived, state %s -> Wr",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
        else if(both_rd_cct_avail_num_high){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit both_rd_cct_avail_num_high, state %s -> Rd",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
        else if(both_wr_cct_avail_num_high){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit both_wr_cct_avail_num_high, state %s -> Wr",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
        else if(both_rd_cct_avail_num_medium){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit both_rd_cct_avail_num_medium, state %s -> Rd",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            for(size_t pch_index = 0; pch_index < num_pch; pch_index++)
            {
                advance_prepare_wr[pch_index] = pch_wr_ntt_avail_cmd_num[pch_index] < WR_CNT_THR;
            }
            return;
        }
        else if(both_wr_cct_avail_num_medium){
            for(size_t pch_index = 0; pch_index < num_pch; pch_index++)
            {
                advance_prepare_wr[pch_index] = true;
            }
            // 首次进入该分支，启动计时器
            if(!m_wr_delay_active){
                switch_to_write_delay_ready_time = sc_core::sc_time_stamp() + SW_WR_DELAY;
                m_wr_delay_active = true;
            }
            // 检查是否到期
            if(sc_core::sc_time_stamp() >= switch_to_write_delay_ready_time){
                current_state = GlobalRdWrState::Wr;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Rd mode: hit both_wr_cct_avail_num_medium (delay expired), state %s -> Wr",
                    PrintState(old_state).c_str());
                ResetWrDelayTimer(); // 切换后重置
                return;
            }
            else{
                current_state = GlobalRdWrState::Rd;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Rd mode: hit both_wr_cct_avail_num_medium (delay not expired), state %s -> Rd",
                    PrintState(old_state).c_str());
                return;
            }
        }
        else if(has_rd_cct_avail_cmd){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit has_rd_cct_avail_cmd, state %s -> Rd",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
        else if(has_wr_cct_avail_cmd){
            ResetWrDelayTimer();
            if(both_wr_cmd_num_above_threshold){
                current_state = GlobalRdWrState::Wr;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Rd mode: hit has_wr_cct_avail_cmd (wr_cam_above_threshold), state %s -> Wr",
                    PrintState(old_state).c_str());
                for(size_t pch_index = 0; pch_index < num_pch; pch_index++)
                {
                    advance_prepare_wr[pch_index] = true;
                }
                return;
            }
            else{
                current_state = GlobalRdWrState::Rd;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Rd mode: hit has_wr_cct_avail_cmd (not above threshold), state %s -> Rd",
                    PrintState(old_state).c_str());
                return;
            }
        }
        else if(both_rd_cct_no_valid_cmd){
            ResetWrDelayTimer();
            if(has_wr_cct_valid_cmd){
                current_state = GlobalRdWrState::Wr;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Rd mode: hit both_rd_cct_no_valid_cmd & has_wr_cct_valid_cmd, state %s -> Wr",
                    PrintState(old_state).c_str());
                return;
            }
            else{
                current_state = PREFER_WR ? GlobalRdWrState::Wr : GlobalRdWrState::Rd;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Rd mode: hit both_rd_cct_no_valid_cmd & no_wr_valid (PREFER_WR=%d), state %s -> %s",
                    PREFER_WR, PrintState(old_state).c_str(), PrintState(current_state).c_str());
                return;
            }
        }
        else if(only_one_pch_has_rd_valid_cmd){
            ResetWrDelayTimer();
            if(both_wr_cct_has_valid_cmd){
                for(size_t pch_index = 0; pch_index < num_pch; pch_index++)
                {
                    advance_prepare_wr[pch_index] = true;
                }
                current_state = both_wr_cmd_num_above_threshold ? GlobalRdWrState::Wr : GlobalRdWrState::Rd;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Rd mode: hit only_one_pch_has_rd_valid_cmd & both_wr_cct_has_valid_cmd (wr_cam_above=%d), state %s -> %s",
                    both_wr_cmd_num_above_threshold, PrintState(old_state).c_str(), PrintState(current_state).c_str());
                return;
            }
            else{
                current_state = GlobalRdWrState::Rd;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Rd mode: hit only_one_pch_has_rd_valid_cmd & no_both_wr_valid, state %s -> Rd",
                    PrintState(old_state).c_str());
                return;
            }
        }
        else{
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Rd mode: hit default fallback, state %s -> Rd",
                PrintState(old_state).c_str());
            ResetWrDelayTimer();
            return;
        }
    }
    else if(current_state == GlobalRdWrState::Wr)
    {
        if(has_expired_rd_page_hit_activing){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit has_expired_rd_page_hit_activing, state %s -> Rd",
                PrintState(old_state).c_str());
            return;
        }
        else if(has_expired_wr_page_hit_activing){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit has_expired_wr_page_hit_activing, state %s -> Wr",
                PrintState(old_state).c_str());
            return;
        }
        else if(has_flush_rd_page_hit_activing){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit has_flush_rd_page_hit_activing, state %s -> Rd",
                PrintState(old_state).c_str());
            return;
        }
        else if(has_flush_wr_page_hit_actived){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit has_flush_wr_page_hit_actived, state %s -> Wr",
                PrintState(old_state).c_str());
            return;
        }
        else if(has_rd_critical_page_hit_activing){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit has_rd_critical_page_hit_activing, state %s -> Rd",
                PrintState(old_state).c_str());
            return;
        }
        else if(has_wr_critical_page_hit_actived){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit has_wr_critical_page_hit_actived, state %s -> Wr",
                PrintState(old_state).c_str());
            return;
        }
        else if(both_wr_cct_avail_num_high){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit both_wr_cct_avail_num_high, state %s -> Wr",
                PrintState(old_state).c_str());
            return;
        }
        else if(both_rd_cct_avail_num_high){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit both_rd_cct_avail_num_high, state %s -> Rd",
                PrintState(old_state).c_str());
            return;
        }
        else if(both_wr_cct_avail_num_medium){
            for(size_t pch_index = 0; pch_index < num_pch; pch_index++)
            {
                advance_prepare_rd[pch_index] = pch_rd_ntt_avail_cmd_num[pch_index] < RD_CNT_THR;
            }
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit both_wr_cct_avail_num_medium, state %s -> Wr",
                PrintState(old_state).c_str());
            return;
        }
        else if(both_rd_cct_avail_num_medium){
            for(size_t pch_index = 0; pch_index < num_pch; pch_index++)
            {
                advance_prepare_rd[pch_index] = true;
            }
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit both_rd_cct_avail_num_medium, state %s -> Rd",
                PrintState(old_state).c_str());
            return;
        }
        else if(has_wr_cct_avail_cmd){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit has_wr_cct_avail_cmd, state %s -> Wr",
                PrintState(old_state).c_str());
            return;
        }
        else if(has_rd_cct_avail_cmd){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit has_rd_cct_avail_cmd, state %s -> Rd",
                PrintState(old_state).c_str());
            return;
        }
        else if(both_wr_cct_no_valid_cmd){
            if(has_rd_cct_valid_cmd){
                current_state = GlobalRdWrState::Rd;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Wr mode: hit both_wr_cct_no_valid_cmd & has_rd_cct_valid_cmd, state %s -> Rd",
                PrintState(old_state).c_str());
                return;
            }
            else{
                current_state = PREFER_WR ? GlobalRdWrState::Wr : GlobalRdWrState::Rd;
                DMU_LOG_INFO_NF("MrdimmModeSwitch",
                    "[MrdimmModeSwitch] Wr mode: hit both_wr_cct_no_valid_cmd & no_rd_valid (PREFER_WR=%d), state %s -> %s",
                    PREFER_WR, PrintState(old_state).c_str(), PrintState(current_state).c_str());
                return;
            }
        }
        else if(both_rd_cct_has_valid_cmd){
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit both_rd_cct_has_valid_cmd, state %s -> Wr",
                PrintState(old_state).c_str());
            return;
        }
        else if(both_rd_cct_has_valid_cmd){
            current_state = GlobalRdWrState::Rd;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit both_rd_cct_has_valid_cmd, state %s -> Rd",
                PrintState(old_state).c_str());
            for(size_t pch_index = 0; pch_index < num_pch; pch_index++)
            {
                advance_prepare_rd[pch_index] = true;
            }
            return;
        }
        else{
            current_state = GlobalRdWrState::Wr;
            DMU_LOG_INFO_NF("MrdimmModeSwitch",
                "[MrdimmModeSwitch] Wr mode: hit default fallback, state %s -> Wr",
                PrintState(old_state).c_str());
            return;
        }
    }
    else
    {
        std::cerr << " Invalid Rd and Wr State in Mode Switch module, the invalid state: " << PrintState(current_state)<< std::endl;
        std::abort();
    }
}

    }
}