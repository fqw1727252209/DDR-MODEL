#ifndef __CMD_SELECT_HH__
#define __CMD_SELECT_HH__

#include "Controller/CamEntry.hh"
#include "Controller/common/Command.hh"
#include "Configure/Configure.hh"
#include "Controller/BankSliceManager.hh"

namespace dmu{
    namespace Controller{

// class CmdSelectIF{
// protected:
//     CmdSelectIF(const CmdSelectIF&) = default;
//     CmdSelectIF(CmdSelectIF&&) = default;

//     CmdSelectIF& operator=(const CmdSelectIF&) = default;
//     CmdSelectIF& operator=(CmdSelectIF&&) = default;

// public:
//     CmdSelectIF() = default;
//     virtual ~CmdSelectIF() = default;
//     virtual CommandTuple::Type SelectCommand(const ReadyCommands& ready_commands) = 0;
// };

class CmdSelect
{
    using Rank_INDEX = unsigned;
    public:
        explicit CmdSelect(const Configure& config, BankSliceManager& bank_slice_manager)
        : _config(config)
        , _bank_slice_manager(bank_slice_manager)
        {};
        CommandTuple::Type SelectCommand(const ReadyCommands& ready_commands, GlobalRdWrState global_rdwr_state);
    private:
        BankSliceManager& _bank_slice_manager;
        const Configure& _config;

        // ReadyCommands readyRasCommands;
        // ReadyCommands readyCasCommands;
        // ReadyCommands readyRefCommands;

        BSC_INDEX last_selected_row_bsc{0};
        BSC_INDEX last_selected_col_bsc{0};

        Rank_INDEX last_selected_rank_index{100};

        const sc_core::sc_time MaxTime = sc_core::sc_max_time();

};

    }
}

#endif