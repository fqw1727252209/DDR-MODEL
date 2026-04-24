#include <cassert>
#include <vector>
#include <utility>

#include <tlm>

#include "Controller/CamIF.hh"
#include "Controller/InputProcess.hh"
#include "Controller/Scheduler.hh"
#include "Controller/common/ChildParentExtension.hh"
// #include "Controller/common/ControllerExtension.hh"
#include "Common/UifExtension.hh"
#include "Controller/common/ControllerCommon.hh"
#include "Common/StatisticExtension.hh"

namespace dmu{
    namespace Controller{

InputProcessReq::InputProcessReq(tlm::tlm_generic_payload& trans)
: _request(&trans)
{
    UifExtension* uif_ext = trans.get_extension<UifExtension>();
    cmd_type = uif_ext->_uif_info.cmd_type;
    if(!(cmd_type == CmdType::RMW && trans.get_command() == tlm::TLM_READ_COMMAND))
    {
        _qos = uif_ext->_uif_info.qos;
    }
    else
    {
        _qos = Qos(uif_ext->_uif_info.qos.GetQosValue(),true);
    }
    cmd_id = uif_ext->_uif_info.cmd_id;
    expired_time = uif_ext->_uif_info.expired_time;
    _request->acquire();
}

void
InputProcessReq::SetCmdType()
{
    UifExtension* uif_ext = _request->get_extension<UifExtension>();
    if(_request->get_command() == tlm::TLM_READ_COMMAND)
        cmd_type = CmdType::RD;
    if(_request->get_command() == tlm::TLM_WRITE_COMMAND)
    {
        if(uif_ext->_uif_info.is_rmw)
            cmd_type = CmdType::RMW;
        else
            cmd_type = CmdType::WR;
    }
}

InputProcessReq::InputProcessReq(InputProcessReq&& other)
{
    _request = other._request;
    cmd_type = other.cmd_type;
    cmd_id = other.cmd_id;
    sdram_addr = std::move(other.sdram_addr);
    cam_index = other.cam_index;
    rmw_related_cam_index = other.rmw_related_cam_index;
    _qos = other._qos;
    expired_time = other.expired_time;

    other._request = nullptr;
    other.cmd_type = CmdType::Invalid;
    other.cmd_id = 0;
    other.cam_index = 0;
    other.rmw_related_cam_index = 0;
    other._qos = Qos();
    other.expired_time = sc_core::SC_ZERO_TIME;
}
InputProcessReq&
InputProcessReq::operator=(InputProcessReq&& other)
{
    if(this != &other)
    {
        // 移动资源
        _request = other._request;
        cmd_type = other.cmd_type;
        cmd_id = other.cmd_id;
        sdram_addr = std::move(other.sdram_addr);
        cam_index = other.cam_index;
        rmw_related_cam_index = other.rmw_related_cam_index;
        _qos = other._qos;
        expired_time = other.expired_time;

        // 将源对象的资源设为有效默认状态
        other._request = nullptr;
        other.cmd_type = CmdType::Invalid;
        other.cmd_id = 0;
        other.cam_index = 0;
        other.rmw_related_cam_index = 0;
        other._qos = Qos();
        other.expired_time = sc_core::SC_ZERO_TIME;
    }
    return *this;
}

void
InputProcessReq::print()
{
    std::cout << "[Input Request] :{ " <<"\t"
              <<"allocated cam index: " << this->cam_index <<"\t"
              << this->sdram_addr <<"\t"
              <<"}"
              <<std::endl;
}

InputProcess::InputProcess(const Configure& config, Scheduler& scheduler)
: _config(config)
, _address_decoder(*config.address_decoder)
, _scheduler(scheduler)
{
    std::cout<< "InputProcess Module created" << std::endl;
    for(unsigned i = 0; i < config.controller_config->RD_CAM_DEPTH; ++i)
    {
        unallocated_rd_cam_index_set.insert(i);
    }
    for(unsigned i = 0; i < config.controller_config->WR_CAM_DEPTH; ++i)
    {
        unallocated_wr_cam_index_set.insert(i);
    }
}

void
InputProcess::AcceptRequest(tlm::tlm_generic_payload& trans)
{
    // interface_buffer.emplace_back(&trans);
    // PipProcess();
    // interface_buffer.pop_front();
    if(IsPipBufferEmpty())
    {
        // tlm::tlm_generic_payload trans = *interface_buffer.front();
        tlm::tlm_generic_payload* request = nullptr;
        if(temp_buffer.empty())
        {
            request = &trans; // when there is no valid request in temp buffer, bypass the temp buffer
        }
        else
        {
            // when there is a valid request in temp buffer, interface request store in
            // temp buffer, the temp buffer request goes to pip buffer
            request = temp_buffer.front();
        }

        UifExtension* uif_ext = request->get_extension<UifExtension>();
        InputProcessReq req_trans = InputProcessReq(*request);
        DecodedAddress sdram_address = _address_decoder.decodeAddress(request->get_address());
        request->get_extension<StatisticExtension>()->RecordDecodedAddress(sdram_address);
        req_trans.SetCmdType();
        if(req_trans.cmd_type == CmdType::RD)
        {
            unsigned request_rd_cam_index = AllocateRdCamIndex();
            // DEBUG_PRINT("address decoder do rd cmd address mapping");
            req_trans.cam_index = request_rd_cam_index;
            req_trans.sdram_addr = sdram_address;
            // std::cout << req_trans.sdram_addr <<std::endl;
            rd_pip_buffer.push_back(std::move(req_trans));
        }
        else if(req_trans.cmd_type == CmdType::WR)
        {
            unsigned request_wr_cam_index = AllocateWrCamIndex();
            req_trans.cam_index = request_wr_cam_index;
            req_trans.sdram_addr = sdram_address;
            // std::cout << req_trans.sdram_addr <<std::endl;
            wr_pip_buffer.push_back(std::move(req_trans));
        }
        else if(req_trans.cmd_type == CmdType::RMW)
        {
            SplitTrans(*request);

            auto rmw_rd_childTrans = (request->get_extension<ParentExtension>()->GetChildTrans().at(0));
            tlm::tlm_generic_payload* rmw_wr_childTrans = (request->get_extension<ParentExtension>()->GetChildTrans().at(1));
            InputProcessReq rmw_rd_trans = InputProcessReq(*rmw_rd_childTrans);
            rmw_rd_trans.sdram_addr = sdram_address;
            rmw_rd_trans.cam_index = AllocateRdCamIndex();
            rmw_rd_trans.setCmdType(CmdType::RMW);

            InputProcessReq rmw_wr_trans = InputProcessReq(*rmw_wr_childTrans);
            rmw_wr_trans.sdram_addr = sdram_address;
            rmw_wr_trans.cam_index = AllocateWrCamIndex();
            rmw_wr_trans.setCmdType(CmdType::RMW);

            rmw_rd_trans.rmw_related_cam_index = rmw_wr_trans.cam_index;
            rmw_wr_trans.rmw_related_cam_index = rmw_rd_trans.cam_index;

            rd_pip_buffer.push_back(std::move(rmw_rd_trans));
            wr_pip_buffer.push_back(std::move(rmw_wr_trans));

        }
        else
        {
            std::cerr << "Invalid Transaction Command type, which is neither RD nor WR. "<<std::endl;
            std::exit(1);
        }
    }
    else
    {
        if(temp_buffer.empty())
        {
            temp_buffer.push_back(&trans);
        }
        else
        {
            // this is impossible scenario
            ABORT_MESSAGE("Impossible scenario: Pip buffer and Temp buffer both occupied, but still new trans sent to Controller");
        }
    }
}

void
InputProcess::PipProcess()
{

}

void
InputProcess::SplitTrans(tlm::tlm_generic_payload& trans)
{
    std::vector<tlm::tlm_generic_payload*> childTranses;

    constexpr unsigned numChildTranser = 2;//rmw = read and write

    for(unsigned childId = 0; childId < numChildTranser; childId++)
    {
        tlm::tlm_generic_payload& childTrans = _memory_manager.allocate();
        childTrans.acquire();
        childTrans.set_address(trans.get_address());
        childTrans.set_data_length(trans.get_data_length());
        childTrans.set_data_ptr(trans.get_data_ptr());

        if(childId == 0)
        {
            childTrans.set_command(tlm::TLM_READ_COMMAND);
        }
        else
        {
            childTrans.set_command(tlm::TLM_WRITE_COMMAND);
        }
        ChildExtension::SetExtension(childTrans,trans);
        UifExtension* clone_uif_ext = dynamic_cast<UifExtension*>(trans.get_extension<UifExtension>()->clone());
        assert(!(clone_uif_ext == NULL));
        childTrans.set_extension<UifExtension>(clone_uif_ext);
        childTranses.push_back(&childTrans);
    }

    ParentExtension::SetExtension(trans,std::move(childTranses));

}

void
InputProcess::DetectAddrCollision()
{
    // bool has_collision = false;
    // // wr merge wont stall
    // bool waw = false;
    // bool warmw = false; // 1. rd cam detect 2. rd cam not detect, wr detect
    // // stall
    // bool war = false;
    // bool raw = false;
    // bool rarmw = false; // 1. rd cam detect 2. rd cam not detect, wr detect

    // bool rmwaw = false;
    // bool rmwar = false;
    // bool rmwarmw = false; // 1. rd cam detect 2. rd cam not detect,wr detect
    // // there are 1 + 4 + 1 + 1*2 + 2*2 = 12 condition
    // // 1 not consider rar, 4 + 2*2 will result in stall,and in among 8 condition, 1 + 1 can do write merge, waw and warmw

    //INFO: this is the single port UIF interface, this may change to dual port UIF interface

    DecodedAddress pip_buffer_sdram_addr;
    CmdType _cmd_type_temp{CmdType::Invalid};
    if(!rd_pip_buffer.empty())
    {
        _cmd_type_temp = rd_pip_buffer.front().cmd_type;
        pip_buffer_sdram_addr = rd_pip_buffer.front().sdram_addr;
    }
    else if(!wr_pip_buffer.empty())
    {
        _cmd_type_temp = rd_pip_buffer.front().cmd_type;
        pip_buffer_sdram_addr = rd_pip_buffer.front().sdram_addr;
    }
    else {
        std::cerr<< " Impossible Scenery" << std::endl;
        std::abort();
    }


    // bool rd_cam_collision = false;
    // unsigned collision_rd_cam_index = 0;

    auto rd_cam = _scheduler.GetRdCam();
    rd_cam->ClearRdCollisionCamIndex();

    for(const unsigned& cam_index : _scheduler.GetRdCamIndex())
    {
        RdCamEntry* rd_cam_entry = _scheduler.GetRdCam()->GetCamEntry(cam_index);
        if(rd_cam_entry->sdram_addr == pip_buffer_sdram_addr)
        {
            if(_cmd_type_temp == CmdType::RD && rd_cam_entry->cmd_type == CmdType::RD)
            {
                rd_cam_entry->SetCollision(AddrCollisionType::No_Collision);
                continue;
            }
            // RD-A-RMW (RD) --> RD->RMW(WR)->RMW(RD) --> rd flush, and maskr wr flush
            else if(_cmd_type_temp == CmdType::RD && rd_cam_entry->cmd_type == CmdType::RMW)
            {
                rd_cam_entry->SetCollision(AddrCollisionType::RARMW); // stall rd pip buffer
                rd_cam->AddRdCollisionCamIndex(cam_index);
            }
            // RMW-A-RD --> stall rd pip buffer and wr pip buffer
            else if(_cmd_type_temp == CmdType::RMW && rd_cam_entry->cmd_type == CmdType::RD)
            {
                rd_cam_entry->SetCollision(AddrCollisionType::RMWAR);
                rd_cam->AddRdCollisionCamIndex(cam_index);
            }
            // stall rd pip buffer and wr pip buffer, and set rd flush
            else if(_cmd_type_temp == CmdType::RMW && rd_cam_entry->cmd_type == CmdType::RMW)
            {
                rd_cam_entry->SetCollision(AddrCollisionType::RMWARMW);
                rd_cam->AddRdCollisionCamIndex(cam_index);
            }
            // W-A-R --> stall wr pip buffer, call the rd flush
            else if(_cmd_type_temp == CmdType::WR && rd_cam_entry->cmd_type == CmdType::RD)
            {
                rd_cam_entry->SetCollision(AddrCollisionType::WAR);
                rd_cam->AddRdCollisionCamIndex(cam_index);
            }
            // W-A-RMW(RD) --> W-A-RMW(WR)-RMW(RD) --> if Wr-combine fail, then call the rd flush, and stall wr pip buffer
            else if(_cmd_type_temp == CmdType::WR && rd_cam_entry->cmd_type == CmdType::RMW)
            {
                rd_cam_entry->SetCollision(AddrCollisionType::WARMW);
                rd_cam->AddRdCollisionCamIndex(cam_index);
            }
        }
        else
        {
            rd_cam_entry->SetCollision(AddrCollisionType::No_Collision);
            continue;
        }
    }

    // bool wr_cam_collision = false;
    // unsigned collision_wr_cam_index = 0;
    auto wr_cam = _scheduler.GetWrCam();
    wr_cam->ClearWrCollisionCamIndex();

    for(const unsigned& cam_index : _scheduler.GetWrCamIndex())
    {
        WrCamEntry* wr_cam_entry = _scheduler.GetWrCam()->GetCamEntry(cam_index);
        if(wr_cam_entry->sdram_addr == pip_buffer_sdram_addr)
        {
            // R-A-W --> stall rd pip buffer, and call the wr flush
            if(_cmd_type_temp == CmdType::RD && wr_cam_entry->cmd_type == CmdType::WR)
            {
                wr_cam_entry->SetCollision(AddrCollisionType::RAW);
                wr_cam->AddWrCollisionCamIndex(cam_index);
            }
            // R-A-RMW --> stall rd pip buffer, and call the wr flush, if rd flush exist, mask wr flush
            else if(_cmd_type_temp == CmdType::RD && wr_cam_entry->cmd_type == CmdType::RMW)
            {
                wr_cam_entry->SetCollision(AddrCollisionType::RARMW);
                wr_cam->AddWrCollisionCamIndex(cam_index);
            }
            // RMW-A-WR --> stall wr pip buffer and rd pip buffer, call the wr flush
            else if(_cmd_type_temp == CmdType::RMW && wr_cam_entry->cmd_type == CmdType::WR)
            {
                wr_cam_entry->SetCollision(AddrCollisionType::RMWAW);
                wr_cam->AddWrCollisionCamIndex(cam_index);
            }
            // RMW-A-RMW --> stall wr pip buffer and rd pip buffer (RMW(WR)-RMW(RD)-RMW(WR)-RMW(RD)), call the wr flush( may be mask)
            else if(_cmd_type_temp == CmdType::RMW && wr_cam_entry->cmd_type == CmdType::RMW)
            {
                wr_cam_entry->SetCollision(AddrCollisionType::RMWARMW);
                wr_cam->AddWrCollisionCamIndex(cam_index);
            }
            // WR-A-WR --> can do the write combine, if wr-combine-condition is satisfied, else do the wr flush, stall wr pip buffer
            else if(_cmd_type_temp == CmdType::WR && wr_cam_entry->cmd_type == CmdType::WR)
            {
                if(_scheduler.GetWrCam()->IsWrCamEntryWrCombSatisfied(cam_index))
                {
                    wr_cam_entry->SetCollision(AddrCollisionType::No_Collision);
                    // write_combine_cam_index_vec.push_back(cam_index);
                    wr_cam->AddWrCombineCamIndex(cam_index);
                }
                else {
                    wr_cam_entry->SetCollision(AddrCollisionType::WAW);
                    wr_cam->AddWrCollisionCamIndex(cam_index);
                }
            }
            //W-A-RMW --> can do the write combine, if wr-combine-condition is satisfied, else do the wr flush and rd flush( if exist),
            // and stall the wr pip buffer
            else if(_cmd_type_temp == CmdType::WR && wr_cam_entry->cmd_type == CmdType::RMW)
            {
                if(_scheduler.GetWrCam()->IsWrCamEntryWrCombSatisfied(cam_index))
                {
                    wr_cam_entry->SetCollision(AddrCollisionType::No_Collision);
                    wr_cam->AddWrCombineCamIndex(cam_index);
                }
                else {
                    wr_cam_entry->SetCollision(AddrCollisionType::WARMW);
                    wr_cam->AddWrCollisionCamIndex(cam_index);
                }
            }
        }
        else
        {
            wr_cam_entry->SetCollision(AddrCollisionType::No_Collision);
            continue;
        }
    }
/*
    // if(!rd_cam_collision && !wr_cam_collision)
    // {
    //     this->collision_type = AddrCollisionType::No_Collision;
    //     return AddrCollisionType::No_Collision;
    // }
    // else if(rd_cam_collision && !wr_cam_collision)
    // {
    //     RdCamEntry* collision_rd_cam_entry = _scheduler.GetRdCam()->GetCamEntry(collision_rd_cam_index);
    //     if(_cmd_type_temp == CmdType::RMW && collision_rd_cam_entry->cmd_type == CmdType::RD)
    //     {
    //         rmwar = true;
    //         this->collision_type = AddrCollisionType::RMWAR;
    //         return AddrCollisionType::RMWAR;
    //     }
    //     else if(_cmd_type_temp == CmdType::WR && collision_rd_cam_entry->cmd_type == CmdType::RD)
    //     {
    //         war = true;
    //         this->collision_type = AddrCollisionType::WAR;
    //         return AddrCollisionType::WAR;
    //     }
    // }
    // else if(!rd_cam_collision && wr_cam_collision)
    // {
    //     WrCamEntry* collision_wr_cam_entry = _scheduler.GetWrCam()->GetCamEntry(collision_wr_cam_index);
    //     if(_cmd_type_temp == CmdType::RMW && collision_wr_cam_entry->cmd_type == CmdType::RMW)
    //     {
    //         rmwarmw = true;
    //         this->collision_type = AddrCollisionType::RMWARMW;
    //         return AddrCollisionType::RMWARMW;
    //     }
    //     else if(_cmd_type_temp == CmdType::WR && collision_wr_cam_entry->cmd_type == CmdType::RMW)
    //     {
    //         warmw = true;
    //         this->collision_type = AddrCollisionType::WARMW;
    //         return AddrCollisionType::WARMW;
    //     }
    //     else if(_cmd_type_temp == CmdType::RD && collision_wr_cam_entry->cmd_type == CmdType::RMW)
    //     {
    //         rarmw = true;
    //         this->collision_type = AddrCollisionType::RARMW;
    //         return AddrCollisionType::RARMW;
    //     }
    //     else if(_cmd_type_temp == CmdType::RMW && collision_wr_cam_entry->cmd_type == CmdType::WR)
    //     {
    //         rmwaw = true;
    //         this->collision_type = AddrCollisionType::RMWAW;
    //         return AddrCollisionType::RMWAW;
    //     }
    //     else if(_cmd_type_temp == CmdType::RD && collision_wr_cam_entry->cmd_type == CmdType::WR)
    //     {
    //         raw = true;
    //         this->collision_type = AddrCollisionType::RAW;
    //         return AddrCollisionType::RAW;
    //     }
    //     else if(_cmd_type_temp == CmdType::WR && collision_wr_cam_entry->cmd_type == CmdType::WR)
    //     {
    //         waw = true;
    //         this->collision_type = AddrCollisionType::WAW;
    //         return AddrCollisionType::WAW;
    //     }
    // }
    // else
    // {
    //     RdCamEntry* collision_rd_cam_entry = _scheduler.GetRdCam()->GetCamEntry(collision_rd_cam_index);
    //     // WrCamEntry* collision_wr_cam_entry = _scheduler.GetWrCam()->GetCamEntry(collision_wr_cam_index);
    //     if(_cmd_type_temp == CmdType::RMW && collision_rd_cam_entry->cmd_type == CmdType::RMW)
    //     {
    //         rmwarmw = true;
    //         this->collision_type = AddrCollisionType::RMWARMW;
    //         return AddrCollisionType::RMWARMW;
    //     }
    //     else if(_cmd_type_temp == CmdType::WR && collision_rd_cam_entry->cmd_type == CmdType::RMW)
    //     {
    //         warmw = true;
    //         this->collision_type = AddrCollisionType::WARMW;
    //         return AddrCollisionType::WARMW;
    //     }
    //     else if(_cmd_type_temp == CmdType::RD && collision_rd_cam_entry->cmd_type == CmdType::RMW)
    //     {
    //         rarmw = true;
    //         this->collision_type = AddrCollisionType::RARMW;
    //         return AddrCollisionType::RARMW;
    //     }
    // }
    // // this->collision_type = AddrCollisionType::Invalid;
    // // return AddrCollisionType::Invalid;
**/
}

std::pair<bool, unsigned>
InputProcess::SendCmd2Cq()
{
    bool wr_store = false;
    unsigned wr_cam_index = 0;
    if(!rd_pip_buffer.empty())
    {
        _scheduler.StoreRdRequest(rd_pip_buffer.front());
        rd_pip_buffer.pop_front();
    }
    if(!wr_pip_buffer.empty())
    {
        wr_store = true;
        wr_cam_index = wr_pip_buffer.front().cam_index;
        _scheduler.StoreWrRequest(wr_pip_buffer.front());
        wr_pip_buffer.pop_front();
    }
    return std::make_pair(wr_store, wr_cam_index);
    assert(rd_pip_buffer.empty() && wr_pip_buffer.empty());
}

void
InputProcess::SendRdCmd2RdCam()
{
    _scheduler.StoreRdRequest(rd_pip_buffer.front());
    rd_pip_buffer.pop_front();
}

void
InputProcess::SendWrCmd2WrCam()
{
    _scheduler.StoreWrRequest(wr_pip_buffer.front());
    wr_pip_buffer.pop_front();
}


// void
// InputProcess::RdCamCollisionDetect()
// {

// }

// void
// InputProcess::WrCamCollisionDetect()
// {

// }


// write Merge

    } // Controller
} // dmu