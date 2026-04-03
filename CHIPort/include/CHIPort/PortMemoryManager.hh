#ifndef __PORT_MEMORY_MANAGER_HH__
#define __PORT_MEMORY_MANAGER_HH__


#include <cstdint>
#include <stack>
#include <tlm>
#include <unordered_map>

#include "CHIPort/CHIUtilities.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"

namespace dmu{
    namespace Port{

class PortMemoryManager : public tlm::tlm_mm_interface
{
public:
    explicit PortMemoryManager(bool storageEnabled);
    PortMemoryManager(const PortMemoryManager&) = delete;
    PortMemoryManager(PortMemoryManager&&) = delete;
    PortMemoryManager& operator=(const PortMemoryManager&) = delete;
    PortMemoryManager& operator=(const PortMemoryManager&&) = delete;

    ~PortMemoryManager() override;


    tlm::tlm_generic_payload& allocate(const CHIFlit& flit, bool is_rd);
    void free(tlm::tlm_generic_payload* payload) override;
protected:
    tlm::tlm_generic_payload& allocate(unsigned dataLength);
private:
    uint64_t numberOfAllocations = 0;
    uint64_t numberOfFrees = 0;
    std::unordered_map<unsigned, std::stack<tlm::tlm_generic_payload*> > freePayloads;
    bool storageEnabled = false;
    uint64_t transactionId = 0;
};

    } // namespace Port
} // namespace dmu

#endif