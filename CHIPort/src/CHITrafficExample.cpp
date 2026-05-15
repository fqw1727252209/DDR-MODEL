#include <iostream>

#include "ARM/TLM/arm_chi_phase.h"
#include "CHIPort/CHIMonitor.hh"
#include "CHIPort/CHIPort.hh"
#include "CHIPort/CHITrafficGenerator.h"
#include "Common/UifSlave.hh"

#include "Common/Common.hh"
#include "Configure/Configure.hh"
#include "Configure/LoadConfigure.hh"

using namespace dmu::Port;

int sc_main(int, char **) {

  dmu::LoadConfigure load_configure("../../ConfigureFile",
                                    "mrdimm_map1_no_refresh.json");
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

  TrafficConfig cfg;
  cfg.read_ratio = 0.5;
  cfg.addr_start = 0x1000;
  cfg.addr_end = 0x7000;
  cfg.addr_step = 0x40;
  cfg.sequential = true;
  cfg.max_generated_transactions = 100;
  cfg.max_outstanding = 16;

  CHITrafficGenerator tg("tg", cfg, data_width_bits);
  CHIMonitor mon("mon", data_width_bits);
  dmu::Port::CHIPort port("port", configure, data_width_bits, dfi_clk.period());
  dmu::Port::UifSlave uif("uif", data_width_bits, dfi_clk.period());

  tg.clock.bind(noc_clk);
  port.dfi_clock.bind(dfi_clk);
  port.noc_clock.bind(noc_clk);
  uif.dfi_clk.bind(dfi_clk);

  tg.initiator[0]->bind(mon.target);
  mon.initiator.bind(port.target);
  port.iSocket.bind(uif.target_socket);

  sc_core::sc_start(2000, sc_core::SC_NS);

  ARM::CHI::Payload::debug_payload_pool(std::cout);

  return 0;
}