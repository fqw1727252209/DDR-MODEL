#include <memory>
#include <algorithm>

#include "Controller/CamIF.hh"
#include "Controller/WrCam.hh"
#include "Controller/InputProcess.hh"
#include "Common/CommonDefine.hh"

namespace dmu{
    namespace Controller{

void
WrCam::StoreRequest(InputProcessReq& wr_request)
{
    DPRINT_INFO(WR_CAM, "Wr Cam", "call Wr Cam Store Request");
    used_allocated_cam_index.insert(wr_request.cam_index);
    cam_order_list.push_back(wr_request.cam_index);

    // add the cmd into ba-addr based order list
    if(ba_cmds_order_list.find(wr_request.sdram_addr.real_ba) == ba_cmds_order_list.end())
    {
        ba_cmds_order_list.emplace(wr_request.sdram_addr.real_ba,std::list<CAM_INDEX>());
    }
    ba_cmds_order_list[wr_request.sdram_addr.real_ba].push_back(wr_request.cam_index);
    assert(cam_store.size() < cam_depth);
    cam_store.emplace(wr_request.cam_index,std::make_unique<WrCamEntry>(wr_request));
    UpdateTpwCamFull();
    UpdateTpwFillLevel();
}

void
WrCam::DeleteCamEntry(CAM_INDEX removed_cam_index)
{
    used_allocated_cam_index.erase(removed_cam_index);
    WrCamEntry* removed_wr_cam_entry = GetCamEntry(removed_cam_index);
    RealBaIndex removed_wr_cam_entry_ba_addr = removed_wr_cam_entry->GetCamEntryRealBa();

    if(removed_wr_cam_entry->IsAddrCollision())
    {
        collision_wr_cam_index_vec.erase(std::remove(collision_wr_cam_index_vec.begin(),
                                         collision_wr_cam_index_vec.end(),removed_cam_index),collision_wr_cam_index_vec.end());
    }

    ba_cmds_order_list[removed_wr_cam_entry_ba_addr].remove(removed_cam_index);
    cam_order_list.remove(removed_cam_index);
    cam_store.erase(removed_cam_index);
    UpdateTpwCamFull();
    UpdateTpwFillLevel();
}

OrderList
WrCam::GetAvailBaOrderList(RealBaIndex ba_addr)
{
    OrderList aval_ba_order_list;
    auto ba_list = this->ba_cmds_order_list.at(ba_addr);
    for(auto cam_index : ba_list)
    {
        // FIX: Wr cmd should also be data ready
        if(GetCamEntry(cam_index)->is_allocated && GetCamEntry(cam_index)->data_ready)
            aval_ba_order_list.push_back((cam_index));
    }
    return aval_ba_order_list;
}

bool
WrCam::IsAvailBaOrderListEmpty(RealBaIndex ba_addr)
{
    if(IsBaOrderListEmpty(ba_addr))
        return true;
    else
    {
        auto ba_list = this->ba_cmds_order_list.at(ba_addr);
        for(auto cam_index : ba_list)
        {
            // FIX: Wr cmd should also be data ready
            if(GetCamEntry(cam_index)->is_allocated && GetCamEntry(cam_index)->data_ready)
                return false;
        }
        return true;
    }
}

bool
WrCam::IsWrCamExpired() const
{
    return this->is_cam_expired; // 原图中这行被后面覆盖了逻辑
    for(auto cam_index: used_allocated_cam_index)
    {
        if(cam_store.at(cam_index)->IsExpired())
            return true;
    }
    return false;
}

void
WrCam::UpdateWrCriticalState()
{
    // UpdateTpwFillLevel();
    // UpdateTpwCamFull();
    // Implement with Codex
    // TODO:
    // the 1st false is uif_gpw_critical, and the 2ed false is luif_gpr_critical
    // the 3rd true is wr_fill_level_ok: ~csrCqWrCamFillLevelMode || csrCqWrCamHighThr < csrCqWrCamLowThr || wr_cam_gt_highthresh
    bool set_tpw_critical = false || (false && ((true && _config.controller_config->TPW_MAX_STARVE!=0 && (IsTpwStarve())) || tpw_cam_full));

    if(set_tpw_critical)
        is_tpw_critical = true;
    else if(_config.controller_config->TPW_FILL_LEVEL_MODE && tpw_full_negedge)
        is_tpw_critical = false;
    else if(!IsTpwAvailable() || (tpw_run_lenth_cnt == _config.controller_config->TPW_CMD_RUNLEN) || _config.controller_config->TPW_MAX_STARVE==0)
        is_tpw_critical = false;
}

    } // namespace Controller
} // namespace dmu