#include <memory>

#include "Controller/CamEntry.hh"
#include "Controller/RdCam.hh"
#include "Controller/InputProcess.hh"
namespace dmu{
    namespace Controller{

// RdCam::RdCam(const unsigned& cam_depth)
// : CamIF(cam_depth)
// {
// }

RdCam::RdCam(const Configure& config)
: CamIF(config.controller_config->RD_CAM_DEPTH)
, _config(config)
{
    lpr_cam_credit = _config.controller_config->LPR_CREDIT;
    hpr_cam_credit = _config.controller_config->HPR_CREDIT;
}

void
RdCam::StoreRequest(InputProcessReq& rd_request)
{
    DPRINT_INFO(RD_CAM, "Rd Cam", "call Rd Cam Store Request");
    // record the used cam index
    used_allocated_cam_index.insert(rd_request.cam_index);
    // add the rd cmd into cam order list
    cam_order_list.push_back(rd_request.cam_index);

    // add the cmd into ba-addr based order list
    if(ba_cmds_order_list.find(rd_request.sdram_addr.real_ba) == ba_cmds_order_list.end())
    {
        ba_cmds_order_list.emplace(rd_request.sdram_addr.real_ba,std::list<CAM_INDEX>());
    }
    ba_cmds_order_list[rd_request.sdram_addr.real_ba].push_back(rd_request.cam_index);

    if(rd_request._qos.GetQosLevel() == PriorityClass::HPR)
    {
        hpr_cmd_set.insert(rd_request.cam_index);
        UpdateHprCamFull();
        UpdateHprFillLevel();
    }
    else if(rd_request._qos.GetQosLevel() == PriorityClass::LPR || rd_request._qos.GetQosLevel() == PriorityClass::GPR)
    {
        lpr_cmd_set.insert(rd_request.cam_index);
        UpdateLprCamFull();
        UpdateLprFillLevel();
    }
    else
    {
        ABORT_MESSAGE("Invalid qos level in rd cam entry store");
    }
    // assert((cam_store.size()>=1) && (cam_store.find(rd_request.cam_index)!= cam_store.end()));
    assert(cam_store.size()<cam_depth);
    cam_store.emplace(rd_request.cam_index,std::make_unique<RdCamEntry>(rd_request));
}

void
RdCam::DeleteCamEntry(CAM_INDEX removed_cam_index)
{
    used_allocated_cam_index.erase(removed_cam_index);
    RdCamEntry* removed_rd_cam_entry = GetCamEntry(removed_cam_index);
    RealBaIndex removed_rd_cam_entry_ba_addr = removed_rd_cam_entry->GetCamEntryRealBa();
    if(removed_rd_cam_entry->GetQosLevel() == PriorityClass::HPR)
    {
        hpr_cmd_set.erase(removed_cam_index);
        UpdateHprCamFull();
        UpdateHprFillLevel();
    }
    else if(removed_rd_cam_entry->GetQosLevel() == PriorityClass::LPR || removed_rd_cam_entry->GetQosLevel() == PriorityClass::GPR)
    {
        lpr_cmd_set.erase(removed_cam_index);
        UpdateLprCamFull();
        UpdateLprFillLevel();
    }
    else
    {
        ABORT_MESSAGE("Invalid qos level in rd cam entry delete");
    }
    if(removed_rd_cam_entry->IsAddrCollision())
    {
        collision_rd_cam_index_vec.erase(std::remove(collision_rd_cam_index_vec.begin(),
                                         collision_rd_cam_index_vec.end(),removed_cam_index),collision_rd_cam_index_vec.end());
    }

    ba_cmds_order_list[removed_rd_cam_entry_ba_addr].remove(removed_cam_index);
    cam_order_list.remove(removed_cam_index);
    cam_store.erase(removed_cam_index);
}


OrderList
RdCam::GetAvailBaOrderList(RealBaIndex ba_addr)
{
    OrderList aval_ba_order_list;
    auto ba_list = this->ba_cmds_order_list.at(ba_addr);
    for(auto cam_index : ba_list)
    {
        if(GetCamEntry(cam_index)->is_allocated)//rd cam only bsc allocated
            aval_ba_order_list.push_back((cam_index));
    }
    return aval_ba_order_list;
}

bool
RdCam::IsAvailBaOrderListEmpty(RealBaIndex ba_addr)
{
    if(IsBaOrderListEmpty(ba_addr))
        return true;
    else
    {
        auto ba_list = this->ba_cmds_order_list.at(ba_addr);
        for(auto cam_index : ba_list)
        {
            if(GetCamEntry(cam_index)->is_allocated)//rd cam only bsc allocated
                return false;
        }
        return true;
    }
}

void
RdCam::UpdateRdCriticalState()
{
    // UpdateHprFillLevel();
    // UpdateLprFillLevel();

    // UpdateHprCamFull();
    // UpdateLprCamFull();

    // Implement with Codex
    // TODO:
    // uif gpr and uif gpw is not critical, starve counter achieve max or hpr is full state
    bool set_hpr_critical = !(false || false) &&  (_config.controller_config->HPR_MAX_STARVE!=0 && (IsHprStarve())) || hpr_cam_full;

    if(set_hpr_critical)
    {
        is_hpr_critical = true;
    }
    else if(_config.controller_config->HPR_FILL_LEVEL_MODE && hpr_full_negedge)
    {
        is_hpr_critical = false;
    }
    else if(!IsHprAvailable() || hpr_run_lenth_cnt == _config.controller_config->HPR_CMD_RUNLEN || _config.controller_config->HPR_MAX_STARVE == 0)
    {
        is_hpr_critical = false;
    }

    bool set_lpr_critical = false || ( (!(set_hpr_critical || false) &&  (_config.controller_config->LPR_MAX_STARVE!=0 && IsLprStarve() )) || lpr_cam_full);
    // Implement with Codex
    // TODO:uif gpr is critical or not hpr cam in critical,not uif gpw in critical, starve counter achieve max or lpr cam is full state
    if(set_lpr_critical)
    {
        is_lpr_critical = true;
    }
    else if(_config.controller_config->LPR_FILL_LEVEL_MODE && lpr_full_negedge)
    {
        is_lpr_critical = false;
    }
    else if(!IsLprAvailable() || lpr_run_lenth_cnt == _config.controller_config->LPR_CMD_RUNLEN || _config.controller_config->LPR_MAX_STARVE == 0)
    {
        is_hpr_critical = false;
    }
}

bool
RdCam::IsRdCamExpired() const
{
    for(auto cam_index: used_allocated_cam_index)
    {
        if(cam_store.at(cam_index)->IsExpired())
            return true;
    }
    return false;
}

    } // Controller
} // dmu