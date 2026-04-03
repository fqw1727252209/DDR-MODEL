#include "DMU/DramManagerUnit.hh"
// #include "../../include/DMU/DramManagerUnit.hh"

#include "CHIPort/CHITrafficGenerator.h"
#include "CHIPort/CHIMonitor.hh"
#include "sysc/kernel/sc_externs.h"
#include <systemc>

void add_payloads_to_tg(dmu::Port::CHITrafficGenerator& tg)
{
    // tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00001000, ARM::CHI::SIZE_4);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x0000600c, ARM::CHI::SIZE_64);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00002008, ARM::CHI::SIZE_8);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00005010, ARM::CHI::SIZE_64);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00003020, ARM::CHI::SIZE_16);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00004030, ARM::CHI::SIZE_64);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00004000, ARM::CHI::SIZE_32);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00003000, ARM::CHI::SIZE_64);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00005000, ARM::CHI::SIZE_32);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00002000, ARM::CHI::SIZE_64);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00006000, ARM::CHI::SIZE_64);
    // tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x00001000, ARM::CHI::SIZE_64);

    for(unsigned i = 0; i < 1000; i++)
    {
        tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00001000 + i*0x40, ARM::CHI::SIZE_64);
        tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x10001000 + i*0x40, ARM::CHI::SIZE_64);
    }
}


int sc_main(int argc, char **argv)
{

    // ("../../ConfigureFile", "3ds_map2.json");


    sc_core::sc_clock noc_clk("noc_clk", 2, sc_core::SC_NS, 0.5);
    unsigned chi_data_width_bits = 256;
    dmu::DramManagerUnit dmu("dram_manager_unit",noc_clk,chi_data_width_bits,"../../ConfigureFile","3ds_map2.json");
    dmu::Port::CHITrafficGenerator tg("tg", chi_data_width_bits);
    dmu::Port::CHIMonitor monitor("monitor", chi_data_width_bits);
    tg.clock(noc_clk);
    tg.initiator.bind(monitor.target);
    monitor.initiator.bind(dmu.chi_port_0->target);

    add_payloads_to_tg(tg);

    sc_core::sc_start(20, sc_core::SC_US);

    return 0;
}