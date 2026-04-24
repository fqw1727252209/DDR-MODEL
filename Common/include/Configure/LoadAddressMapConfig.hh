#ifndef __LOAD_ADDRESS_MAP_CONFIG_HH__
#define __LOAD_ADDRESS_MAP_CONFIG_HH__

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>

#include <bitset>
#include <cmath>
#include <iomanip>

#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"

#include "Configure/LoadConfigFromJson.hh"
namespace dmu{

struct AddressMapping
{
    static constexpr std::string_view SUB_DIR = "addressmapping";

    /* Since the vector store all address map bit, this may no need
    to use the **MaskBits
    */
    // std::vector<unsigned> vBaMaskBits;
    // std::vector<unsigned> vCidMaskBits;
    // std::vector<unsigned> vRowHashBits;
    // std::vector<unsigned> vAddrMaskBits;

    std::vector<unsigned> vChannelBits;
    std::vector<unsigned> vCsBits;
    std::vector<unsigned> vCidBits;
    std::vector<unsigned> vBankGroupBits; //
    std::vector<unsigned> vBankBits; // the Bankgroup + Bank will be combined to real Ba address
    std::vector<unsigned> vRowBits;
    std::vector<unsigned> vColumnBits;
    std::vector<unsigned> vByteBits;

};

class AddressMapConfig: public LoadConfigFromJson
{
    public:
        // AddressMapping address_mapping_;
        AddressMapping address_mapping;
        explicit AddressMapConfig(const std::string& filename)
        {
            LoadFromJson(filename);
            ParseJson();
        }
        ~AddressMapConfig() = default;
    private:
        bool ParseJson() override
        {
            if(doc.HasMember("addressmapping") && doc["addressmapping"].IsObject())
            {
                const rapidjson::Value& AddressMapping = doc["addressmapping"];
                if(AddressMapping.HasMember("CHANNEL_BIT") && AddressMapping["CHANNEL_BIT"].IsArray())
                {
                    const rapidjson::Value& channel_bit = AddressMapping["CHANNEL_BIT"];
                    for (rapidjson::SizeType i = 0; i < channel_bit.Size(); ++i) {
                        if (channel_bit[i].IsUint()) {
                            (address_mapping.vChannelBits).push_back(channel_bit[i].GetUint());
                        }
                    }
                }

                if(AddressMapping.HasMember("CS_BIT") && AddressMapping["CS_BIT"].IsArray())
                {
                    const rapidjson::Value& cs_bit = AddressMapping["CS_BIT"];
                    for (rapidjson::SizeType i = 0; i < cs_bit.Size(); ++i) {
                        if (cs_bit[i].IsUint()) {
                            (address_mapping.vCsBits).push_back(cs_bit[i].GetUint());
                        }
                    }
                }

                if(AddressMapping.HasMember("CID_BIT") && AddressMapping["CID_BIT"].IsArray())
                {
                    const rapidjson::Value& cid_bit = AddressMapping["CID_BIT"];
                    for (rapidjson::SizeType i = 0; i < cid_bit.Size(); ++i) {
                        if (cid_bit[i].IsUint()) {
                            (address_mapping.vCidBits).push_back(cid_bit[i].GetUint());
                        }
                    }
                }

                if(AddressMapping.HasMember("BANKGROUP_BIT") && AddressMapping["BANKGROUP_BIT"].IsArray())
                {
                    const rapidjson::Value& bg_bit = AddressMapping["BANKGROUP_BIT"];
                    for (rapidjson::SizeType i = 0; i < bg_bit.Size(); ++i) {
                        if (bg_bit[i].IsUint()) {
                            (address_mapping.vBankGroupBits).push_back(bg_bit[i].GetUint());
                        }
                    }
                }

                if(AddressMapping.HasMember("BANK_BIT") && AddressMapping["BANK_BIT"].IsArray())
                {
                    const rapidjson::Value& bk_bit = AddressMapping["BANK_BIT"];
                    for (rapidjson::SizeType i = 0; i < bk_bit.Size(); ++i) {
                        if (bk_bit[i].IsUint()) {
                            (address_mapping.vBankBits).push_back(bk_bit[i].GetUint());
                        }
                    }
                }

                if(AddressMapping.HasMember("ROW_BIT") && AddressMapping["ROW_BIT"].IsArray())
                {
                    const rapidjson::Value& row_bit = AddressMapping["ROW_BIT"];
                    for (rapidjson::SizeType i = 0; i < row_bit.Size(); ++i) {
                        if (row_bit[i].IsUint()) {
                            (address_mapping.vRowBits).push_back(row_bit[i].GetUint());
                        }
                    }
                }

                if(AddressMapping.HasMember("COLUMN_BIT") && AddressMapping["COLUMN_BIT"].IsArray())
                {
                    const rapidjson::Value& col_bit = AddressMapping["COLUMN_BIT"];
                    for (rapidjson::SizeType i = 0; i < col_bit.Size(); ++i) {
                        if (col_bit[i].IsUint()) {
                            (address_mapping.vColumnBits).push_back(col_bit[i].GetUint());
                        }
                    }
                }

                if(AddressMapping.HasMember("BYTE_BIT") && AddressMapping["BYTE_BIT"].IsArray())
                {
                    const rapidjson::Value& byte_bit = AddressMapping["BYTE_BIT"];
                    for (rapidjson::SizeType i = 0; i < byte_bit.Size(); ++i) {
                        if (byte_bit[i].IsUint()) {
                            (address_mapping.vByteBits).push_back(byte_bit[i].GetUint());
                        }
                    }
                }
            }
            else{
                std::cerr<< "Address Mapping Json File ont have adress mapping OBJ" <<std::endl;
                std::abort();
            }
            return false;
        }
};

} // namespace dmu

#endif