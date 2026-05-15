#ifndef __MRDIMM_CONTROLLER_HH__
#define __MRDIMM_CONTROLLER_HH__

#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Common/logger.hh"
#include "Common/CommonDefine.hh"
#include "Configure/Configure.hh"
#include "Controller/SdramConstraint.hh"
#include "Controller/InputProcess.hh"
#include "Common/UifExtension.hh"
#include "Controller/Scheduler.hh"
#include "Controller/BankSliceManager.hh"
#include "Controller/ModeSwitch.hh"
#include "Controller/CmdSelect.hh"
#include "Controller/RefreshMachineManager.hh"
#include "Controller/MrdimmModeSwitch.hh"
#include "Controller/MrdimmCmdSelect.hh"
#include "Controller/common/DfiExtension.hh"
#include "Controller/common/CmdExtension.hh"
#include "Common/StatisticExtension.hh"
#include "Common/Common.hh"

#include <memory>
#include <string>
#include <systemc>
#include <tlm>
#include "tlm_utils/simple_initiator_socket.h"
#include "tlm_utils/simple_target_socket.h"
#include "tlm_utils/peq_with_cb_and_phase.h"

namespace dmu{
    namespace Controller{

        class MrdimmController : public sc_core::sc_module{
        public:
            // TLM sockets
            std::vector<std::unique_ptr<tlm_utils::simple_target_socket<MrdimmController>>> tSocket;
            tlm_utils::simple_initiator_socket<MrdimmController> iSocket;

            // Constructor
            explicit MrdimmController(const sc_core::sc_module_name& name,
                                      const Configure& config,
                                      SdramConstraintIF* sdram_constraint,const std::string& output_dir);

            ~MrdimmController();

            // TLM interface methods - per pseudo channel nb_transport_fw
            tlm::tlm_sync_enum nb_transport_fw_0(tlm::tlm_generic_payload& trans,
                                                 tlm::tlm_phase& phase,
                                                 sc_core::sc_time& delay);

            tlm::tlm_sync_enum nb_transport_fw_1(tlm::tlm_generic_payload& trans,
                                                 tlm::tlm_phase& phase,
                                                 sc_core::sc_time& delay);

            tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                               tlm::tlm_phase& phase,
                                               sc_core::sc_time& delay);

            // SystemC process methods
            void ControllerMethod();
            void CreditSend();        // 独立SC_METHOD, 内部循环处理两个伪通道
            void end_of_simulation() override;
            void bind_dfi_clock(const sc_core::sc_clock& clk);

        private:
            // Per-pseudo-channel pipeline method callbacks
            void pipline_method_0(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);
            void pipline_method_1(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);

            // Core control methods
            void AcTimingUpdate();  // 全局统一
            void CmdSend();         // 全局协调
            void ReqUpdate(unsigned pseudo_channel_id); // 每个伪通道
            void CqStore(unsigned pseudo_channel_id);   // 每个伪通道
            void ControllerFinishCheck();

            // Internal helper method for unified nb_transport_fw processing
            tlm::tlm_sync_enum nb_transport_fw_internal(tlm::tlm_generic_payload& trans,
                                                        tlm::tlm_phase& phase,
                                                        sc_core::sc_time& delay,
                                                        unsigned pseudo_channel_id);

            // Internal helper for pipline_method
            void pipline_method_internal(tlm::tlm_generic_payload& trans,
                                         const tlm::tlm_phase& phase,
                                         unsigned pseudo_channel_id);

            // Configuration and constraints
            const Configure& m_config;
            SdramConstraintDDR5_3ds* m_sdram_constraint{nullptr};

            // Per-pseudo-channel components
            std::vector<std::unique_ptr<InputProcess>> m_input_process;
            std::vector<std::unique_ptr<Scheduler>> m_scheduler;
            std::vector<std::unique_ptr<BankSliceManager>> m_bankslice_manager;
            std::vector<std::unique_ptr<MrdimmCmdSelect>> m_mrdimm_cmd_select;
            std::vector<std::unique_ptr<RefreshMachineManager>> m_refresh_manager;

            // Global mode switch
            std::unique_ptr<MrdimmModeSwitch> m_mrdimm_mode_switch;

            // Per-pseudo-channel payload event queues
            std::vector<std::unique_ptr<tlm_utils::peq_with_cb_and_phase<MrdimmController>>> m_payload_event_queues;

            // Clock and timing
            sc_core::sc_in<bool> dfi_clock;
            sc_core::sc_time next_trigger_delay;
            sc_core::sc_event ctrl_event;
            const sc_core::sc_time dfi_cycle_time;
            const sc_core::sc_time ddr_cycle_time;

            // DFI interface (double buffered to avoid race condition with PhyDelayModel)
            tlm::tlm_generic_payload* dfi_payload[2];
            DfiCmdExtension* dfi_extension[2];
            unsigned dfi_buffer_sel = 0;

            // Per-pseudo-channel state
            std::vector<bool> addr_collision_busy;
            std::vector<ReadyCommands> refresh_ready_commands;

        };
    }
}

#endif