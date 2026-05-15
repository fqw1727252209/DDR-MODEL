#include "DMU/DramManagerUnit.hh"
// #include "../include/DMU/DramManagerUnit.hh"
#include "Configure/LoadConfigure.hh"
#include "Controller/SdramConstraint.hh"
#include "sysc/communication/sc_clock.h"
#include <memory>

namespace dmu{
    DramManagerUnit::DramManagerUnit(const std::string& name,
        const sc_core::sc_clock& noc_clock, 
        const unsigned chi_data_width,
        const std::string& configure_base_dir, const std::string& configure_filename, const std::string& output_dir)
    :noc_clock(noc_clock)
    {
        load_configure = std::make_unique<LoadConfigure>(configure_base_dir, configure_filename);
        load_configure->ParseJson();
        load_configure->ParseConfig();

        configure = std::make_unique<Configure>(*load_configure.get());
        //TODO: 需要考虑不同DramManagerUnit被多个例化的情况
        Qos::ConfigureQosMap(configure->controller_config->RD_QOS_LEVEL_0,
        configure->controller_config->RD_QOS_LEVEL_1,
        configure->controller_config->WR_QOS_LEVEL);

        sdram_constraint = std::make_unique<Controller::SdramConstraintDDR5_3ds>(*configure->mem_spec.get());

        //创建dfi 时钟
        dfi_clock = std::make_unique<sc_core::sc_clock>((name+"_dfi_clock").c_str(),configure->mem_spec->tCK_mc);
        // 使用name作为前缀创建两个端口
        chi_port_0 = std::make_unique<Port::CHIPort>((name + "_chi_port_0").c_str(), *configure, chi_data_width,
                                                    dfi_clock->period());
        // chi_port_1 = std::make_unique<Port::CHIPort>((name + "_chi_port_1").c_str(), *configure, 256,
        //                                            dfi_clock.period());

        // 使用name作为前缀创建两个控制器
        controller_0 = std::make_unique<Controller::MemoryController>((name + "_memory_controller_0").c_str(),
                                                                     *configure,
                                                                     sdram_constraint.get());
        // controller_1 = std::make_unique<Controller::MemoryController>((name + "_memory_controller_1").c_str(),
        //                                                              *configure,
        //                                                              sdram_constraint.get());
        // 使用name作为前缀创建两个设备
        device_0 = std::make_unique<Controller::MemoryDevice>((name + "_memory_device_0").c_str(),
                                                             *configure,
                                                             output_dir);
        // device_1 = std::make_unique<Controller::MemoryDevice>((name + "_memory_device_1").c_str(),
        //                                                      *configure,
        //                                                      output_dir);

        // bind clock
        chi_port_0->dfi_clock.bind(*dfi_clock);
        chi_port_0->noc_clock.bind(noc_clock);
        controller_0->bind_dfi_clock(*dfi_clock);

        // bind tlm interface
        chi_port_0->iSocket.bind(controller_0->tSocket);
        controller_0->iSocket.bind(device_0->tSocket);
    }
}