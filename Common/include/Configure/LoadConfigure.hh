#ifndef __LOAD_CONFIGURE_HH__
#define __LOAD_CONFIGURE_HH__

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

#include "Configure/LoadAddressMapConfig.hh"
#include "Configure/LoadConfigFromJson.hh"
#include "Configure/LoadControllerConfig.hh"
#include "Configure/LoadMemSpec.hh"

/*
address map

scheduler 相关寄存器配置

AC timing 时序参数配置

static PHY delay
*/

namespace dmu {

struct ConfigurePath {
  std::string address_mapping_filename;
  std::string mem_spec_filename;
  std::string controller_config_filename;
};

class LoadConfigure : public LoadConfigFromJson {
private:
  ConfigurePath configure_path;
  const std::string base_dir;

public:
  explicit LoadConfigure(const std::string &_base_dir,
                         const std::string &filename)
      : base_dir(_base_dir) {
    std::filesystem::path base_dir_path = std::filesystem::path(base_dir);
    std::string real_filename = (base_dir_path / filename).string();
    LoadFromJson(real_filename);
  }

  BEGIN_JSON_MAP(ConfigurePath)
  JSON_FIELD(std::string, address_mapping_filename)
  JSON_FIELD(std::string, mem_spec_filename)
  JSON_FIELD(std::string, controller_config_filename)
  END_JSON_MAP()

  using LoadAddressMap = AddressMapConfig;
  std::unique_ptr<LoadDDR5MemConfig> load_mem_spec;
  std::unique_ptr<LoadAddressMap> load_address_map;
  std::unique_ptr<LoadControllerConfig> load_controller_config;

  bool ParseJson() override {
    if (!doc.IsObject()) {
      std::cerr << "JSON root is not an object!" << std::endl;
      return false;
    }

    bool parse_result = ParseStruct(doc, configure_path);

    return parse_result;
  }

  void ParseConfig() {
    std::string address_mapping_path =
        (std::filesystem::path(base_dir) / AddressMapping::SUB_DIR /
         configure_path.address_mapping_filename)
            .string();
    std::string mem_spec_path =
        (std::filesystem::path(base_dir) / DDR5MemConfig::SUB_DIR /
         configure_path.mem_spec_filename)
            .string();
    std::string controller_config_path =
        (std::filesystem::path(base_dir) / ControllerConfig::SUB_DIR /
         configure_path.controller_config_filename)
            .string();

    load_mem_spec = std::make_unique<LoadDDR5MemConfig>(mem_spec_path);
    load_mem_spec->ParseJson();
    load_address_map = std::make_unique<LoadAddressMap>(address_mapping_path);
    load_controller_config =
        std::make_unique<LoadControllerConfig>(controller_config_path);
    load_controller_config->ParseJson();
  }
};

} // namespace dmu

#endif