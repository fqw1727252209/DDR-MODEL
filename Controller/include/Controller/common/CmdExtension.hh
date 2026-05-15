#ifndef __CMD_EXTENSION_HH__
#define __CMD_EXTENSION_HH__

#include <deque>

#include <systemc>
#include <tlm>

#include "Controller/common/Command.hh"

namespace dmu{
    namespace Controller{

class CmdExtension: public tlm::tlm_extension<CmdExtension>
{
    public:
        tlm_extension_base * clone() const override
        {
            auto* ext= new CmdExtension();
            ext->trans_cmd_queue = trans_cmd_queue;
            ext->cmd_address = cmd_address;
            return ext;
        }
        void copy_from(const tlm_extension_base &ext) override
        {
            const auto& cpyFrom = dynamic_cast<const CmdExtension&>(ext);
            trans_cmd_queue = cpyFrom.trans_cmd_queue;
            cmd_address = cpyFrom.cmd_address;
        }
        inline const Command& GetCommand() const
        {
            if(trans_cmd_queue.empty())
            {
                SC_REPORT_FATAL("Payload", "Has no Command");
            }
            return trans_cmd_queue.front();
        }
        inline void AddCommand(Command::Type sending_cmd) { trans_cmd_queue.emplace_back(sending_cmd); }

        inline void PopCommand() { trans_cmd_queue.pop_front(); }

        inline void SetAddress(BankAddress address) { cmd_address = address; }
        inline BankAddress GetAddress() const { return cmd_address; }

        CmdExtension() = default;
    private:
        std::deque<Command> trans_cmd_queue;
        BankAddress cmd_address;

};

    }
}

#endif