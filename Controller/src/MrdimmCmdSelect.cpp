#include "Controller/MrdimmCmdSelect.hh"
#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Common/CommonDefine.hh"
#include "Common/logger.hh"
#include <cstdlib>
#include <ios>
#include <optional>
#include <iomanip>

namespace dmu {
namespace Controller {

CommandTuple::Type MrdimmCmdSelect::SelectCommand(
    const std::vector<std::vector<ReadyCommands>>& bsc_ready_commands,
    const ReadyCommands& refresh_ready_commands,
    GlobalRdWrState global_rdwr_state)
{
    using BscCmdIdx = std::underlying_type_t<BscCmdType>;
    using ActCmdIdx = std::underlying_type_t<ActCmdType>;
    using PreCmdIdx = std::underlying_type_t<PreCmdType>;
    using RdWrCmdIdx = std::underlying_type_t<RdWrCmdType>;

    const auto ACT_IDX = static_cast<BscCmdIdx>(BscCmdType::Activate);
    const auto PRE_IDX = static_cast<BscCmdIdx>(BscCmdType::Precharge);
    const auto RDWR_IDX = static_cast<BscCmdIdx>(BscCmdType::RdWr);

    auto log_selected_cmd = [&](const std::string& priority_desc,
                                const CommandTuple::Type& cmd_tuple) -> CommandTuple::Type {
        DMU_LOG_INFO_N("Mc_" + std::to_string(m_pch_id),
            "[MrdimmCmdSelect] PCH " << m_pch_id << " " <<std::left<< std::setw(18)<<priority_desc
            << ": cmd=" << std::setw(8)<<std::get<CommandTuple::Command>(cmd_tuple).to_string()
            << " bank=" << std::get<CommandTuple::BaAddress>(cmd_tuple));
        return cmd_tuple;
    };

    // Priority 1: Force Precharge
    {
        const auto& force_pre_cmds = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::Force)];
        if (!force_pre_cmds.empty()) {
            return log_selected_cmd("ForcePrecharge",
                                    SelectByRoundRobin(force_pre_cmds, last_selected_pre_bsc));
        }
    }

    // Priority 2: Expired Rd/Wr (Column commands)
    {
        const auto& rd_expired_cmds = bsc_ready_commands[RDWR_IDX][static_cast<RdWrCmdIdx>(RdWrCmdType::RdExpired)];
        const auto& wr_expired_cmds = bsc_ready_commands[RDWR_IDX][static_cast<RdWrCmdIdx>(RdWrCmdType::WrExpired)];
        if ( (!rd_expired_cmds.empty() && global_rdwr_state == GlobalRdWrState::Rd)
          || (!wr_expired_cmds.empty() && global_rdwr_state == GlobalRdWrState::Wr) ) {
            return log_selected_cmd("ExpiredRdWr",
                                    SelectRdWrByGlobalState(rd_expired_cmds, wr_expired_cmds, global_rdwr_state));
        }
    }

    // Priority 3: Flush Rd/Wr
    {
        const auto& rd_flush_cmds = bsc_ready_commands[RDWR_IDX][static_cast<RdWrCmdIdx>(RdWrCmdType::RdFlush)];
        const auto& wr_flush_cmds = bsc_ready_commands[RDWR_IDX][static_cast<RdWrCmdIdx>(RdWrCmdType::WrFlush)];
        if ( (!rd_flush_cmds.empty() && global_rdwr_state == GlobalRdWrState::Rd)
          || (!wr_flush_cmds.empty() && global_rdwr_state == GlobalRdWrState::Wr) ) {
            return log_selected_cmd("FlushRdWr",
                                    SelectRdWrByGlobalState(rd_flush_cmds, wr_flush_cmds, global_rdwr_state));
        }
    }

    // Priority 4: Critical Rd/Wr
    {
        const auto& rd_critical_cmds = bsc_ready_commands[RDWR_IDX][static_cast<RdWrCmdIdx>(RdWrCmdType::RdCritical)];
        const auto& wr_critical_cmds = bsc_ready_commands[RDWR_IDX][static_cast<RdWrCmdIdx>(RdWrCmdType::WrCritical)];
        if ( (!rd_critical_cmds.empty() && global_rdwr_state == GlobalRdWrState::Rd)
          || (!wr_critical_cmds.empty() && global_rdwr_state == GlobalRdWrState::Wr) ) {
            return log_selected_cmd("CriticalRdWr",
                                    SelectRdWrByGlobalState(rd_critical_cmds, wr_critical_cmds, global_rdwr_state));
        }
    }

