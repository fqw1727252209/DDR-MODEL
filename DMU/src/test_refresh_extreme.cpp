#include "DMU/DramManagerUnit.hh"
#include "CHIPort/CHITrafficGenerator.h"
#include "CHIPort/CHIMonitor.hh"
#include "sysc/kernel/sc_externs.h"
#include <systemc>
#include <cstdlib>
#include <string>

void add_payloads_to_tg(dmu::Port::CHITrafficGenerator& tg) {
    for(unsigned i = 0; i < 1000; i++) {
        tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00001000 + i*0x40, ARM::CHI::SIZE_64);
        tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x10001000 + i*0x40, ARM::CHI::SIZE_64);
    }
}

void add_rfm_hammer_payloads(dmu::Port::CHITrafficGenerator& tg) {
    // 地址映射 (3ds_map2): Byte=[0:1], Col=[2:5,9:14], BG=[6:8], Bank=[15:16], CID=[17], CS=[18], Row=[19:34]
    // 集中火力极高频攻击同一个 Bank (BG 0, Bank 0) 的不同 Row
    // 利用连续访问不同行强制硬件连续执行 PRE 和 ACT，逼迫 act_counter 快速飙升
    for(unsigned i = 0; i < 5000; i++) {
        uint64_t addr = 0x00001000
                      | ((uint64_t)0 << 6)   // bg = 0
                      | ((uint64_t)0 << 15)  // bank = 0
                      | ((uint64_t)i << 19); // 每次访问新行 i
        tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, addr, ARM::CHI::SIZE_64);
    }
}

void add_burst_block_payloads(dmu::Port::CHITrafficGenerator& tg) {
    // 短时间内集中爆发高频写负荷，容易耗尽总线和Bank资源造成Refresh推迟进入Critical，并测试Burst连续5次发出的情况
    for(unsigned i = 0; i < 5000; i++) tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x20000000 + i*0x40, ARM::CHI::SIZE_64);
}

void add_refsb_parallel_payloads(dmu::Port::CHITrafficGenerator& tg) {
    // 双行交替读写，测试关闭REFab时，单Bank阻塞下的极高并发吞吐特性
    for(unsigned i = 0; i < 1000; i++) {
        tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x10000000 + i*0x40, ARM::CHI::SIZE_64);
        tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x20000000 + i*0x40, ARM::CHI::SIZE_64);
    }
}

void add_integration_payloads(dmu::Port::CHITrafficGenerator& tg) {
    // 综合集成测试：混合读写负载，覆盖多个 Bank/BG 的同时施加中等强度流量压力，
    // 用于验证所有刷新特性（tREFI 周期、Stagger、Hysteresis、PRE→REF、tRFC）
    // 在有业务流量干扰条件下能否协同正常工作
    for(unsigned i = 0; i < 2000; i++) {
        unsigned bg = i % 8;
        unsigned bank = (i / 8) % 4;
        uint64_t base = ((uint64_t)bg << 6) | ((uint64_t)bank << 15) | ((uint64_t)(i / 32) << 19);
        tg.add_payload(ARM::CHI::REQ_OPCODE_READ_NO_SNP, 0x00001000 + base, ARM::CHI::SIZE_64);
        if (i % 3 == 0)  // 每 3 笔读插 1 笔写，模拟真实混合负载
            tg.add_payload(ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL, 0x10001000 + base, ARM::CHI::SIZE_64);
    }
}

int sc_main(int argc, char **argv)
{
    sc_core::sc_clock noc_clk("noc_clk", 2, sc_core::SC_NS, 0.5);
    unsigned chi_data_width_bits = 256;
    dmu::DramManagerUnit dmu("dram_manager_unit", noc_clk, chi_data_width_bits, "../../ConfigureFile", "3ds_map2.json");
    dmu::Port::CHITrafficGenerator tg("tg", chi_data_width_bits);
    dmu::Port::CHIMonitor monitor("monitor", chi_data_width_bits);
    tg.clock(noc_clk);
    tg.initiator.bind(monitor.target);
    monitor.initiator.bind(dmu.chi_port_0->target);

    if (const char* env_p = std::getenv("TEST_MODE")) {
        std::string mode(env_p);
        if (mode == "RFM") add_rfm_hammer_payloads(tg);
        else if (mode == "BURST" || mode == "CRITICAL") add_burst_block_payloads(tg);
        else if (mode == "REFSB") add_refsb_parallel_payloads(tg);
        else if (mode == "BASELINE") add_payloads_to_tg(tg);  // 轻量流量，验证基础刷新时序
        else if (mode == "INTEGRATION") add_integration_payloads(tg);
        else add_payloads_to_tg(tg);
    } else {
        add_payloads_to_tg(tg);
    }

    sc_core::sc_start(200, sc_core::SC_US);
    return 0;
}
