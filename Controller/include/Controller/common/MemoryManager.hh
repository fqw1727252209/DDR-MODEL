#ifndef __MEMORY_MANAGER_HH__
#define __MEMORY_MANAGER_HH__

#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include <tlm>
#include <stack>
namespace dmu{
    namespace Controller{


class MemoryManager : public tlm::tlm_mm_interface
{
    public:
        // MemoryManager() = default;
        MemoryManager()
        {
            dummy_payload = new tlm::tlm_generic_payload(this);
            dummy_payload->set_address(0);
            dummy_payload->set_data_ptr(nullptr);
            dummy_payload->set_data_length(0);
            dummy_payload->set_streaming_width(0);
            dummy_payload->set_response_status(tlm::TLM_OK_RESPONSE);
            dummy_payload->set_command(tlm::TLM_IGNORE_COMMAND);
        }

        MemoryManager(const MemoryManager&) = delete;
        MemoryManager(MemoryManager&&) = delete;
        MemoryManager& operator=(const MemoryManager&) = delete;
        MemoryManager& operator=(MemoryManager&&) = delete;
        ~MemoryManager() override
        {
            while(!freePayloads.empty())
            {
                tlm::tlm_generic_payload* trans = freePayloads.top();
                freePayloads.pop();
                trans->reset();
                delete trans;
            }
            delete dummy_payload;
        }

        tlm::tlm_generic_payload& allocate()
        {
            if(freePayloads.empty())
            {
                return * new tlm::tlm_generic_payload(this);
            }

            tlm::tlm_generic_payload* result = freePayloads.top();
            freePayloads.pop();
            return *result;
        }

        void free(tlm::tlm_generic_payload* trans) override
        {
            freePayloads.push(trans);
        }

        tlm::tlm_generic_payload* getDummyPayload()
        {
            return dummy_payload;
        }

    private:
        std::stack<tlm::tlm_generic_payload*> freePayloads;
        tlm::tlm_generic_payload* dummy_payload;

};

    } // Controller
} // dmu








#endif