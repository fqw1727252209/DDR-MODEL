#ifndef __ADDRESS_DECODER_H__
#define __ADDRESS_DECODER_H__

#include <cstdint>
#include <utility>
#include <vector>

#include "Configure/DDR5MemSpec.hh"
#include "Configure/DDR5MemSpec3ds.hh"
#include "Configure/LoadAddressMapConfig.hh"
#include "Configure/LoadMemSpec.hh"

namespace dmu {
// since the addr map may be needed in Port stage, so the address map should in
// dmu namespace;
/*
addr will map to the following fields:
Bytes:  the DDR5 bit width is 32 bit, whenever a beat, 4 Bytes will transfer
from controller to devices, but system address is byte-based address visitig, so
lowest log2 (4)  = 2 bit, [1:0] will be masked, since a SDRAM address offer one
SDRAM address with burst lenth data transfer, Column: Row: Bank: BankGroup: Cid:
// if exist 3DS device, Cid used to indentify logical rank Cs:     // if exist
3DS device, Cs used to indentify physical rank, oherwise, Cs used to indentify
rank Channel:
*/

struct DecodedAddress {
  DecodedAddress() = default;

  unsigned channel = 0;
  unsigned sub_channel = 0;
  unsigned pseudo_channel = 0;
  unsigned cs = 0;
  unsigned cid = 0;
  unsigned bankgroup = 0;
  unsigned bank = 0;

  unsigned hash_bankgroup = 0;
  unsigned hash_bank = 0;

  unsigned row = 0;
  unsigned column = 0;
  unsigned byte = 0;

  // this is to indentify real bankgroup and bank and cid
  unsigned real_ba = 0;
  unsigned real_bg = 0;
  unsigned real_cid = 0;
  unsigned real_cs = 0;

  friend std::ostream &operator<<(std::ostream &os, const DecodedAddress &da) {
    os << std::left << "{"
       << "channel: " << da.channel << ",\t"
       << "sub_channel: " << da.sub_channel << ",\t"
       << "pseudo_channel: " << da.pseudo_channel << ",\t"
       << "cs: " << da.cs << ",\t"
       << "cid: " << da.cid << ",\t"
       << "bankgroup: " << da.bankgroup << ",\t"
       << "bank: " << da.bank << ",\t"
       << "row: " << std::setw(8) << da.row << ",\t"
       << "column: " << std::setw(6) << da.column << " },"
       << "real_ba: " << da.real_ba;
    return os;
  }

  bool operator==(const DecodedAddress &comp) const {
    return sub_channel == comp.sub_channel &&
           pseudo_channel == comp.pseudo_channel && cs == comp.cs &&
           cid == comp.cid && bankgroup == comp.bankgroup &&
           bank == comp.bank && row == comp.row && column == comp.column;
  }

  bool operator!=(const DecodedAddress &comp) const {
    return !(sub_channel == comp.sub_channel &&
             pseudo_channel == comp.pseudo_channel && cs == comp.cs &&
             cid == comp.cid && bankgroup == comp.bankgroup &&
             bank == comp.bank && row == comp.row && column == comp.column);
  }
};

class AddressDecoder {
public:
  AddressDecoder(const AddressMapping &address_mapping,
                 const Controller::DDR5MemSpec3ds &mem_spec,
                 bool bank_hash_enable);
  DecodedAddress decodeAddress(uint64_t encAddr) const;
  unsigned decodeChannel(uint64_t encAddr) const;
  unsigned decodeSubChannels(uint64_t encAddr) const;
  unsigned decodePseudoChannel(uint64_t encAddr) const;
  uint64_t encodeAddress(const DecodedAddress &decodedAddress) const;
  void print() const;

private:
  unsigned banksPerGroup;
  unsigned bankgroupsPerCid;
  unsigned bankgroupsPerCs;

  uint64_t maximumAddress;

  const bool
      bank_hash_enable; // 如果启用了 bank hash，则 RowHashBits vector不能为空
  std::vector<unsigned> vChannelBits;
  std::vector<unsigned> vSubChannelBits;
  std::vector<unsigned> vPseudoChannelBits;

  std::vector<unsigned> vCsBits;
  std::vector<unsigned> vCidBits;
  std::vector<unsigned> vBankGroupBits;
  std::vector<unsigned> vBankBits;
  std::vector<unsigned> vRowBits;
  std::vector<unsigned> vColumnBits;
  std::vector<unsigned> vByteBits;

  std::vector<unsigned> vRowHashBits;
};

} // namespace dmu

#endif