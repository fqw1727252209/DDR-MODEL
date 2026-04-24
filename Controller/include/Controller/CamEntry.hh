#ifndef __CONTROLLER_CAM_HH__
#define __CONTROLLER_CAM_HH__

#include <cassert>
#include <cstddef>
#include <iomanip>
#include <utility>
#include <string>
#include <iostream>

#include <tlm>

#include "Controller/common/ControllerCommon.hh"
#include "Configure/AddressDecoder.hh"

namespace dmu{
    namespace Controller{

    class InputProcessReq;

    using BSC_INDEX = unsigned;
    using RealBaIndex = uint64_t;
    class CamEntry
    {
        public:
            //From the input process, and wont be changed
            const DecodedAddress sdram_addr;
            const CmdType cmd_type;
            const Qos qos;
            const bool is_rmw;
            const unsigned allocated_cam_index;

            //
            // Implement with Codex
            sc_core::sc_time expired_time; // TODO:this will also not be changed
            // Implement with Codex
            unsigned cmd_aging_limit{100}; // TODO:may not be configured here, can store in the cam

            unsigned ahead_scheduled_cmd_num{0};
            bool is_expired{false}; //
            //
            bool is_addr_collision{false}; // changed value
            AddrCollisionType collision_type{AddrCollisionType::Invalid};
            //
            bool is_allocated{false};
            unsigned allocated_bsc_index{0};
            bool is_page_hit{false};

        private:
            tlm::tlm_generic_payload* _request;
        public:
            inline tlm::tlm_generic_payload* GetRequest() const { return _request; }
        public:
            explicit CamEntry(InputProcessReq& pip_req);
            virtual ~CamEntry()
            {
                _request->release();
            }
            void SetReleaseBsc();
            void SetAllocateBsc(BSC_INDEX allocated_bsc_index);
            void SetBaMatch(BSC_INDEX matched_bsc_index, bool _is_page_hit);

            inline RealBaIndex GetCamEntryRealBa() const {return sdram_addr.real_ba;}
            inline bool IsPageHit() const {return is_page_hit;}
            inline void SetPageOpen() { is_page_hit = true; }
            inline void SetPageClose() { is_page_hit = false; }
            inline bool IsAllocated() const {return is_allocated;}

            inline void SetCollision() {is_addr_collision = true;}
            inline void SetCollision(AddrCollisionType _addr_collision_type)
            {
                collision_type = _addr_collision_type;
                is_addr_collision = (_addr_collision_type != AddrCollisionType::No_Collision && _addr_collision_type != AddrCollisionType::Invalid);
            }
            inline bool IsAddrCollision() const {return is_addr_collision;}
            inline PriorityClass GetQosLevel() const {return qos.GetQosLevel();}
            inline unsigned GetAheadCmdNum() const {return ahead_scheduled_cmd_num;}
            inline void SetCmdAgingLimit(unsigned aging_threshold){cmd_aging_limit = aging_threshold;}

            inline bool IsExpired() const
            {
                return (expired_time >= sc_core::sc_time_stamp() &&
                       (qos.GetQosLevel() == PriorityClass::GPR || qos.GetQosLevel() == PriorityClass::GPW))
                       || ahead_scheduled_cmd_num >= cmd_aging_limit;
            }
            void print(){
                std::cout << "[Cam Entry]: { "
                          <<"cam index: "<<std::setw(3) << this->allocated_cam_index << "\t"
                          <<"addr_collision: "<< (is_addr_collision ? "true" : "false") << "\t"
                          <<"collision type: "<< AddrCollisionTypeStr[static_cast<size_t>(collision_type)] << "\t"
                          <<"time expired: "<< (is_expired ? "true" : "false") << "\t"
                          <<"bsc allocated: "<< (is_allocated ? "true" : "false") << "\t"
                          <<"allocated bsc index: "<< allocated_bsc_index << "\t"
                          <<"page hit: " << (is_page_hit ? "true" : "false") << "\t"
                          << sdram_addr << "\t"
                          <<"}"<<std::endl;
            }

    };

    class RdCamEntry: public CamEntry
    {
        public:
            unsigned cmd_id;
            unsigned rmw_related_wr_cam_index;
        private:
            const tlm::tlm_generic_payload* _request;

        public:
            explicit RdCamEntry(InputProcessReq& pip_req);
            // inline const tlm::tlm_generic_payload* GetRequest() const { return _request;}

            ~RdCamEntry() {}
            RdCamEntry(const RdCamEntry&) = delete;
            RdCamEntry& operator=(const RdCamEntry&) = delete;

    };
    class WrCamEntry : public CamEntry
    {
        public:
            unsigned rmw_related_rd_cam_index;
            bool data_ready;

        private:
            const tlm::tlm_generic_payload* _request;

        public:
            explicit WrCamEntry(InputProcessReq& pip_req);
            // inline const tlm::tlm_generic_payload* GetRequest() const { return _request;}
            ~WrCamEntry() {}
            WrCamEntry(const WrCamEntry&) = delete;
            WrCamEntry& operator=(const WrCamEntry&) = delete;

    };



    } // namespace Controller
} // namespace dmu


#endif