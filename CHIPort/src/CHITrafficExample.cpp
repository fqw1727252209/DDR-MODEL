#include <iostream>

#include "ARM/TLM/arm_chi_phase.h"
#include "CHIPort/CHITrafficGenerator.h"
#include "CHIPort/CHIMonitor.hh"
#include "CHIPort/CHIPort.hh"
#include "Common/UifSlave.hh"

#include "Common/Common.hh"
#include "Configure/LoadConfigure.hh"
#include "Configure/Configure.hh"

using namespace dmu::Port;
void add_payloads_to_tg(CHITrafficGenerator& tg)
{
    tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00001000, ARM::CHI::SIZE_4);
    tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x0000600c, ARM::CHI::SIZE_64);
    tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00002008, ARM::CHI::SIZE_8);
    tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00005010, ARM::CHI::SIZE_64);
    tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00003020, ARM::CHI::SIZE_16);
    tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00004030, ARM::CHI::SIZE_64);
    tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00004000, ARM::CHI::SIZE_32);
    tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00003000, ARM::CHI::SIZE_64);
    tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00005000, ARM::CHI::SIZE_32);
    tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00002000, ARM::CHI::SIZE_64);
    tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00006000, ARM::CHI::SIZE_64);
    tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00001000, ARM::CHI::SIZE_64);
}


void heartbeat() {
    while (true) {
        sc_core::wait(100, sc_core::SC_NS);
        // heartbeat keeps events active
    }
}

int sc_main(int, char**)
{

    dmu::LoadConfigure load_configure("../ConfigureFile", "3ds_map2.json");
    load_configure.ParseJson();
    load_configure.ParseConfig();
    dmu::Configure configure(load_configure);

    static const unsigned data_width_bits = 256;
    dmu::Qos::ConfigureQosMap(configure.controller_config->RD_QOS_LEVEL_0,
        configure.controller_config->RD_QOS_LEVEL_1,
        configure.controller_config->WR_QOS_LEVEL);

//  DDR_TLM/ConfigureFile
    sc_core::sc_clock dfi_clk("clk", 4, sc_core::SC_NS, 0.5);
    sc_core::sc_clock noc_clk("noc_clk", 2, sc_core::SC_NS, 0.5);

    CHITrafficGenerator tg("tg", data_width_bits);
    CHIMonitor mon("mon", data_width_bits);
    dmu::Port::CHIPort port("port", configure,data_width_bits,dfi_clk.period());
    dmu::Port::UifSlave uif("uif", data_width_bits,dfi_clk.period());

    tg.clock.bind(noc_clk);
    port.dfi_clock.bind(dfi_clk);
    port.noc_clock.bind(noc_clk);
    uif.dfi_clk.bind(dfi_clk);

    tg.initiator.bind(mon.target);
    mon.initiator.bind(port.target);
    port.iSocket.bind(uif.target_socket);

    add_payloads_to_tg(tg);
    sc_core::sc_spawn(&heartbeat);


    sc_core::sc_start(30000, sc_core::SC_NS);

    ARM::CHI::Payload::debug_payload_pool(std::cout);

    return 0;
}