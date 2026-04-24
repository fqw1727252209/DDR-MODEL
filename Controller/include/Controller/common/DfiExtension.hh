#ifndef __DFI_EXTENSION_HH__
#define __DFI_EXTENSION_HH__

#include "Controller/common/ControllerCommon.hh"
#include "Common/UifExtension.hh"
#include "sysc/utils/sc_report.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <cstdint>
#include <tlm>

#include "Controller/common/Command.hh"

namespace dmu{
    namespace Controller{

// class dfi_phase
// {
//     public:
//         enum phase: uint8_t
//         {
//             dfi_p0 = 0x0,
//             dfi_p1 = 0x1,
//             dfi_p2 = 0x2,
//             dfi_p3 = 0x3
//         };

//         // dfi_phase operator()();
//         // dfi_phase uint8_t;
//     private:
//         phase px{dfi_p0};

// };


class DfiExtension: public tlm::tlm_extension<DfiExtension>
{
    public:
        tlm_extension_base * clone() const override
        {
            auto* ext= new DfiExtension();
            ext->dfi_cmd_queue = dfi_cmd_queue;
            ext->cmd_address = cmd_address;
            return ext;
        }

        void copy_from(const tlm_extension_base &ext) override
        {
            const auto& cpyFrom = dynamic_cast<const DfiExtension&>(ext);
            dfi_cmd_queue = cpyFrom.dfi_cmd_queue;
            cmd_address = cpyFrom.cmd_address;
        }

        inline const Command& GetCommand() const
        {
            if(dfi_cmd_queue.empty())
            {
                SC_REPORT_FATAL("Payload", "Has no Dfi Command");
            }
            return dfi_cmd_queue.front();
        }

        inline void AddCommand(Command::Type sending_cmd) { dfi_cmd_queue.emplace_back(sending_cmd); }

        inline void PopCommand() { dfi_cmd_queue.pop_front(); }

        inline void SetAddress(BankAddress address) { cmd_address = address; }
        inline BankAddress GetAddress() const { return cmd_address; }

        DfiExtension() = default;
    private:
        std::deque<Command> dfi_cmd_queue;
        BankAddress cmd_address;

};

    }
}
#endif