#include "Controller/ModeSwitch.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Common/CommonDefine.hh"
#include "sysc/datatypes/fx/sc_fxdefs.h"
#include "sysc/utils/sc_report.h"
#include <cassert>

namespace dmu{
    namespace Controller{

void
ModeSwitch::UpdateGlobalState()
{
    // is_rd_mode = current_state == GlobalRdWrState::Rd || current_state == GlobalRdWrState::Rd2Wr;
    // is_wr_mode = current_state == GlobalRdWrState::Wr || current_state == GlobalRdWrState::Wr2Rd;
    is_rd_mode = (st == GlobalState::RD);
    is_wr_mode = (st == GlobalState::WR);
    // if(real_switch2rd)
    //      is_rd_mode = true;
    // if(real_switch2wr)
    //      is_wr_mode = true;
    // Get Rd Cam and has rd
    auto rd_cam = _scheduler.GetRdCam();
    auto wr_cam = _scheduler.GetWrCam();
    rd_cam->UpdateRdCriticalState();
    wr_cam->UpdateWrCriticalState();

    bool switch2wr_pending;
    bool switch2rd_pending;

    //rd cam has cmd --> rd cam cmd has bsc allocated
    bool rd_cam_has_cmd = rd_cam->IsHprAvailable() || rd_cam->IsLprAvailable();
    //wr cam has cmd --> wr cam cmd has bsc allocated and cmd data is ready(rmw dat ready, write data ready, write combine data ready)
    bool wr_cam_has_cmd = wr_cam->IsWrCamAvailable();
    // rd expired --> cmd expired
    bool rd_expired = rd_cam->IsRdCamExpired() && rd_cam_has_cmd;
    // wr expired --> cmd expired
    bool wr_expired = wr_cam->IsWrCamExpired() && wr_cam_has_cmd;

    // rd flush --> rd cmd addr collision, rd cmd need to be sent first
    bool rd_flush = _scheduler.IsRdFlush() && rd_cam_has_cmd;
    // wr flush --> wr cmd addr collision, wr cmd need to be sent first, if exist rd flush, no wr flush
    bool wr_flush = _scheduler.IsWrFlush() && wr_cam_has_cmd;

    bool hpr_critical = rd_cam->IsHprCritical();
    bool lpr_critical = rd_cam->IsLprCritical();
    bool tpw_critical = wr_cam->IsTpwCritical();

    bool rd_critical = ((hpr_critical || lpr_critical) && rd_cam_has_cmd) && !tpw_critical;
    bool wr_critical = !(hpr_critical || lpr_critical) && (tpw_critical && wr_cam_has_cmd);

    bool opposite_cmd_ok;
    //          = ((is_rd_mode && !_bank_slice_manager.IsRdAccess() && !_bank_slice_manager.IsRdWait())
    //          && (_bank_slice_manager.IsWrAccess() || _bank_slice_manager.IsWrWait()))
    //          ||
    //          ((is_wr_mode && !_bank_slice_manager.IsWrAccess() && !_bank_slice_manager.IsWrWait())
    //          && (_bank_slice_manager.IsRdAccess() || _bank_slice_manager.IsRdWait()))
    //          ;
    bool NCWEN{true};
    if(NCWEN)
    {
        opposite_cmd_ok = false;
    }
    else if((is_rd_mode && !_bank_slice_manager.IsRdAccess() && !_bank_slice_manager.IsRdWait())
            )
    {
        opposite_cmd_ok = (_bank_slice_manager.IsWrAccess() || _bank_slice_manager.IsWrWait());
    }
    else if ((is_wr_mode && !_bank_slice_manager.IsWrAccess() && !_bank_slice_manager.IsWrWait())
            )
    {
        opposite_cmd_ok = (_bank_slice_manager.IsRdAccess() || _bank_slice_manager.IsRdWait());
    }
    else{
        opposite_cmd_ok = false;
    }
    // read no write yes, below condition both satisfied
    // 1. rd mode
    // 2.       1) no rd cmd in rd cam
    // or       2) in ntt , there is no avail RD or ACT/PRE CMD (rd direction) and (wr direction) has avail WR or ACT/PRE CMD
    // 3. wr cam has cmd
    // implement with Codex
    //TODO: add the rd2wr_idle_gap_time_out logic
    //
    bool rd2wr_idle_gap_time_out{true};
    bool rd_no_wr_yes = is_rd_mode && (!rd_cam_has_cmd || opposite_cmd_ok) && wr_cam_has_cmd && rd2wr_idle_gap_time_out;

    // read yes write no, below condition both satisfied
    // 1. wr mode
    // 2.       1) no wr cmd in wr cam
    // or       2) in ntt, there is no avail WR or ACT/PRE CMD (wr direction) and (rd direction) has avail RD or ACT/PRE CMD
    // 3. rd cam has cmd
    bool rd_yes_wr_no = is_wr_mode && (!wr_cam_has_cmd || opposite_cmd_ok) && rd_cam_has_cmd;

    // priority:
    // 1. rd expired
    // 2. wr expired
    // 3. rd flush
    // 4. wr flush
    // 5. (lpr critical or hpr critical) and rd cam has avail cmd, and wr cam is not critical
    // 6. tpw critical and wr cam has avail cmd, and (lpr and hpr are both not critical)
    // 7. rd_no_wr_yes or wr_yes_wr_no
    // this will call the switch2rd or switch2wr pending request
    //

    switch2wr_pending = (is_rd_mode) &&
                        ((!rd_expired && wr_expired) ||
                        (!rd_expired && !wr_expired && !rd_flush && wr_flush) ||
                        (!rd_expired && !wr_expired && !rd_flush && !wr_flush && !rd_critical && wr_critical) ||
                        (!rd_expired && !wr_expired && !rd_flush && !wr_flush && !rd_critical && !wr_critical && rd_no_wr_yes)
                        );

    switch2rd_pending = (is_wr_mode) &&
                        ( rd_expired ||
                            (!wr_expired && rd_flush) ||
                            (!wr_expired && !wr_flush && rd_critical) ||
                            (!wr_expired && !wr_flush && !wr_critical && rd_yes_wr_no)
                        );

    if(!switch2rd_pending && !switch2wr_pending)
    {
        // DPRINT_WARNING(MODE_SWITCH,"Mode Switch switch Pending check ",
        // "neither switch to rd nor switch to wr happened, current state is %s", PrintState(current_state).c_str());
    }
    else if(switch2rd_pending && switch2wr_pending)
        DPRINT_FATAL("Mode Switch switch Pending check: ", "switch to rd and switch to wr both happened");
    //switch decide
    auto& opp_hit_num = _config.controller_config->OPP_HIT_CNT;
    // opposite wr ok
    //      1. OppHitCnt is not zero and write page hit num >= OppHitCnt
    // or   2. wr page hit num == ntt wr cmd num
    // or   3. wr page-hit cmd is being precharged
    // or   4. wr cmd is page-hit and expired

    bool opposite_wr_ok = (opp_hit_num >0 && _bank_slice_manager.GetWrNttPageHitNum() >= opp_hit_num)
                        || _bank_slice_manager.IsWrNttCmdAllPageHit()//
                        || false // need to add a function to show whether the ntt wr page-hit cmd will be precharged
                        || _bank_slice_manager.IsWrNttCmdPageHitExpired() // need to add a function to show whether the ntt wr page-hit cmd is already expired
                        ;

    // opposite rd ok
    //      1. OppHitCnt is not zero and read page hit num >= OppHitCnt
    // or   2. rd page hit num == ntt rd cmd num
    // or   3. rd page-hit cmd is being precharged
    // or   4. rd cmd is page-hit and expired
    bool opposite_rd_ok = (opp_hit_num >0 && _bank_slice_manager.GetRdNttPageHitNum() >= opp_hit_num)
                        || _bank_slice_manager.IsRdNttCmdAllPageHit()//
                        || false // need to add a function to show whether the ntt rd page-hit cmd will be precharged
                        || _bank_slice_manager.IsRdNttCmdPageHitExpired() // need to add a function to show whether the ntt rd page-hit cmd is already expired
                        ;

    //----------------------------------------------------------------------------------------------------------------------//
    auto& max_cmd_num_before_switch = _config.controller_config->Max_RW_BEFORE_SWITCH;

    bool is_wr_cam_expired = _scheduler.GetWrCam()->IsWrCamExpired();
    bool is_rd_cam_expired = _scheduler.GetRdCam()->IsRdCamExpired();
    // current wr ok
    //      1. no wr cmd page-hit
    // or   2. in write mode write sending cmd num is achieve MAX_BEFORE_SWITCH
    // or   3. no gpw expired cmd but exist gpr expired cmd
    bool current_wr_ok = _bank_slice_manager.GetWrNttPageHitNum() == 0
                        || wr_cmd_cnt_before_switch >= max_cmd_num_before_switch
                        || (is_rd_cam_expired && !is_wr_cam_expired)
                        ;

    // current rd ok
    //      1. no rd cmd page-hit
    // or   2. in read mode read sending cmd num is achieve MAX_BEFORE_SWITCH
    // or   3. no gpr expired cmd but exist gpw expired cmd
    bool current_rd_ok = _bank_slice_manager.GetRdNttPageHitNum() == 0
                        || rd_cmd_cnt_before_switch >= max_cmd_num_before_switch
                        || (!is_rd_cam_expired && is_wr_cam_expired)
                        ;

    // real switch to wr
    //      1. exisit switch2wr pending
    //      2. based on the Mode Switch Policy
    //          2.1. if Mode Switch Policy is true
    //              (1) current_rd_ok satisfied
    //              or (2) opposite_wr_ok satisfied
    //          2.2. if Mode Switch Policy is false
    //              (1) current_rd_ok satisfied
    //              and (2) (currently rd ntt has no page-hit cmd or opposite_wr_ok satisfied)

    real_switch2wr = switch2wr_pending && (_config.controller_config->MODE_SWITCH_POLICY
                    ? (current_rd_ok || opposite_wr_ok)
                    : (current_rd_ok && (_bank_slice_manager.GetRdNttPageHitNum() == 0 || opposite_wr_ok)));

    real_switch2rd = switch2rd_pending && (_config.controller_config->MODE_SWITCH_POLICY
                    ? (current_wr_ok || opposite_rd_ok)
                    : (current_wr_ok && (_bank_slice_manager.GetWrNttPageHitNum() == 0 || opposite_rd_ok)));

    real_switch2wr_access = switch2wr_pending && (_config.controller_config->MODE_SWITCH_POLICY ? true : (current_rd_ok || (!is_rd_cam_expired && is_wr_cam_expired)));
    real_switch2rd_access = switch2rd_pending && (_config.controller_config->MODE_SWITCH_POLICY ? true : (current_wr_ok || (is_rd_cam_expired && !is_wr_cam_expired)));


    if(real_switch2rd_access)
        select_wr_access = false;
    else if(is_wr_mode || real_switch2wr_access)
        select_wr_access = true;
    else if(select_wr_access)
        select_wr_access = false;
    else
        ;

    if(real_switch2wr_access)
        select_rd_access = false;
    else if(is_rd_mode || real_switch2rd_access)
        select_rd_access = true;
    else if(select_rd_access)
        select_rd_access = false;
    else
        ;

    assert(!(real_switch2rd_access && real_switch2wr_access));
    assert(!(real_switch2wr && real_switch2rd));

    assert((st==GlobalState::WR) || (st == GlobalState::RD));
    if(real_switch2wr && (st==GlobalState::RD))
    {
        st = GlobalState::WR;
    }

    else if(real_switch2rd && (st==GlobalState::WR))
    {
        st = GlobalState::RD;
    }

    switch(current_state)
    {
        case GlobalRdWrState::Rd:
        {
            if(real_switch2wr && select_wr_access)
            {
                current_state = GlobalRdWrState::Wr;
            }
            else if(!real_switch2wr && select_wr_access)
            {
                current_state = GlobalRdWrState::Rd2Wr;
            }
            else if(real_switch2rd || select_rd_access)
            {
                current_state = GlobalRdWrState::Rd;
            }
            else {
                DPRINT_FATAL("ModeSwitch State Update", "Invalid switch condition");
            }
            break;
        }
        case GlobalRdWrState::Rd2Wr:
        {
            if(real_switch2wr && select_wr_access)
            {
                current_state = GlobalRdWrState::Wr;
            }
            else if(!real_switch2wr && select_wr_access)
            {
                current_state = GlobalRdWrState::Rd2Wr;
            }
            else if(select_rd_access)
            {
                current_state = GlobalRdWrState::Rd;
            }
            else {
                DPRINT_FATAL("ModeSwitch State Update", "Invalid switch condition");
            }
            break;
        }
        case GlobalRdWrState::Wr:
        {
            if(real_switch2rd && select_rd_access)
            {
                current_state = GlobalRdWrState::Rd;
            }
            else if(!real_switch2rd && select_rd_access)
            {
                current_state = GlobalRdWrState::Wr2Rd;
            }
            else if(real_switch2wr || select_wr_access)
            {
                current_state = GlobalRdWrState::Wr;
            }
            else {
                DPRINT_FATAL("ModeSwitch State Update", "Invalid switch condition");
            }
            break;
        }
        case GlobalRdWrState::Wr2Rd:
        {
            if(real_switch2rd && select_rd_access)
            {
                current_state = GlobalRdWrState::Rd;
            }
            else if(!real_switch2rd && select_rd_access)
            {
                current_state = GlobalRdWrState::Wr2Rd;
            }
            else if(select_wr_access)
            {
                current_state = GlobalRdWrState::Wr;
            }
            else {
                DPRINT_FATAL("ModeSwitch State Update", "Invalid switch condition");
            }
            break;
        }
        default:
            DPRINT_FATAL("ModeSwitch State Update", "Invalid Current State");

    }

}
    }
}