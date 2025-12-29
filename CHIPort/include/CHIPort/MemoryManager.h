#ifndef MEMORYMANAGER_H
#define MEMORYMANAGER_H

#include <stack>
#include <tlm>
#include <unordered_map>
#include "CHIUtilities.h"
namespace dmu{
    
}
class MemoryManager : public tlm::tlm_mm_interface
{
public:
    explicit MemoryManager(bool storageEnabled);
    MemoryManager(const MemoryManager&) = delete;
    MemoryManager(MemoryManager&&) = delete;
    MemoryManager& operator=(const MemoryManager&) = delete;
    MemoryManager& operator=(MemoryManager&&) = delete;
    ~MemoryManager() override;

    tlm::tlm_generic_payload& allocate(unsigned dataLength);
    void free(tlm::tlm_generic_payload* payload) override;

private:
    uint64_t numberOfAllocations = 0;
    uint64_t numberOfFrees = 0;
    std::unordered_map<unsigned, std::stack<tlm::tlm_generic_payload*>> freePayloads;
    bool storageEnabled = false;
};

#endif // MEMORYMANAGER_H
