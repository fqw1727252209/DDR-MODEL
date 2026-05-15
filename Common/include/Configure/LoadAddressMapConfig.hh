#ifndef __LOAD_ADDRESS_MAP_CONFIG_HH__
#define __LOAD_ADDRESS_MAP_CONFIG_HH__

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>


#include <bitset>
#include <cmath>
#include <iomanip>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include "Configure/LoadConfigFromJson.hh"

namespace dmu {

#define ADDRESS_MAPPING_FIELDS(M)                                              \
  M(std::vector<unsigned>, vChannelBits)                                       \
  M(std::vector<unsigned>, vSubChannelBits)                                    \
  M(std::vector<unsigned>, vPseudoChannelBits)                                 \
  M(std::vector<unsigned>, vCsBits)                                            \
  M(std::vector<unsigned>, vCidBits)                                           \
  M(std::vector<unsigned>, vBankGroupBits)                                     \
  M(std::vector<unsigned>, vBankBits)                                          \
  M(std::vector<unsigned>, vRowBits)                                           \
  M(std::vector<unsigned>, vColumnBits)                                        \
  M(std::vector<unsigned>, vByteBits)                                          \
  M(std::vector<unsigned>, vRowHashBits)

struct AddressMapping {
  static constexpr std::string_view SUB_DIR = "addressmapping";

#define ADDR_MAP_DECLARE(Type, Name) Type Name;
  ADDRESS_MAPPING_FIELDS(ADDR_MAP_DECLARE)
#undef ADDR_MAP_DECLARE
};

class AddressMapConfig : public LoadConfigFromJson {
public:
  BEGIN_JSON_MAP(AddressMapping)
#define ADDR_MAP_MAP(Type, Name) JSON_FIELD(Type, Name)
  ADDRESS_MAPPING_FIELDS(ADDR_MAP_MAP)
#undef ADDR_MAP_MAP
  END_JSON_MAP()

  AddressMapping address_mapping;
  explicit AddressMapConfig(const std::string &filename) {
    LoadFromJson(filename);
    ParseJson();
  }
  ~AddressMapConfig() = default;

private:
  bool ParseJson() override {
    if (!doc.IsObject()) {
      std::cerr << "JSON root is not an object!" << std::endl;
      return false;
    }

    if (!doc.HasMember("addressmapping") || !doc["addressmapping"].IsObject()) {
      std::cerr
          << "Address Mapping Json File does not have 'addressmapping' object"
          << std::endl;
      std::abort();
    }

    return ParseStruct(doc["addressmapping"], address_mapping);
  }
};

} // namespace dmu

#endif