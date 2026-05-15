#include <bitset>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>

#include <string>
#include <systemc>

#include "Configure/AddressDecoder.hh"
#include "Configure/DDR5MemSpec.hh"
#include "Configure/DDR5MemSpec3ds.hh"
#include "sysc/utils/sc_report.h"

namespace dmu{

AddressDecoder::
AddressDecoder(const AddressMapping& address_mapping, const Controller::DDR5MemSpec3ds& mem_spec, bool bank_hash_enable)
: bank_hash_enable(bank_hash_enable)
{
    const auto& channelBits = address_mapping.vChannelBits;
    if(!channelBits.empty()){
        std::copy(channelBits.begin(),channelBits.end(),std::back_insert_iterator(vChannelBits));
    }
    const auto& subchannelBits = address_mapping.vSubChannelBits;
    if(!subchannelBits.empty()){
        std::copy(subchannelBits.begin(),subchannelBits.end(),std::back_insert_iterator(vSubChannelBits));
    }
    const auto& pseudochannelBits = address_mapping.vPseudoChannelBits;
    if(!pseudochannelBits.empty()){
        std::copy(pseudochannelBits.begin(),pseudochannelBits.end(),std::back_insert_iterator(vPseudoChannelBits));
    }

    const auto& prankBits = address_mapping.vCsBits;
    if(!prankBits.empty()){
        std::copy(prankBits.begin(),prankBits.end(),std::back_insert_iterator(vCsBits));
    }
    const auto& lrankBits = address_mapping.vCidBits;
    if(!lrankBits.empty()){
        std::copy(lrankBits.begin(),lrankBits.end(),std::back_insert_iterator(vCidBits));
    }
    const auto& bgBits = address_mapping.vBankGroupBits;
    if(!bgBits.empty()){
        std::copy(bgBits.begin(),bgBits.end(),std::back_insert_iterator(vBankGroupBits));
    }
    const auto& bankBits = address_mapping.vBankBits;
    if(!bankBits.empty()){
        std::copy(bankBits.begin(),bankBits.end(),std::back_insert_iterator(vBankBits));
    }
    const auto& rowBits = address_mapping.vRowBits;
    if(!rowBits.empty()){
        std::copy(rowBits.begin(),rowBits.end(),std::back_insert_iterator(vRowBits));
    }
    const auto& columnBits = address_mapping.vColumnBits;
    if(!columnBits.empty()){
        std::copy(columnBits.begin(),columnBits.end(),std::back_insert_iterator(vColumnBits));
    }
    const auto& wordBits = address_mapping.vByteBits;
    if(!wordBits.empty()){
        std::copy(wordBits.begin(),wordBits.end(),std::back_insert_iterator(vByteBits));
    }

    const auto& rowhashBits = address_mapping.vRowHashBits;
    if(bank_hash_enable && rowhashBits.empty())
    {
        SC_REPORT_FATAL("AddressDecoder", "Bank Hash is enabled but no RowHashBits are defined in Addresss Mapping Configuration");
    }
    if(!rowhashBits.empty()){
        std::copy(rowhashBits.begin(),rowhashBits.end(),std::back_insert_iterator(vRowHashBits));
    }


    unsigned channels = std::lround(std::pow(2.0,vChannelBits.size()));
    unsigned subchannels = std::lround(std::pow(2.0,vSubChannelBits.size()));
    unsigned pseudochannels = std::lround(std::pow(2.0,vPseudoChannelBits.size()));
    unsigned cs = std::lround(std::pow(2.0,vCsBits.size()));
    unsigned cids = std::lround(std::pow(2.0,vCidBits.size()));
    unsigned bankgroups = std::lround(std::pow(2.0,vBankGroupBits.size()));
    unsigned banks = std::lround(std::pow(2.0,vBankBits.size()));
    unsigned rows = std::lround(std::pow(2.0,vRowBits.size()));
    unsigned columns = std::lround(std::pow(2.0,vColumnBits.size()));
    unsigned bytes = std::lround(std::pow(2.0,vByteBits.size()));

    if(bankgroups != 8 && bankgroups != 4)
    {
        std::string msg = "Only 8 or 4 BankGroups per Device are supported in DDR5, but BankGroups are " + std::to_string(bankgroups);
        SC_REPORT_FATAL("AddressDecoder", msg.c_str());
    }
    if(banks != 2 && banks != 4)
    {
        std::string msg = "Only 2 or 4 Banks per BankGroup are supported in DDR5, but Banks are " + std::to_string(banks);
        SC_REPORT_FATAL("AddressDecoder", msg.c_str());
    }
    if( bank_hash_enable && vRowHashBits.size() != 17)
    {
        std::string msg = "Bank Hash is enabled but RowHashBits are not correct size, 17 required but ("
                          +std::to_string(vRowHashBits.size())+")  offerd in Addresss Mapping Configuration";
        SC_REPORT_FATAL("AddressDecoder", msg.c_str());
    }

    /*
    计算最大地址范围，以及地址范围对应的总bit位数，依次检测最大地址的每一个bit position是否存在于已保存的对应dram physical addr field中
    出现次数不为1都是非法的
    */
    maximumAddress = 
        static_cast<uint64_t>(bytes) * columns * rows * banks * bankgroups * cids * cs * pseudochannels * subchannels * channels - 1;
    auto totalAddressBits = static_cast<unsigned>(std::log2(maximumAddress));
    
    for (unsigned bitPosition = 0; bitPosition < totalAddressBits; bitPosition++)
    {
        if (std::count(vChannelBits.begin(), vChannelBits.end(), bitPosition) +
            std::count(vSubChannelBits.begin(), vSubChannelBits.end(), bitPosition) +
            std::count(vPseudoChannelBits.begin(), vPseudoChannelBits.end(), bitPosition) +
            std::count(vCsBits.begin(), vCsBits.end(), bitPosition) +
            std::count(vCidBits.begin(), vCidBits.end(), bitPosition) +
            std::count(vBankGroupBits.begin(), vBankGroupBits.end(), bitPosition) +
            std::count(vBankBits.begin(), vBankBits.end(), bitPosition) +
            std::count(vRowBits.begin(), vRowBits.end(), bitPosition) +
            std::count(vColumnBits.begin(), vColumnBits.end(), bitPosition) +
            std::count(vByteBits.begin(), vByteBits.end(), bitPosition) !=
            1)
            SC_REPORT_FATAL("AddressDecoder", ("Not all address bits occur exactly once, " + 
                          std::to_string(bitPosition) + " defined more than one times").c_str());
    }
    
    int highestByteBit = -1;

    if (!vByteBits.empty())
    {
        highestByteBit = static_cast<int>(*std::max_element(vByteBits.begin(), vByteBits.end()));

        for (unsigned bitPosition = 0; bitPosition <= static_cast<unsigned>(highestByteBit);
             bitPosition++)
        {
            if (std::find(vByteBits.begin(), vByteBits.end(), bitPosition) == vByteBits.end())
                SC_REPORT_FATAL("AddressDecoder", "Byte bits are not continuous starting from 0");
        }
    }

    auto maxBurstLengthBits = static_cast<unsigned>(std::log2(mem_spec.BurstLenth));
    for (unsigned bitPosition = highestByteBit + 1; bitPosition < highestByteBit + 1 + maxBurstLengthBits; bitPosition++)
    {
        if (std::find(vColumnBits.begin(), vColumnBits.end(), bitPosition) == vColumnBits.end())
            SC_REPORT_FATAL("AddressDecoder", "No continuous column bits for maximum burst length");
    }

    // 修改为更详细的错误报告，指出具体哪个条件不满足
    if (mem_spec.NumOfChannels != channels) {
        SC_REPORT_FATAL("AddressDecoder",
            ("MemSpec and address mapping do not match: NumOfChannels mismatch. MemSpec: " +
            std::to_string(mem_spec.NumOfChannels) + ", AddressMapping: " + std::to_string(channels)).c_str());
    }
    if (mem_spec.NumOfPhysicalRanksPerPseudoChannel != cs) {
        SC_REPORT_FATAL("AddressDecoder",
            ("MemSpec and address mapping do not match: NumOfPhysicalRanksPerSubChannel mismatch. MemSpec: " +
            std::to_string(mem_spec.NumOfPhysicalRanksPerPseudoChannel) + ", AddressMapping: " + std::to_string(cs)).c_str());
    }
    if (mem_spec.NumOfLogicalRanksPerPhysicalRank != cids) {
        SC_REPORT_FATAL("AddressDecoder",
            ("MemSpec and address mapping do not match: NumOfLogicalRanksPerPhysicalRank mismatch. MemSpec: " +
            std::to_string(mem_spec.NumOfLogicalRanksPerPhysicalRank) + ", AddressMapping: " + std::to_string(cids)).c_str());
    }
    if (mem_spec.NumOfBankGroupsPerDevice != bankgroups) {
        SC_REPORT_FATAL("AddressDecoder",
            ("MemSpec and address mapping do not match: NumOfBankGroupsPerDevice mismatch. MemSpec: " +
            std::to_string(mem_spec.NumOfBankGroupsPerDevice) + ", AddressMapping: " + std::to_string(bankgroups)).c_str());
    }
    if (mem_spec.NumOfBanksPerBankGroup != banks) {
        SC_REPORT_FATAL("AddressDecoder",
            ("MemSpec and address mapping do not match: NumOfBanksPerBankGroup mismatch. MemSpec: " +
            std::to_string(mem_spec.NumOfBanksPerBankGroup) + ", AddressMapping: " + std::to_string(banks)).c_str());
    }
    if (mem_spec.NumOfRows != rows) {
        SC_REPORT_FATAL("AddressDecoder",
            ("MemSpec and address mapping do not match: NumOfRows mismatch. MemSpec: " +
            std::to_string(mem_spec.NumOfRows) + ", AddressMapping: " + std::to_string(rows)).c_str());
    }
    if (mem_spec.NumOfColumns != columns) {
        SC_REPORT_FATAL("AddressDecoder",
            ("MemSpec and address mapping do not match: NumOfColumns mismatch. MemSpec: " +
            std::to_string(mem_spec.NumOfColumns) + ", AddressMapping: " + std::to_string(columns)).c_str());
    }
}

void
AddressDecoder::print() const
{
    std::cout << headline << std::endl;
    std::cout << "Used Address Mapping:" << std::endl;
    std::cout << std::endl;

    for (int it = static_cast<int>(vChannelBits.size() - 1); it >= 0; it--)
    {
        uint64_t addressBits =
            (UINT64_C(1) << vChannelBits[static_cast<std::vector<unsigned>::size_type>(it)]);

        std::cout << " Ch  " << std::setw(2) << it << ": " << std::bitset<64>(addressBits)
                  << std::endl;
    }

    for (int it = static_cast<int>(vCsBits.size() - 1); it >= 0; it--)
    {
        uint64_t addressBits =
            (UINT64_C(1) << vCsBits[static_cast<std::vector<unsigned>::size_type>(it)]);

        std::cout << " Cs  " << std::setw(2) << it << ": " << std::bitset<64>(addressBits)
                  << std::endl;
    }

    for (int it = static_cast<int>(vCidBits.size() - 1); it >= 0; it--)
    {
        uint64_t addressBits =
            (UINT64_C(1) << vCidBits[static_cast<std::vector<unsigned>::size_type>(it)]);

        std::cout << " Cid " << std::setw(2) << it << ": " << std::bitset<64>(addressBits)
                  << std::endl;
    }

    for (int it = static_cast<int>(vBankGroupBits.size() - 1); it >= 0; it--)
    {
        uint64_t addressBits =
            (UINT64_C(1) << vBankGroupBits[static_cast<std::vector<unsigned>::size_type>(it)]);

        std::cout << " Bg  " << std::setw(2) << it << ": " << std::bitset<64>(addressBits)
                  << std::endl;
    }

    for (int it = static_cast<int>(vBankBits.size() - 1); it >= 0; it--)
    {
        uint64_t addressBits =
            (UINT64_C(1) << vBankBits[static_cast<std::vector<unsigned>::size_type>(it)]);

        std::cout << " Ba  " << std::setw(2) << it << ": " << std::bitset<64>(addressBits)
                  << std::endl;
    }

    for (int it = static_cast<int>(vRowBits.size() - 1); it >= 0; it--)
    {
        uint64_t addressBits =
            (UINT64_C(1) << vRowBits[static_cast<std::vector<unsigned>::size_type>(it)]);

        std::cout << " Ro  " << std::setw(2) << it << ": " << std::bitset<64>(addressBits)
                  << std::endl;
    }

    for (int it = static_cast<int>(vColumnBits.size() - 1); it >= 0; it--)
    {
        uint64_t addressBits =
            (UINT64_C(1) << vColumnBits[static_cast<std::vector<unsigned>::size_type>(it)]);

        std::cout << " Co  " << std::setw(2) << it << ": " << std::bitset<64>(addressBits)
                  << std::endl;
    }

    for (int it = static_cast<int>(vByteBits.size() - 1); it >= 0; it--)
    {
        uint64_t addressBits =
            (UINT64_C(1) << vByteBits[static_cast<std::vector<unsigned>::size_type>(it)]);

        std::cout << " By  " << std::setw(2) << it << ": " << std::bitset<64>(addressBits)
                  << std::endl;
    }

    std::cout << std::endl;
}

DecodedAddress
AddressDecoder::decodeAddress(uint64_t encAddr) const
{
    if (encAddr > maximumAddress)
    {
        SC_REPORT_ERROR("AddressDecoder",
                        ("Address " + std::to_string(encAddr) + " out of range (maximum addrss is " + std::to_string(maximumAddress) + ")")
                        .c_str());
    }
    DecodedAddress decAddr;

    for (unsigned it = 0; it < vChannelBits.size(); it++)
        decAddr.channel |= ((encAddr >> vChannelBits[it]) & UINT64_C(1)) << it;

    for (unsigned it = 0; it < vSubChannelBits.size(); it++)
        decAddr.sub_channel |= ((encAddr >> vSubChannelBits[it]) & UINT64_C(1)) << it;

    for (unsigned it = 0; it < vPseudoChannelBits.size(); it++)
        decAddr.pseudo_channel |= ((encAddr >> vPseudoChannelBits[it]) & UINT64_C(1)) << it;

    for (unsigned it = 0; it < vCsBits.size(); it++)
        decAddr.cs |= ((encAddr >> vCsBits[it]) & UINT64_C(1)) << it;

    for (unsigned it = 0; it < vCidBits.size(); it++)
        decAddr.cid |= ((encAddr >> vCidBits[it]) & UINT64_C(1)) << it;

    if(!bank_hash_enable)
    {
        for (unsigned it = 0; it < vBankGroupBits.size(); it++)
            decAddr.bankgroup |= ((encAddr >> vBankGroupBits[it]) & UINT64_C(1)) << it;

        for (unsigned it = 0; it < vBankBits.size(); it++)
            decAddr.bank |= ((encAddr >> vBankBits[it]) & UINT64_C(1)) << it;
    }
    else
    {
        // When bank hash is enabled, calculate bank and bankgroup based on vRowHashBits
        // bank bit 0: encAddr[vBankBits[0]] ^ encAddr[vRowHashBits[0]] ^ encAddr[vRowHashBits[1]] ^ encAddr[vRowHashBits[2]]
        if (vBankBits.size() > 0) {
            decAddr.bank |= (((encAddr >> vBankBits[0]) & UINT64_C(1)) ^
                             ((encAddr >> vRowHashBits[0]) & UINT64_C(1)) ^
                             ((encAddr >> vRowHashBits[1]) & UINT64_C(1)) ^
                             ((encAddr >> vRowHashBits[2]) & UINT64_C(1))) << 0;
        }

        // bank bit 1: encAddr[vBankBits[1]] ^ encAddr[vRowHashBits[3]] ^ encAddr[vRowHashBits[4]] ^ encAddr[vRowHashBits[5]]
        if (vBankBits.size() > 1) {
            decAddr.bank |= (((encAddr >> vBankBits[1]) & UINT64_C(1)) ^
                             ((encAddr >> vRowHashBits[3]) & UINT64_C(1)) ^
                             ((encAddr >> vRowHashBits[4]) & UINT64_C(1)) ^
                             ((encAddr >> vRowHashBits[5]) & UINT64_C(1))) << 1;
        }

        // bankgroup bit 0: encAddr[vBankGroupBits[0]] ^ encAddr[vRowHashBits[6]] ^ encAddr[vRowHashBits[7]] ^ encAddr[vRowHashBits[8]] ^ encAddr[vRowHashBits[9]]
        if (vBankGroupBits.size() > 0) {
            decAddr.bankgroup |= (((encAddr >> vBankGroupBits[0]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[6]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[7]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[8]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[9]) & UINT64_C(1))) << 0;
        }

        // bankgroup bit 1: encAddr[vBankGroupBits[1]] ^ encAddr[vRowHashBits[10]] ^ encAddr[vRowHashBits[11]] ^ encAddr[vRowHashBits[12]] ^ encAddr[vRowHashBits[13]]
        if (vBankGroupBits.size() > 1) {
            decAddr.bankgroup |= (((encAddr >> vBankGroupBits[1]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[10]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[11]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[12]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[13]) & UINT64_C(1))) << 1;
        }

        // bankgroup bit 2: encAddr[vBankGroupBits[2]] ^ encAddr[vRowHashBits[14]] ^ encAddr[vRowHashBits[15]] ^ encAddr[vRowHashBits[16]] ^ encAddr[vRowHashBits[17]]
        if (vBankGroupBits.size() > 2) {
            decAddr.bankgroup |= (((encAddr >> vBankGroupBits[2]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[14]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[15]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[16]) & UINT64_C(1)) ^
                                  ((encAddr >> vRowHashBits[17]) & UINT64_C(1))) << 2;
        }
    }

    for (unsigned it = 0; it < vRowBits.size(); it++)
        decAddr.row |= ((encAddr >> vRowBits[it]) & UINT64_C(1)) << it;

    for (unsigned it = 0; it < vColumnBits.size(); it++)
        decAddr.column |= ((encAddr >> vColumnBits[it]) & UINT64_C(1)) << it;

    for (unsigned it = 0; it < vByteBits.size(); it++)
        decAddr.byte |= ((encAddr >> vByteBits[it]) & UINT64_C(1)) << it;

    decAddr.real_ba = decAddr.bank | (decAddr.bankgroup << (vBankBits.size())) |
                      (decAddr.cid << (vBankGroupBits.size() + vBankBits.size())) |
                      (decAddr.cs << (vCidBits.size() + vBankGroupBits.size() + vBankBits.size()));

    decAddr.real_bg = decAddr.bankgroup |
                      (decAddr.cid << (vBankGroupBits.size())) |
                      (decAddr.cs << (vCidBits.size() + vBankGroupBits.size()));

    decAddr.real_cid = decAddr.cid | (decAddr.cs << (vCidBits.size()))
                     // | (decAddr.pseudo_channel << (vPseudoChannelBits.size() + vCsBits.size()))
                     ;

    decAddr.real_cs = decAddr.cs
                     // | (decAddr.pseudo_channel << (vPseudoChannelBits.size()))
                     ;

    return decAddr;
}

unsigned
AddressDecoder::decodeChannel(uint64_t encAddr) const
{
    if (encAddr > maximumAddress)
        SC_REPORT_WARNING("AddressDecoder",
                          ("Address " + std::to_string(encAddr) +
                           " out of range (maximum address is " + std::to_string(maximumAddress) +
                           ")")
                          .c_str());

    if (vChannelBits.size() == 0)
        SC_REPORT_FATAL("AddressDecoder", "No channel bits defined");

    unsigned channel = 0;
    for (unsigned it = 0; it < vChannelBits.size(); it++)
        channel |= ((encAddr >> vChannelBits[it]) & UINT64_C(1)) << it;

    return channel;
}

unsigned
AddressDecoder::decodeSubChannels(uint64_t encAddr) const
{
    if (encAddr > maximumAddress)
        SC_REPORT_WARNING("AddressDecoder",
                          ("Address " + std::to_string(encAddr) +
                           " out of range (maximum address is " + std::to_string(maximumAddress) +
                           ")")
                          .c_str());

    if (vSubChannelBits.size() == 0)
        SC_REPORT_FATAL("AddressDecoder", "No sub-channel bits defined");

    unsigned subChannel = 0;
    for (unsigned it = 0; it < vSubChannelBits.size(); it++)
        subChannel |= ((encAddr >> vSubChannelBits[it]) & UINT64_C(1)) << it;

    return subChannel;
}

unsigned
AddressDecoder::decodePseudoChannel(uint64_t encAddr) const
{
    if (encAddr > maximumAddress)
        SC_REPORT_WARNING("AddressDecoder",
                          ("Address " + std::to_string(encAddr) +
                           " out of range (maximum address is " + std::to_string(maximumAddress) +
                           ")")
                          .c_str());

    if (vPseudoChannelBits.size() == 0)
        SC_REPORT_FATAL("AddressDecoder", "Pseudo channel bits not defined");

    unsigned pseudoChannel = 0;
    for (unsigned it = 0; it < vPseudoChannelBits.size(); it++)
        pseudoChannel |= ((encAddr >> vPseudoChannelBits[it]) & UINT64_C(1)) << it;

    return pseudoChannel;
}

uint64_t
AddressDecoder::encodeAddress(const DecodedAddress &decodedAddress) const
{
    uint64_t encodedAddress = 0;
    for (unsigned it = 0; it < vChannelBits.size(); it++)
        encodedAddress |= ((decodedAddress.channel >> it) & UINT64_C(1)) << vChannelBits[it];

    for (unsigned it = 0; it < vCsBits.size(); it++)
        encodedAddress |= ((decodedAddress.cs >> it) & UINT64_C(1)) << vCsBits[it];

    for (unsigned it = 0; it < vCidBits.size(); it++)
        encodedAddress |= ((decodedAddress.cid >> it) & UINT64_C(1)) << vCidBits[it];

    for (unsigned it = 0; it < vBankGroupBits.size(); it++)
        encodedAddress |= ((decodedAddress.bankgroup >> it) & UINT64_C(1)) << vBankGroupBits[it];

    for (unsigned it = 0; it < vBankBits.size(); it++)
        encodedAddress |= ((decodedAddress.bank >> it) & UINT64_C(1)) << vBankBits[it];

    for (unsigned it = 0; it < vRowBits.size(); it++)
        encodedAddress |= ((decodedAddress.row >> it) & UINT64_C(1)) << vRowBits[it];

    for (unsigned it = 0; it < vColumnBits.size(); it++)
        encodedAddress |= ((decodedAddress.column >> it) & UINT64_C(1)) << vColumnBits[it];

    for (unsigned it = 0; it < vByteBits.size(); it++)
        encodedAddress |= ((decodedAddress.byte >> it) & UINT64_C(1)) << vByteBits[it];

    return encodedAddress;
}

} // namespace dmu