    // Priority 5: Normal Rd/Wr
    {
        const auto& rd_normal_cmds = bsc_ready_commands[RDWR_IDX][static_cast<RdWrCmdIdx>(RdWrCmdType::RdNormal)];
        const auto& wr_normal_cmds = bsc_ready_commands[RDWR_IDX][static_cast<RdWrCmdIdx>(RdWrCmdType::WrNormal)];
        if ( (!rd_normal_cmds.empty() && global_rdwr_state == GlobalRdWrState::Rd)
          || (!wr_normal_cmds.empty() && global_rdwr_state == GlobalRdWrState::Wr) ) {
            return log_selected_cmd("NormalRdWr",
                                    SelectRdWrByGlobalState(rd_normal_cmds, wr_normal_cmds, global_rdwr_state));
        }
    }

    // Priority 6: Refresh command
    if (!refresh_ready_commands.empty()) {
        return log_selected_cmd("Refresh",
                                SelectRefreshByRoundRobin(refresh_ready_commands));
    }

    // Priority 7: Device Precharge (for refresh waiting)
    {
        const auto& device_pre_cmds = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::Device)];
        if (!device_pre_cmds.empty()) {
            return log_selected_cmd("DevicePrecharge",
                                    SelectByRoundRobin(device_pre_cmds, last_selected_pre_bsc));
        }
    }

    // Priority 8: Expired Act/Pre (Row commands, combine Rd and Wr)
    {
        const auto& rd_expired_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::RdExpired)];
        const auto& wr_expired_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::WrExpired)];
        const auto& rd_expired_pre = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::RdExpired)];
        const auto& wr_expired_pre = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::WrExpired)];
        if (!rd_expired_act.empty() || !wr_expired_act.empty() ||
            !rd_expired_pre.empty() || !wr_expired_pre.empty()) {
            return log_selected_cmd("ExpiredActPre",
                                    SelectRowCommands(rd_expired_act, wr_expired_act,
                                                      rd_expired_pre, wr_expired_pre));
        }
    }

    // Priority 9: Flush Act/Pre
    {
        const auto& rd_flush_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::RdFlush)];
        const auto& wr_flush_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::WrFlush)];
        const auto& rd_flush_pre = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::RdFlush)];
        const auto& wr_flush_pre = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::WrFlush)];
        if (!rd_flush_act.empty() || !wr_flush_act.empty() ||
            !rd_flush_pre.empty() || !wr_flush_pre.empty()) {
            return log_selected_cmd("FlushActPre",
                                    SelectRowCommands(rd_flush_act, wr_flush_act,
                                                      rd_flush_pre, wr_flush_pre));
        }
    }

    // Priority 10: Critical Act/Pre
    {
        const auto& rd_critical_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::RdCritical)];
        const auto& wr_critical_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::WrCritical)];
        const auto& rd_critical_pre = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::RdCritical)];
        const auto& wr_critical_pre = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::WrCritical)];
        if (!rd_critical_act.empty() || !wr_critical_act.empty() ||
            !rd_critical_pre.empty() || !wr_critical_pre.empty()) {
            return log_selected_cmd("CriticalActPre",
                                    SelectRowCommands(rd_critical_act, wr_critical_act,
                                                      rd_critical_pre, wr_critical_pre));
        }
    }

    // Priority 11: Advance Act/Pre
    {
        const auto& rd_advance_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::RdAdvance)];
        const auto& wr_advance_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::WrAdvance)];
        if (!rd_advance_act.empty() || !wr_advance_act.empty()) {
            return log_selected_cmd("AdvanceAct",
                                    SelectRowCommands(rd_advance_act, wr_advance_act,
                                                      ReadyCommands{}, ReadyCommands{}));
        }
    }

    // Priority 12: Normal Act/Pre
    {
        const auto& rd_normal_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::RdNormal)];
        const auto& wr_normal_act = bsc_ready_commands[ACT_IDX][static_cast<ActCmdIdx>(ActCmdType::WrNormal)];
        const auto& rd_normal_pre = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::RdNormal)];
        const auto& wr_normal_pre = bsc_ready_commands[PRE_IDX][static_cast<PreCmdIdx>(PreCmdType::WrNormal)];
        if ( ((!rd_normal_act.empty() || !rd_normal_pre.empty()) ) ||
             ((!wr_normal_act.empty() || !wr_normal_pre.empty()) ) ) {
            return log_selected_cmd("NormalActPre",
                                    SelectRowCommandsByGlobalState(rd_normal_act, wr_normal_act,
                                                                   rd_normal_pre, wr_normal_pre, global_rdwr_state));
        }
    }

    // No command available
    DMU_LOG_FATAL_N("Mc_" + std::to_string(m_pch_id), "[MrdimmCmdSelect] PCH " << m_pch_id << " No command to select");
    std::abort();
}

