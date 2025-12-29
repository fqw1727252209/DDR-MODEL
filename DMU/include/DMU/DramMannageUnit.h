# pragma once

#include <DRAMSys/config/DRAMSysConfiguration.h>
#include <DRAMSys/simulation/DRAMSysRecordable.h>

#include <CHIPort/CHIPort.h>



namespace dmu{

class DramMannageUnit
{
public:
    DramMannageUnit(DRAMSys::Config::Configuration configuration,
                    std::filesystem::path resourceDirectory,
                    unsigned data_width_bits);

    std::unique_ptr<DRAMSys::DRAMSys> dramSys;
    std::unique_ptr<CHIPort> chi_port;
private:

    std::filesystem::path resourceDirectory = DRAMSYS_RESOURCE_DIR;
    std::filesystem::path baseConfig = resourceDirectory / "ddr4-example.json";
    DRAMSys::Config::Configuration configuration = DRAMSys::Config::from_path(baseConfig.c_str(), resourceDirectory.c_str());

    //
    const unsigned data_width_bits = 256;
};

}