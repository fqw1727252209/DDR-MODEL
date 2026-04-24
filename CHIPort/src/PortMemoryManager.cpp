#include "CHIPort/PortMemoryManager.hh"
#include "Common/StatisticExtension.hh"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <iostream>
#include <memory>
#include <new>

namespace dmu{
    namespace Port{
using namespace tlm;

PortMemoryManager::PortMemoryManager(bool storageEnabled) : storageEnabled(storageEnabled)
{
}

PortMemoryManager::~PortMemoryManager()
{
    for(auto& innerBuffer: freePayloads)
    {
        while( !innerBuffer.second.empty())
        {
            tlm_generic_payload* payload = innerBuffer.second.top();
            if (storageEnabled){
                delete[] payload->get_data_ptr();
                delete[] payload->get_byte_enable_ptr();
            }

            payload->reset();
            delete payload;
            innerBuffer.second.pop();
            numberOfFrees++;
        }
    }
}

tlm_generic_payload& PortMemoryManager::allocate(unsigned dataLength)
{
    if(freePayloads[dataLength].empty())
    {
        numberOfAllocations++;
        auto* payload = new tlm_generic_payload(this);

        if(storageEnabled)
        {
            //
            auto* data = new unsigned char[dataLength];
            std::fill(data,data + dataLength, 0);
            payload->set_data_ptr(data);
            auto* be = new unsigned char[CHI_CACHE_LINE_SIZE_BYTES];
            std::fill(be,be + CHI_CACHE_LINE_SIZE_BYTES, 0x0);
            payload->set_byte_enable_ptr(be);
        }
        StatisticExtension* statistic_ext = new StatisticExtension();
        payload->set_extension(statistic_ext);
        return *payload;
    }

    tlm_generic_payload* result = freePayloads[dataLength].top();
    freePayloads[dataLength].pop();
    return *result;
}

tlm::tlm_generic_payload& PortMemoryManager::allocate(const CHIFlit& flit, bool is_rd)
{
    const unsigned dataLength = 1 << flit.payload.size;
    tlm_generic_payload* payload = reinterpret_cast<tlm::tlm_generic_payload*>(& (this->allocate(dataLength)));
    payload->set_address(flit.payload.address);
    payload->set_command(is_rd ? tlm::TLM_READ_COMMAND: tlm::TLM_WRITE_COMMAND);
    payload->get_extension<StatisticExtension>()->RecordTransactionId(transactionId);
    transactionId++;
    return *payload;
}

void PortMemoryManager::free(tlm::tlm_generic_payload* payload)
{
    unsigned dataLength = payload->get_data_length();
    freePayloads[dataLength].push(payload);
}



    } // namespace Port
} // namespace dmu