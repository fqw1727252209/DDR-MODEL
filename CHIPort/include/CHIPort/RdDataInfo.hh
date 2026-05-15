#ifndef __RDA_INFO_HH__
#define __RDA_INFO_HH__

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <systemc>
#include <set>
#include <unordered_map>
#include <cassert>
#include <utility>


#include "ARM/TLM/arm_chi_payload.h"
#include "CHIPort/CHIUtilities.h"
#include "Configure/Configure.hh"

namespace dmu {
namespace Port {

struct RdDataInfoEntry {
  bool is_data_ready;
  // unsigned beat_count; // this is not used for
  ARM::CHI::Payload &payload;
  ARM::CHI::Phase phase;
  std::vector<uint8_t> data_ids;
  std::vector<uint8_t> remaining_data_ids; // Track remaining data IDs to send

  explicit RdDataInfoEntry(const CHIFlit &flit, const unsigned data_width_bytes): is_data_ready(false), payload(flit.payload), phase(flit.phase) 
  {
    payload.ref();
    data_ids = transaction_data_ids(payload, data_width_bytes);
    remaining_data_ids = data_ids; // Initially, all data IDs need to be sent
  }

  ~RdDataInfoEntry() { payload.unref(); }

  RdDataInfoEntry(const RdDataInfoEntry &other): is_data_ready(other.is_data_ready), data_ids(other.data_ids),remaining_data_ids(other.remaining_data_ids), payload(other.payload),phase(other.phase) 
  {
    payload.ref();
  }

  RdDataInfoEntry &operator=(const RdDataInfoEntry &) = delete;

  inline void set_data_ready() { is_data_ready = true; }
  inline bool is_receive_data_ready() const { return is_data_ready; }

  inline bool is_send_data_finished() const {return remaining_data_ids.empty();}
  inline bool is_send_data_sending() const {return remaining_data_ids.size() < data_ids.size();}

  inline uint8_t get_next_data_id() {
    assert(!remaining_data_ids.empty());
    uint8_t next_data_id = remaining_data_ids.front();
    remaining_data_ids.erase(remaining_data_ids.begin());
    return next_data_id;
  }

  // Check if there are more data IDs to send
  // inline bool has_more_data_ids() const {
  //     return !remaining_data_ids.empty();
  // }
};

class RdDataInfo {
public:
  explicit RdDataInfo(const Configure &configure, unsigned data_width_bytes)
      : rd_data_info_buffer_size(
            configure.controller_config->RD_DAT_INFO_DEPTH),
        data_width_bytes(data_width_bytes) {
    for (uint16_t i = 0; i < rd_data_info_buffer_size; ++i) {
      unused_buffer_id.insert(i);
    }
  }

private:
  std::set<uint16_t> unused_buffer_id;
  std::unordered_map<uint16_t, RdDataInfoEntry> rdata_info_buffer;
  const unsigned rd_data_info_buffer_size;
  const unsigned data_width_bytes;

public:
  void release_info_tag(uint16_t id) { unused_buffer_id.insert(id); }

  bool IsEmpty() const { return rdata_info_buffer.empty(); }

  bool IsFull() const {
    return rdata_info_buffer.size() >= rd_data_info_buffer_size;
  }

  uint16_t allocate_info_tag() {
    assert(!unused_buffer_id.empty());
    uint16_t id = *unused_buffer_id.begin();
    unused_buffer_id.erase(unused_buffer_id.begin());
    return id;
  }

  const unsigned size() const { return rdata_info_buffer.size(); }

  void allocate_info_buffer_entry(CHIFlit req_flit, uint16_t id) {
    rdata_info_buffer.emplace(id, RdDataInfoEntry(req_flit, data_width_bytes));
  }

  void set_entry_data_ready(uint16_t id) {
    rdata_info_buffer.at(id).set_data_ready();
  }

  void erase_entry(uint16_t id) {
    rdata_info_buffer.erase(id);
    release_info_tag(id);
  }

  RdDataInfoEntry get_entry(uint16_t id) { return rdata_info_buffer.at(id); }

  bool has_entry_ready() const {
    for (auto &entry : rdata_info_buffer) {
      if (entry.second.is_receive_data_ready())
        return true;
    }
    return false;
  }

  bool has_entry_sending_data() const {
    for (auto &entry : rdata_info_buffer) {
      if (entry.second.is_send_data_sending())
        return true;
    }
    return false;
  }

  // std::pair<uint16_t, RdDataInfoEntry> get_ready_entry() const {
  //     for(auto& entry: rdata_info_buffer){
  //         if(entry.second.is_receive_data_ready())
  //             return std::make_pair(entry.first, entry.second);
  //     }
  //     std::cerr << "Error: No entry ready in RdDataInfo" << std::endl;
  //     std::abort();
  // }

  std::tuple<uint16_t, RdDataInfoEntry, uint8_t> get_ready_entry() {
    for (auto &entry : rdata_info_buffer) {
      if (entry.second.is_receive_data_ready()) {
        uint8_t next_data_id =
            rdata_info_buffer.at(entry.first).get_next_data_id();
        return std::make_tuple(entry.first, rdata_info_buffer.at(entry.first),
                               next_data_id);
      }
    }
    std::cerr << "Error: No entry ready in RdDataInfo" << std::endl;
    std::abort();
  }

  std::tuple<uint16_t, RdDataInfoEntry, uint8_t> get_sending_data_entry() {
    for (auto &entry : rdata_info_buffer) {
      if (entry.second.is_send_data_sending() &&
          entry.second.is_receive_data_ready()) {
        uint8_t next_data_id =
            rdata_info_buffer.at(entry.first).get_next_data_id();
        return std::make_tuple(entry.first, rdata_info_buffer.at(entry.first),
                               next_data_id);
      }
    }
    std::cerr << "Error: No entry ready in RdDataInfo" << std::endl;
    std::abort();
  }

  bool is_entry_data_finished(uint16_t id) const {
    return rdata_info_buffer.at(id).is_send_data_finished();
  }

  // Method to get the next ready entry with its next data ID
  // std::tuple<uint16_t, RdDataInfoEntry, uint8_t>
  // get_ready_entry_with_data_id() {
  //     for(auto& entry: rdata_info_buffer){
  //         if(entry.second.is_receive_data_ready() &&
  //         !entry.second.is_send_data_finished()){
  //             uint8_t next_data_id =
  //             rdata_info_buffer.at(entry.first).get_next_data_id(); return
  //             std::make_tuple(entry.first, rdata_info_buffer.at(entry.first),
  //             next_data_id);
  //         }
  //     }
  //     std::cerr << "Error: No entry ready in RdDataInfo" << std::endl;
  //     std::abort();
  // }

  // Method to check if entry has more data IDs to send
  // bool entry_has_more_data_ids(uint16_t id) const {
  //     return rdata_info_buffer.at(id).has_more_data_ids();
  // }
};

} // namespace Port
} // namespace dmu

#endif // __RDA_INFO_HH__