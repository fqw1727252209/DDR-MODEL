#ifndef __MEMORY_CONTROLLER_HH__
#define __MEMORY_CONTROLLER_HH__

#include <cstddef>
#include <memory>
#include <string>

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <unordered_map>

#include "Controller/common/Command.hh"
#include "Controller/common/ControllerCommon.hh"
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

#include "sysc/communication/sc_clock.h"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_time.h"
#include "tlm_core/tlm_2/tlm_2_interfaces/tlm_fw_bw_ifs.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_phase.h"
#include "tlm_utils/peq_with_cb_and_phase.h"

namespace dmu{
    namespace Controller{

        // TLM tlm::util nb_transport_fw // 上游调用，下游实现
        // TLM tlm::util nb_transport_bw // 反馈数据，反馈写的响应，简单回调
        // 

class MemoryController: public sc_core::sc_module{

public:
    tlm_utils::simple_target_socket<MemoryController> tSocket;
    tlm_utils::simple_initiator_socket<MemoryController> iSocket;
    MemoryController(const sc_core::sc_module_name& name, const Configure& config, SdramConstraintIF* sdram_constraint)
    : sc_module(name)
    , tSocket("tSocket")
    , iSocket("iSocket")
    , _config(config)
    , _sdram_constraint(dynamic_cast<SdramConstraintDDR5_3ds*>(sdram_constraint))
    , payload_event_queue(this, &MemoryController::pipline_method)
    , dfi_cycle_time(config.mem_spec->tCK_mc)
    , ddr_cycle_time(config.mem_spec->tCK)
    , phy_cmd_delay(config.controller_config->PHY_CMD_DELAY * config.mem_spec->tCK)
    , phy_wdat_delay(config.controller_config->PHY_WDAT_DELAY * config.mem_spec->tCK)
    {
        tSocket.register_nb_transport_fw(this, &MemoryController::nb_transport_fw);

        iSocket.register_nb_transport_bw(this, &MemoryController::nb_transport_bw);

        _scheduler = std::make_unique<Scheduler>(config);
        _input_process = std::make_unique<InputProcess>(config, *_scheduler);
        _bankslice_manager = std::make_unique<BankSliceManager>(*_scheduler, config);
        _mode_switch = std::make_unique<ModeSwitch>(*_scheduler, *_bankslice_manager, config);
        _cmd_select = std::make_unique<CmdSelect>(config, *_bankslice_manager);
        _refresh_machine_manager = std::make_unique<RefreshMachineManager>(*_bankslice_manager, config);

        _scheduler->RegisterBa2BscTable(_bankslice_manager->GetBa2BscTable());
        _scheduler->RegisterBscSliceMap(_bankslice_manager->GetBankSliceMap());
        SC_HAS_PROCESS(MemoryController);


        SC_METHOD(ControllerMethod);
        sensitive<< ctrl_event;
        dont_initialize();

        SC_METHOD(CreditSend);
        sensitive<< dfi_clock.neg();
        dont_initialize();
    }
    ~MemoryController();

    tlm::tlm_sync_enum nb_transport_fw(tlm::tlm_generic_payload& trans,
                                       tlm::tlm_phase& phase,
                                       sc_core::sc_time& delay);

    tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload& trans,
                                       tlm::tlm_phase& phase,
                                       sc_core::sc_time& delay);

public:

    void end_of_simulation() override;
    void bind_dfi_clock(const sc_core::sc_clock& clk)
    {
        dfi_clock.bind(clk);
    }

private:
    const Configure& _config;
    std::unique_ptr<InputProcess> _input_process;
    std::unique_ptr<Scheduler> _scheduler;
    std::unique_ptr<BankSliceManager> _bankslice_manager;
    std::unique_ptr<ModeSwitch> _mode_switch;
    std::unique_ptr<CmdSelect> _cmd_select;
    std::unique_ptr<RefreshMachineManager> _refresh_machine_manager;
    SdramConstraintDDR5_3ds* _sdram_constraint{nullptr};

    tlm_utils::peq_with_cb_and_phase<MemoryController> payload_event_queue;
    void pipline_method(tlm::tlm_generic_payload& trans, const tlm::tlm_phase& phase);
private:
    void CreditSend()
    {
        UifSideBandInfo uif_side_band_info;
        uif_side_band_info.hpr_credit_valid = _scheduler->HasHprCredit();
        if(uif_side_band_info.hpr_credit_valid)
        {
            _scheduler->GetRdCam()->DecreaseHprCredit();
        }
        uif_side_band_info.lpr_credit_valid = _scheduler->HasLprCredit();
        if(uif_side_band_info.lpr_credit_valid)
        {
            _scheduler->GetRdCam()->DecreaseLprCredit();
        }
        uif_side_band_info.tpw_credit_valid = _scheduler->HasTpwCredit();
        if(uif_side_band_info.tpw_credit_valid)
        {
            _scheduler->GetWrCam()->DecreaseTpwCredit();
        }
        if(uif_side_band_info.hpr_credit_valid || uif_side_band_info.lpr_credit_valid || uif_side_band_info.tpw_credit_valid)
        {
            UifSideBandExtension* uif_side_band_extension = new UifSideBandExtension(uif_side_band_info);
            auto credit_trans = _input_process->GetMemoryManager().getDummyPayload();
            credit_trans->set_extension<UifSideBandExtension>(uif_side_band_extension);
            tlm::tlm_phase phase = UIF_CREDIT;
            sc_core::sc_time delay{sc_core::SC_ZERO_TIME};
            tSocket->nb_transport_bw(*credit_trans, phase, delay);
        }
    }


    sc_core::sc_in<bool> dfi_clock;

    //SC_METHOD, the main simulate process driven by event
    void ControllerMethod();

    void AcTimingUpdate();

    void CmdSend();
    void ReqUpdate();
    // do addr collsion detect, and back-pressure, and set pip busy
    void CqStore();
    // if the ctrl_event is triggered, then do the nb_transport_fw, else store the request, and do AcceptRequest(trans) in pip process stage
    void PipProcess();
    bool addr_collision_busy{false}; // to show wether the controller is collision
    void ReqCmdProcess();
    void WdataProcess();
    void RdataProcess();

    void ControllerFinishCheck();
    ReadyCommands ready_commands;

    sc_core::sc_event ctrl_event;

    sc_core::sc_time next_trigger_delay;

    const sc_core::sc_time dfi_cycle_time; // dfi clk time
    const sc_core::sc_time ddr_cycle_time;

    const sc_core::sc_time phy_cmd_delay;
    const sc_core::sc_time phy_wdat_delay;

    // sc_core::sc_time data_trans_time{sc_core::SC_ZERO_TIME}; // record the data busy time
    // sc_core::sc_time last_column_trans_time{sc_core::SC_ZERO_TIME};
    // bool DfiDataChannelBusy{false};

    unsigned trans_send{0};

    std::unordered_map<unsigned, tlm::tlm_generic_payload*> resp_queue;

    inline void AddTrans2ResonseQueue(unsigned trans_id, tlm::tlm_generic_payload* trans)
    {
        resp_queue.insert(std::make_pair(trans_id,trans));
        trans->acquire();
    }
    void RemoveTransFromResonseQueue(unsigned trans_id, tlm::tlm_generic_payload* trans)
    {
        // trans->release();
        resp_queue.erase(trans_id);
    }
};

}
}
#endif