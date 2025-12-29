#include "DMU/DramMannageUnit.h"

namespace dmu{

DramMannageUnit::DramMannageUnit(DRAMSys::Config::Configuration configuration,
                                 std::filesystem::path resourceDirectory,
                                 const unsigned data_width_bits) :
    configuration(std::move(configuration)),
    resourceDirectory(std::move(resourceDirectory)),
    data_width_bits(data_width_bits)
{
    if (this->configuration.simconfig.DatabaseRecording.value_or(false))
    {
        dramSys = std::make_unique<DRAMSys::DRAMSysRecordable>("DRAMSys", this->configuration);
    }
    else
    {
        dramSys = std::make_unique<DRAMSys::DRAMSys>("DRAMSys", this->configuration);
    }

    chi_port = std::make_unique<CHIPort>("CHIPort", this->data_width_bits);
    chi_port->iSocket.bind(dramSys->tSocket);
    
}


} // end dmu