CommandTuple::Type MrdimmCmdSelect::SelectRdWrByGlobalState(
    const ReadyCommands& rd_cmds,
    const ReadyCommands& wr_cmds,
    GlobalRdWrState state)
{
    // Based on global_rdwr_state, select read or write commands
    if (state == GlobalRdWrState::Rd) {
        // In Rd mode, select read commands with page hit priority
        if (!rd_cmds.empty()) {
            return SelectColCmdWithPageHitPriority(rd_cmds, last_selected_rdwr_bsc, true);
        }
    } else if (state == GlobalRdWrState::Wr) {
        // In Wr mode, select write commands with page hit priority
        if (!wr_cmds.empty()) {
            return SelectColCmdWithPageHitPriority(wr_cmds, last_selected_rdwr_bsc, false);
        }
    }

    // If no command in preferred direction, abort
    std::abort();
}

CommandTuple::Type MrdimmCmdSelect::SelectRowCommands(
    const ReadyCommands& rd_act_cmds,
    const ReadyCommands& wr_act_cmds,
    const ReadyCommands& rd_pre_cmds,
    const ReadyCommands& wr_pre_cmds)
{
    // Combine all row commands (both Act and Pre, both Rd and Wr direction)
    // First check if there are any commands
    bool has_rd_act = !rd_act_cmds.empty();
    bool has_wr_act = !wr_act_cmds.empty();
    bool has_rd_pre = !rd_pre_cmds.empty();
    bool has_wr_pre = !wr_pre_cmds.empty();

    if (!has_rd_act && !has_wr_act && !has_rd_pre && !has_wr_pre) {
        std::abort();
    }

    // Build a combined set of BSC indices for round-robin arbitration
    std::set<BSC_INDEX> row_bsc_set;
    std::map<BSC_INDEX, CommandTuple::Type> row_bsc2cmd_map;

    auto add_cmds_to_map = [&](const ReadyCommands& cmds) {
        for (const auto& cmd : cmds) {
            BSC_INDEX bsc_index = m_bankslice_manager.GetBa2BscTable()->at(
                std::get<CommandTuple::BaAddress>(cmd).real_ba);
            row_bsc_set.emplace(bsc_index);
            row_bsc2cmd_map.emplace(bsc_index, cmd);
        }
    };

    if (has_rd_act) add_cmds_to_map(rd_act_cmds);
    if (has_wr_act) add_cmds_to_map(wr_act_cmds);
    if (has_rd_pre) add_cmds_to_map(rd_pre_cmds);
    if (has_wr_pre) add_cmds_to_map(wr_pre_cmds);

    // Round-robin arbitration among all row commands
    auto arb_iter = row_bsc_set.upper_bound(last_selected_act_bsc);
    if (arb_iter != row_bsc_set.cend()) {
        last_selected_act_bsc = *arb_iter;
    } else {
        last_selected_act_bsc = *row_bsc_set.begin();
    }

    return row_bsc2cmd_map.at(last_selected_act_bsc);
}

CommandTuple::Type MrdimmCmdSelect::SelectRowCommandsByGlobalState(
    const ReadyCommands& rd_act_cmds,
    const ReadyCommands& wr_act_cmds,
    const ReadyCommands& rd_pre_cmds,
    const ReadyCommands& wr_pre_cmds,
    GlobalRdWrState state)
{
    // Combine all row commands (both Act and Pre, both Rd and Wr direction)
    // First check if there are any commands
    bool has_rd_act = !rd_act_cmds.empty();
    bool has_wr_act = !wr_act_cmds.empty();
    bool has_rd_pre = !rd_pre_cmds.empty();
    bool has_wr_pre = !wr_pre_cmds.empty();

    if (!has_rd_act && !has_wr_act && !has_rd_pre && !has_wr_pre) {
        std::abort();
    }

    // Build a combined set of BSC indices for round-robin arbitration
    std::set<BSC_INDEX> row_bsc_set;
    std::map<BSC_INDEX, CommandTuple::Type> row_bsc2cmd_map;

    auto add_cmds_to_map = [&](const ReadyCommands& cmds) {
        for (const auto& cmd : cmds) {
            BSC_INDEX bsc_index = m_bankslice_manager.GetBa2BscTable()->at(
                std::get<CommandTuple::BaAddress>(cmd).real_ba);
            row_bsc_set.emplace(bsc_index);
            row_bsc2cmd_map.emplace(bsc_index, cmd);
        }
    };

    if (has_rd_act) add_cmds_to_map(rd_act_cmds);
    if (has_wr_act) add_cmds_to_map(wr_act_cmds);

    if (state == GlobalRdWrState::Rd)
    {
        if (has_rd_pre) add_cmds_to_map(rd_pre_cmds);
    }
    else if (state == GlobalRdWrState::Wr)
    {
        if (has_wr_pre) add_cmds_to_map(wr_pre_cmds);
    }
    else{}

    // Round-robin arbitration among all row commands
    auto arb_iter = row_bsc_set.upper_bound(last_selected_act_bsc);
    if (arb_iter != row_bsc_set.cend()) {
        last_selected_act_bsc = *arb_iter;
    } else {
        last_selected_act_bsc = *row_bsc_set.begin();
    }

    return row_bsc2cmd_map.at(last_selected_act_bsc);
}

