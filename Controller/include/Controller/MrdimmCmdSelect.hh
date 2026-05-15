#ifndef __MRDIMM_CMD_SELECT_HH__
#define __MRDIMM_CMD_SELECT_HH__

#include "Configure/Configure.hh"
#include "Controller/BankSliceManager.hh"
#include "Controller/common/Command.hh"
#include <optional>

namespace dmu{
    namespace Controller{
/*
命令优先级：
1. force precharge (PreCmdType::Force)
2. expired rd/wr (RdWrCmdType::RdExpired/WrExpired)
3. flush rd/wr (RdWrCmdType::RdFlush/WrFlush)
4. critical rd/wr (RdWrCmdType::RdCritical/WrCritical)
5. normal rd/wr (RdWrCmdType::RdNormal/WrNormal)
6. refresh command
7. device precharge (PreCmdType::Device)
8. expired act/pre (ActCmdType::RdExpired/WrExpired + PreCmdType::RdExpired/WrExpired)
9. flush act/pre (ActCmdType::RdFlush/WrFlush + PreCmdType::RdFlush/WrFlush)
10. critical act/pre (ActCmdType::RdCritical/WrCritical + PreCmdType::RdCritical/WrCritical)
11. advance act/pre (ActCmdType::RdAdvance/WrAdvance)
12. normal act/pre (ActCmdType::RdNormal/WrNormal + PreCmdType::RdNormal/WrNormal)

*/
    class MrdimmCmdSelect{

        public:
            explicit MrdimmCmdSelect(const Configure& config, BankSliceManager& bankslice_manager,unsigned pch_id)
            :m_config(config)
            ,m_bankslice_manager(bankslice_manager)
            ,m_pch_id(pch_id)
            {};

            CommandTuple::Type SelectCommand(const std::vector<std::vector<ReadyCommands>>& bsc_ready_commands, const ReadyCommands& refresh_ready_commands, GlobalRdWrState global_rdwr_state);
        private:
            const Configure& m_config;
            const unsigned m_pch_id;
            BankSliceManager& m_bankslice_manager;

            // Round-robin state for arbitration
            BSC_INDEX last_selected_pre_bsc{0};      // For Precharge commands (Force, Device)
            BSC_INDEX last_selected_rdwr_bsc{0};     // For RdWr column commands
            BSC_INDEX last_selected_act_bsc{0};      // For Activate commands (row commands)
            unsigned last_selected_refresh_rank{0};  // For Refresh commands

            // Helper functions for command selection
            CommandTuple::Type SelectRdWrByGlobalState(
                const ReadyCommands& rd_cmds,
                const ReadyCommands& wr_cmds,
                GlobalRdWrState state);
            
            CommandTuple::Type SelectRowCommands(
                const ReadyCommands& rd_act_cmds,
                const ReadyCommands& wr_act_cmds,
                const ReadyCommands& rd_pre_cmds = {},
                const ReadyCommands& wr_pre_cmds = {}
            );

            CommandTuple::Type SelectRowCommandsByGlobalState(
                const ReadyCommands& rd_act_cmds,
                const ReadyCommands& wr_act_cmds,
                const ReadyCommands& rd_pre_cmds,
                const ReadyCommands& wr_pre_cmds,
                GlobalRdWrState state);

            CommandTuple::Type SelectByRoundRobin(
                const ReadyCommands& cmds,
                BSC_INDEX& last_selected_bsc);

            CommandTuple::Type SelectRefreshByRoundRobin(
                const ReadyCommands& refresh_cmds);

            CommandTuple::Type SelectColCmdWithPageHitPriority(
                const ReadyCommands& cmds,
                BSC_INDEX& last_selected_bsc,
                bool is_rd);
            
    };
    }
}

#endif