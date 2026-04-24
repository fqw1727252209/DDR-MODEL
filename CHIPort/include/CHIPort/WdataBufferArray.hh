#ifndef __WDATA_BUFFER_ARRAY_HH__
#define __WDATA_BUFFER_ARRAY_HH__


#include <cstdint>
#include <systemc>
#include <set>
#include <unordered_map>
#include <cassert>
#include <cstring>
#include "ARM/TLM/arm_chi_payload.h"
#include "ARM/TLM/arm_chi_phase.h"
#include "CHIPort/CHIUtilities.h"
#include "Configure/Configure.hh"

namespace dmu{
    namespace Port{

struct WdataBufferEntry
{
    // uint8_t data_bytes[64];
    uint16_t beat_count;
    ARM::CHI::Payload& payload;
    ARM::CHI::Phase phase;

    WdataBufferEntry(const CHIFlit& flit,const unsigned& data_width_bytes): payload(flit.payload), phase(flit.phase) //constructor
    {
        // memset(data_bytes, 255, sizeof(data_bytes));
        const unsigned size_bytes = 1 << flit.payload.size;
        this->beat_count = (size_bytes <= data_width_bytes) ? 1 : size_bytes / data_width_bytes;
        payload.ref();
    }
    ~WdataBufferEntry() {payload.unref();} // destructor

    WdataBufferEntry(const WdataBufferEntry& other):beat_count(other.beat_count), payload(other.payload), phase(other.phase) // copy constructor
    {
        payload.ref();
    }
    WdataBufferEntry& operator=(const WdataBufferEntry&) = delete; // assignment is delete
    inline bool IsEntryReady() const {return beat_count == 0;}
};


class WdataBufferArray
{
public:
    WdataBufferArray(const Configure& configure, const unsigned data_width_bytes);
    ~WdataBufferArray() = default;

    const uint16_t allocate_dbid();

    void allocate_wdata_buffer_entry(const CHIFlit& req_flit,const unsigned& dbid);
    void erase_wdata_buffer_entry(const uint16_t& dbid);

    void release_dbid(uint16_t dbid);
    void receive_wdata_flit(const CHIFlit& dat_flit);

    bool IsArrayFull() const {return buffer_array.size() >= WdataBufferArraySize;}
    const unsigned size() const {return buffer_array.size();}

    bool IsEntryReady(const uint16_t& dbid) const
    {
        return buffer_array.at(dbid).IsEntryReady();
    }
    bool HasEntryReady() const {
        for(auto& buffer_entry : buffer_array)
        {
            if(buffer_entry.second.IsEntryReady())
                return true;
        }
        return false;
    }

private:
    std::set<uint16_t> unallocated_dbid;
    std::unordered_map<uint16_t, WdataBufferEntry> buffer_array;
    std::set<uint16_t> allocated_ptl_dbid;

    const uint8_t WdataBufferArraySize;
    const unsigned data_width_bytes;


};





    } // namespace Port
} // namespace dmu



#endif