#ifndef __CONFIGURE_HH__
#define __CONFIGURE_HH__

#include <memory>

#include "Configure/LoadConfigure.hh"
#include "Configure/DDR5MemSpec3ds.hh"
#include "Configure/AddressDecoder.hh"
#include "Configure/McConfig.hh"
/*
address map

scheduler 相关寄存器配置

AC timing 时序参数配置

static PHY delay

*/

namespace dmu
{

    class Configure
    {
        public:
            explicit Configure(const LoadConfigure& loader, const std::string& output_dir = "./")
            {
                mem_spec          = std::make_unique<Controller::DDR5MemSpec3ds>(loader.load_mem_spec->mem_spec, output_dir);
                controller_config = std::make_unique<Controller::McConfig>(loader.load_controller_config->controller_config);
                address_decoder   = std::make_unique<AddressDecoder>(loader.load_address_map->address_mapping,*mem_spec.get(),controller_config->BANK_HASH_ENABLE);
            }
            std::unique_ptr<Controller::DDR5MemSpec3ds> mem_spec;
            std::unique_ptr<AddressDecoder> address_decoder;
            std::unique_ptr<Controller::McConfig> controller_config;

    };

}

#endif