CommandTuple::Type MrdimmCmdSelect::SelectByRoundRobin(
    const ReadyCommands& cmds,
    BSC_INDEX& last_selected_bsc)
{
    if (cmds.empty()) {
        std::abort();
    }

    // Build set of BSC indices for round-robin
    std::set<BSC_INDEX> bsc_set;
    std::map<BSC_INDEX, CommandTuple::Type> bsc2cmd_map;

    for (const auto& cmd : cmds) {
        BSC_INDEX bsc_index = m_bankslice_manager.GetBa2BscTable()->at(
            std::get<CommandTuple::BaAddress>(cmd).real_ba);
        bsc_set.emplace(bsc_index);
        bsc2cmd_map.emplace(bsc_index, cmd);
    }

    // Round-robin arbitration
    auto arb_iter = bsc_set.upper_bound(last_selected_bsc);
    if (arb_iter != bsc_set.cend()) {
        last_selected_bsc = *arb_iter;
    } else {
        last_selected_bsc = *bsc_set.begin();
    }

    return bsc2cmd_map.at(last_selected_bsc);
}

CommandTuple::Type MrdimmCmdSelect::SelectRefreshByRoundRobin(
    const ReadyCommands& refresh_cmds)
{
    if (refresh_cmds.empty()) {
        std::abort();
    }

    // Build set of rank indices for round-robin
    std::set<unsigned> rank_set;
    std::map<unsigned, CommandTuple::Type> rank2cmd_map;

    for (const auto& cmd : refresh_cmds) {
        unsigned rank_index = std::get<CommandTuple::BaAddress>(cmd).real_cid;
        rank_set.emplace(rank_index);
        rank2cmd_map.emplace(rank_index, cmd);
    }

    // Round-robin arbitration among ranks
    auto arb_iter = rank_set.upper_bound(last_selected_refresh_rank);
    if (arb_iter != rank_set.cend()) {
        last_selected_refresh_rank = *arb_iter;
    } else {
        last_selected_refresh_rank = *rank_set.begin();
    }

    return rank2cmd_map.at(last_selected_refresh_rank);
}

CommandTuple::Type MrdimmCmdSelect::SelectColCmdWithPageHitPriority(
    const ReadyCommands& cmds,
    BSC_INDEX& last_selected_bsc,
    bool is_rd)
{
    if (cmds.empty()) {
        std::abort();
    }

    // Build set of BSC indices
    std::set<BSC_INDEX> col_bsc_set;
    std::map<BSC_INDEX, CommandTuple::Type> col_bsc2cmd_map;

    for (const auto& cmd : cmds) {
        BSC_INDEX bsc_index = m_bankslice_manager.GetBa2BscTable()->at(
            std::get<CommandTuple::BaAddress>(cmd).real_ba);
        col_bsc_set.emplace(bsc_index);
        col_bsc2cmd_map.emplace(bsc_index, cmd);
    }

    // First try to select oldest page-hit command
    BSC_INDEX oldest_page_hit_bsc;
    if (is_rd) {
        oldest_page_hit_bsc = m_bankslice_manager.GetRdOldestPageHitBsc();
    } else {
        oldest_page_hit_bsc = m_bankslice_manager.GetWrOldestPageHitBsc();
    }

    if (col_bsc_set.count(oldest_page_hit_bsc) > 0) {
        return col_bsc2cmd_map.at(oldest_page_hit_bsc);
    }

    // If no page-hit command, use round-robin
    auto arb_iter = col_bsc_set.upper_bound(last_selected_bsc);
    if (arb_iter != col_bsc_set.cend()) {
        last_selected_bsc = *arb_iter;
    } else {
        last_selected_bsc = *col_bsc_set.begin();
    }

    return col_bsc2cmd_map.at(last_selected_bsc);
}

} // namespace Controller
} // namespace dmu