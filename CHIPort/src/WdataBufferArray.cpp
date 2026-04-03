#include "CHIPort/WdataBufferArray.hh"
#include "Common/CommonDefine.hh"
#include <cassert>

namespace dmu{
    namespace Port{


WdataBufferArray::WdataBufferArray(const Configure& configure, const unsigned data_width_bytes)
: WdataBufferArraySize(configure.controller_config->WR_DAT_BUFFER_DEPTH)
, data_width_bytes(data_width_bytes)
{
    for(uint8_t i = 0; i < WdataBufferArraySize; i++)
    {
        unallocated_dbid.insert(i);
    }
}

const uint16_t
WdataBufferArray::allocate_dbid()
{
    uint16_t dbid = *unallocated_dbid.begin();
    unallocated_dbid.erase(unallocated_dbid.begin());
    return dbid;
}

void
WdataBufferArray::allocate_wdata_buffer_entry(const CHIFlit& req_flit,const unsigned& dbid)
{
    buffer_array.emplace(dbid,WdataBufferEntry(req_flit, this->data_width_bytes));
}

void
WdataBufferArray::erase_wdata_buffer_entry(const uint16_t& dbid)
{
    buffer_array.erase(dbid);
}

void
WdataBufferArray::release_dbid(uint16_t dbid)
{
    unallocated_dbid.insert(dbid);
}

void
WdataBufferArray::receive_wdata_flit(const CHIFlit& dat_flit)
{
    uint16_t& beat_count_remaning = buffer_array.at(dat_flit.phase.txn_id).beat_count;
    // DPRINT_ASSERT(beat_count_remaning > 0, "WdataBufferArray", "dbid:%d ,beat_count_remaning should be greater than 0",dat_flit.phase.txn_id);
    beat_count_remaning--;
    // if(beat_count_remaning == 0)
    // {
    //     memcpy(buffer_array.at(dat_flit.phase.txn_id).data_bytes, dat_flit.payload.data, sizeof(dat_flit.payload.data));
    // }
}

    } // namespace Port
} // namespace dmu