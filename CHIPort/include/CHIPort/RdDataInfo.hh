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

namespace dmu{
    namespace Port{

struct RdDataInfoEntry{
    bool is_data_ready;
    unsigned beat_count; // this is not used for
    ARM::CHI::Payload& payload;
    ARM::CHI::Phase phase;

    RdDataInfoEntry(const CHIFlit& flit):is_data_ready(false), beat_count(0), payload(flit.payload), phase(flit.phase)
    {
        payload.ref();
    }

    ~RdDataInfoEntry() { payload.unref(); }

    RdDataInfoEntry(const RdDataInfoEntry& other):is_data_ready(other.is_data_ready), beat_count(other.beat_count), payload(other.payload), phase(other.phase)
    {
        payload.ref();
    }

    RdDataInfoEntry& operator=(const RdDataInfoEntry&) = delete;

    inline void set_data_ready() { is_data_ready = true; }
    inline bool is_receive_data_ready() const { return is_data_ready; }
    inline bool is_send_data_finished() const { return beat_count == 0; }
};

class RdDataInfo
{
public:
    explicit RdDataInfo(const Configure& configure)
        : rd_data_info_buffer_size(configure.controller_config->RD_DAT_INFO_DEPTH)
    {
        for(uint16_t i = 0; i < rd_data_info_buffer_size; ++i){
            unused_buffer_id.insert(i);
        }
    }

private:
    std::set<uint16_t> unused_buffer_id;
    std::unordered_map<uint16_t, RdDataInfoEntry> rdata_info_buffer;
    const unsigned rd_data_info_buffer_size;
public:
    void release_info_tag(uint16_t id){
        unused_buffer_id.insert(id);
    }


    bool IsEmpty() const {
        return rdata_info_buffer.empty();
    }

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

    void allocate_info_buffer_entry(CHIFlit req_flit, uint16_t id)
    {
        rdata_info_buffer.emplace(id, RdDataInfoEntry(req_flit));
    }

    void set_entry_data_ready(uint16_t id)
    {
        rdata_info_buffer.at(id).set_data_ready();
    }

    void erase_entry(uint16_t id)
    {
        rdata_info_buffer.erase(id);
        release_info_tag(id);
    }

    RdDataInfoEntry get_entry(uint16_t id)
    {
        return rdata_info_buffer.at(id);
    }

    bool has_entry_ready() const {
        for(auto& entry: rdata_info_buffer) {
            if(entry.second.is_receive_data_ready()) 
                return true;
        }
        return false;
    }

    std::pair<uint16_t, RdDataInfoEntry> get_ready_entry() const {
        for(auto& entry: rdata_info_buffer) {
            if(entry.second.is_receive_data_ready()) 
                return std::make_pair(entry.first, entry.second);
        }
        std::cerr << "Error: No entry ready in RdDataInfo" << std::endl;
        std::abort();
    }

};

    } // namespace Port
} // namespace dmu

#endif