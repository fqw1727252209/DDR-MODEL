#ifndef __INPUT_PROCESS_HH__
#define __INPUT_PROCESS_HH__

#include <set>
#include <vector>
#include <deque>
#include <tlm>

#include "Configure/Configure.hh"
#include "Controller/Scheduler.hh"
#include "Configure/AddressDecoder.hh"
#include "Controller/common/MemoryManager.hh"
#include "Controller/common/ControllerCommon.hh"

namespace dmu{
    namespace Controller{

class InputProcessReq{

    public:
        explicit InputProcessReq(tlm::tlm_generic_payload& trans);
        ~InputProcessReq() {
            if(_request != nullptr)
            {
                // 由于在InputPrcoss的AcceptTrans函数中，buffer push back时，调用了移动语义，所以导致最开始生成的InputProcessReq实例，
                // 根据生命周期，原来的实例的_request 被赋值为nullptr,伴随AcceptTrans函数的结束，实例被销毁，所以会出现空指针被释放的情况
                // 需要加一层条件判断
                _request->release();
            }
        }
        InputProcessReq(const InputProcessReq&) = delete;
        InputProcessReq& operator=(const InputProcessReq&) = delete;

        InputProcessReq(InputProcessReq&& other);
        InputProcessReq& operator=(InputProcessReq&& other);

        inline tlm::tlm_generic_payload* GetRequest() { return _request;}
        void SetCmdType();
        inline void setCmdType(CmdType cmd_type_) { cmd_type = cmd_type_;}
    public:
        CmdType cmd_type;
        unsigned cmd_id;
        DecodedAddress sdram_addr;
        // double arrival_time; // this should be in extension

        sc_core::sc_time expired_time;

        unsigned cam_index;
        unsigned rmw_related_cam_index;
        Qos _qos;

        void print();

    private:
        tlm::tlm_generic_payload* _request;
};


class InputProcess
{

    public:
        // InputProcess() {};

        // explicit InputProcess(const AddressDecoder& address_decoder);

        explicit InputProcess(const Configure& config, Scheduler& scheduler);
        explicit InputProcess(const Configure& config, Scheduler& scheduler, unsigned pch_index);

        ~InputProcess() = default;

    public:
        void AcceptRequest(tlm::tlm_generic_payload& trans);
        void PipProcess();
        inline bool IsPipBufferEmpty(){return rd_pip_buffer.empty() && wr_pip_buffer.empty(); }
        inline void ReleaseRdCamIndex(unsigned released_rd_cam_index)
        {
            unallocated_rd_cam_index_set.insert(released_rd_cam_index);
        }
        inline void ReleaseWrCamIndex(unsigned released_wr_cam_index)
        {
            second_unallocated_wr_cam_index_vec.push_back(released_wr_cam_index);
        }
        inline void ReleaseCombWrCamIndex(unsigned combed_wr_cam_index)
        {
            second_unallocated_wr_cam_index_vec.push_back(combed_wr_cam_index);
        }
    private:
        const Configure& _config;
        unsigned pch_index{0};
        // rd
        std::set<unsigned> unallocated_rd_cam_index_set;
        unsigned last_allocated_rd_cam_index;
        inline unsigned AllocateRdCamIndex()
        {
            assert(!unallocated_rd_cam_index_set.empty());
            unsigned allocated_rd_cam_index;
            auto bigger_than_last_granted = unallocated_rd_cam_index_set.lower_bound(last_allocated_rd_cam_index);
            if(bigger_than_last_granted != unallocated_rd_cam_index_set.end())
            {
                allocated_rd_cam_index = *bigger_than_last_granted;
            }
            else
            {
                allocated_rd_cam_index = *unallocated_rd_cam_index_set.begin();
            }
            UpdateLastAllocatedRdCamIndex(allocated_rd_cam_index);
            unallocated_rd_cam_index_set.erase(allocated_rd_cam_index);
            return allocated_rd_cam_index;
        }
        inline void UpdateLastAllocatedRdCamIndex(unsigned allocated_rd_cam_index)
        {
            last_allocated_rd_cam_index = allocated_rd_cam_index;
        }



        // wr
        std::set<unsigned> unallocated_wr_cam_index_set;
        std::deque<unsigned> second_unallocated_wr_cam_index_vec;
        inline unsigned AllocateWrCamIndex()
        {
            unsigned allocated_wr_cam_index;
            if(!unallocated_wr_cam_index_set.empty())
            {
                allocated_wr_cam_index = *unallocated_wr_cam_index_set.begin();
                unallocated_wr_cam_index_set.erase(allocated_wr_cam_index);
            }
            else
            {
                assert(!second_unallocated_wr_cam_index_vec.empty());
                allocated_wr_cam_index = second_unallocated_wr_cam_index_vec.front();
                second_unallocated_wr_cam_index_vec.pop_front();
            }
            return allocated_wr_cam_index;
        }
        //first decide released_wr_cam_index,and then decide combinded_wr_cam_index;

    private:
        std::deque<tlm::tlm_generic_payload*> interface_buffer;
        std::deque<tlm::tlm_generic_payload*> temp_buffer;
        std::deque<InputProcessReq> rd_pip_buffer;
        std::deque<InputProcessReq> wr_pip_buffer;
        bool is_busy;

    private:
        // this function is about to split rmw transaction, or split the long transaction into short transactions.
        void SplitTrans(tlm::tlm_generic_payload& trans);

    private:
        const AddressDecoder& _address_decoder;
        MemoryManager _memory_manager;
    //
    private:
        Scheduler& _scheduler;
        AddrCollisionType collision_type{AddrCollisionType::Invalid};
    public:
        // bool DetectAddrCollision();
        void DetectAddrCollision();
        // void SendCmd2Cq();
        std::pair<bool, unsigned> SendCmd2Cq();
        void SendWrCmd2WrCam();
        void SendRdCmd2RdCam();

        MemoryManager& GetMemoryManager() { return _memory_manager; }


    /*
        1. RMW-request split 这里参考DRAMsys的写法
        2. addr map  and addr collision detect 这里沿用DRAMsys的使用
            1) bank hash
            2) write combined?
        3. CAM index allocation
        4. Back-Pressure ( how to implent this feature, need to considered)
        5. temp-buffer to store pip buffer to drive data to CAM
    */

};

    } // Controller
} // dmu

#endif