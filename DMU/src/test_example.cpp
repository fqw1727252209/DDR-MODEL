#include "DMU/DramManagerUnit.hh"
// #include "../../include/DMU/DramManagerUnit.hh"

#include "CHIPort/CHITrafficGenerator.h"
#include "CHIPort/CHIMonitor.hh"
#include "sysc/kernel/sc_externs.h"
#include <systemc>


int sc_main(int argc, char **argv)
{
    unsigned chi_data_width_bits = 256;
    sc_core::sc_clock noc_clk("noc_clk", 2, sc_core::SC_NS, 0.5);
    dmu::DramManagerUnit dmu("dram_manager_unit", noc_clk, chi_data_width_bits, "../../ConfigureFile", "3ds_map2.json");

    dmu::Port::TrafficConfig cfg;
    cfg.read_ratio = 0.5;
    cfg.addr_start = 0x1000;
    cfg.addr_end = 0x100000;
    cfg.addr_step = 0x40;
    cfg.sequential = true;
    cfg.max_generated_transactions = 2000;
    cfg.max_outstanding = 32;

    dmu::Port::CHITrafficGenerator tg("tg", cfg, chi_data_width_bits);
    dmu::Port::CHIMonitor monitor("monitor", chi_data_width_bits);
    tg.clock(noc_clk);
    tg.initiator[0]->bind(monitor.target);
    monitor.initiator.bind(dmu.chi_port_0->target);

    sc_core::sc_start(20, sc_core::SC_US);

    return 0;
}