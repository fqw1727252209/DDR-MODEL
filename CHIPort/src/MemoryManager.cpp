#include "CHIPort/MemoryManager.h"
#include <iostream>
#include <memory>
#include <new>
namespace dmu{

}
using namespace tlm;

MemoryManager::MemoryManager(bool storageEnabled) : storageEnabled(storageEnabled)
{
}

MemoryManager::~MemoryManager()
{
    for (auto& innerBuffer : freePayloads)
    {
        while (!innerBuffer.second.empty())
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
    
    // Comment in if you are suspecting a memory leak in the manager
    // PRINTDEBUGMESSAGE("MemoryManager","Number of allocated payloads: " +
    // to_string(numberOfAllocations)); PRINTDEBUGMESSAGE("MemoryManager","Number of freed payloads:
    // " + to_string(numberOfFrees));
    std::cout<<"MemoryManager"<<"\tNumber of allocated payloads: "<<numberOfAllocations
             <<"\rMemoryManager"<<"\tNumber of freed payloads: "<<numberOfFrees<<"\r";
}

tlm_generic_payload& MemoryManager::allocate(unsigned dataLength)
{
    if (freePayloads[dataLength].empty())
    {
        numberOfAllocations++;
        auto* payload = new tlm_generic_payload(this);

        if (storageEnabled)
        {
            // Allocate a data buffer and initialize it with zeroes:
            auto* data = new unsigned char[dataLength];
            std::fill(data, data + dataLength, 0);
            payload->set_data_ptr(data);
            
            auto* be = new unsigned char[CHI_CACHE_LINE_SIZE_BYTES];
            std::fill(be, be + CHI_CACHE_LINE_SIZE_BYTES, 0x0);
            payload->set_byte_enable_ptr(be);
        }

        return *payload;
    }

    tlm_generic_payload* result = freePayloads[dataLength].top();
    freePayloads[dataLength].pop();
    return *result;
}

void MemoryManager::free(tlm_generic_payload* payload)
{
    unsigned dataLength = payload->get_data_length();
    freePayloads[dataLength].push(payload);
}
