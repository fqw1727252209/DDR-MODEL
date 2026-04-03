#ifndef __DRAM_MANAGE_UNIT_HH__
#define __DRAM_MANAGE_UNIT_HH__

#include "CHIPort/CHIPort.hh"

#include "Configure/Configure.hh"
#include "Configure/LoadConfigure.hh"
#include "Controller/MemoryController.hh"
#include "Controller/MemoryDevice.hh"
#include "Controller/SdramConstraint.hh"
#include "sysc/communication/sc_clock.h"
#include <memory>
namespace dmu {

class DramManagerUnit {
public:
    explicit DramManagerUnit(const std::string& name,
        const sc_core::sc_clock& noc_clock,const unsigned chi_data_width_bits,
        const std::string& configure_base_dir, const std::string& configure_filename,const std::string& output_dir="./");

    ~DramManagerUnit() = default;
    std::unique_ptr<Port::CHIPort> chi_port_0;
private:
    std::unique_ptr<sc_core::sc_clock> dfi_clock;
    const sc_core::sc_clock& noc_clock;
    std::unique_ptr<LoadConfigure> load_configure;
    std::unique_ptr<Configure> configure;

    std::unique_ptr<Controller::SdramConstraintIF> sdram_constraint;

    std::unique_ptr<Controller::MemoryController> controller_0;
    std::unique_ptr<Controller::MemoryDevice> device_0;

    // std::unique_ptr<Port::CHIPort> chi_port_1;
    // std::unique_ptr<Controller::MemoryController> controller_1;
    // std::unique_ptr<Controller::MemoryDevice> device_1;
};

}

#endif