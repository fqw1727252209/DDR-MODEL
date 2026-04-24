#include "Controller/CamFilter.hh"

namespace dmu{
    namespace Controller{

CAM_INDEX
WrCamFilter::GetSelectedWrCamIndex(const WaitingList& wr_waiting_list,bool IsPageHitLimit)
{
    Candidate_Cmd expired_candidate_cmd;
    Candidate_Cmd expired_hit_candidate_cmd;
    Candidate_Cmd collision_candidate_cmd;
    Candidate_Cmd tpw_hit_candidate_cmd;
    Candidate_Cmd tpw_candidate_cmd;
    for(auto& cam_index: wr_waiting_list)
    {
        WrCamEntry* wr_cam_entry = _wr_cam.GetCamEntry(cam_index);
        if(wr_cam_entry->qos.GetQosLevel() == PriorityClass::Invalid)
        {
            std::cerr << "Invalid Wr cmd Qos Level" <<std::endl;
            std::abort();
        }
        tpw_candidate_cmd.insert(cam_index);
        if(wr_cam_entry->IsExpired())
        {
            if(!IsPageHitLimit && wr_cam_entry->is_page_hit)
                expired_hit_candidate_cmd.insert(cam_index);
            else
                expired_candidate_cmd.insert(cam_index);
        }
        else
        {
            tpw_hit_candidate_cmd.insert(cam_index);
        }
        if(wr_cam_entry->is_addr_collision)
            collision_candidate_cmd.insert(cam_index);
    }

    if(!expired_candidate_cmd.empty())
    {
        if(!expired_hit_candidate_cmd.empty())
            return GetOldestCamIndex(expired_hit_candidate_cmd);
        return GetOldestCamIndex(expired_candidate_cmd);
    }
    if(!collision_candidate_cmd.empty())
    {
        return GetOldestCamIndex(collision_candidate_cmd);
    }
    if(!tpw_hit_candidate_cmd.empty())
    {
        return GetOldestCamIndex(tpw_hit_candidate_cmd);
    }
    if(!tpw_candidate_cmd.empty())
    {
        return GetOldestCamIndex(tpw_candidate_cmd);
    }
    else
    {
        std::cerr << "In Wr Cam Filter, the waiting list is empty, that no impossible" <<std::endl;
        std::abort();
    }
}

CAM_INDEX
WrCamFilter::GetSelectedWrCamIndex(const WaitingList& wr_waiting_list)
{
    return GetSelectedWrCamIndex(wr_waiting_list,true);
}

CAM_INDEX
WrCamFilter::GetOldestCamIndex(const Candidate_Cmd& candidate_cmd)
{
    for(auto& cam_index: _wr_cam.GetOrderList())
    {
        if(candidate_cmd.count(cam_index))
        {
            return cam_index;
        }
    }
    std::cerr << "In Wr Cam Filter, the order list element dont in given candidate_cmd" <<std::endl;
    std::abort();
}


CAM_INDEX
RdCamFilter::GetSelectedRdCamIndex(const WaitingList& rd_waiting_list,bool IsPageHitLimit)
{
    Candidate_Cmd expired_candidate_cmd;
    Candidate_Cmd expired_hit_candidate_cmd;
    Candidate_Cmd collision_candidate_cmd;
    Candidate_Cmd hpr_hit_candidate_cmd;
    Candidate_Cmd lpr_candidate_cmd;
    Candidate_Cmd hpr_candidate_cmd;
    Candidate_Cmd lpr_hit_candidate_cmd;

    for(auto& cam_index: rd_waiting_list)
    {
        RdCamEntry* rd_cam_entry = _rd_cam.GetCamEntry(cam_index);
        if(rd_cam_entry->IsExpired())
        {
            if(!IsPageHitLimit && rd_cam_entry->is_page_hit)
            {
                expired_hit_candidate_cmd.insert(cam_index);
            }
            expired_candidate_cmd.insert(cam_index);
        }
        if(rd_cam_entry->is_addr_collision)
        {
            collision_candidate_cmd.insert(cam_index);
        }

        if(rd_cam_entry->qos.GetQosLevel() == PriorityClass::HPR)
        {
            if(rd_cam_entry->is_page_hit)
            {
                hpr_hit_candidate_cmd.insert(cam_index);
            }
            hpr_candidate_cmd.insert(cam_index);
        }
        if(rd_cam_entry->qos.GetQosLevel() == PriorityClass::LPR || 
           rd_cam_entry->qos.GetQosLevel() == PriorityClass::GPR)
        {
            if(rd_cam_entry->is_page_hit)
            {
                lpr_hit_candidate_cmd.insert(cam_index);
            }
            lpr_candidate_cmd.insert(cam_index);
        }
        if(rd_cam_entry->qos.GetQosLevel() == PriorityClass::Invalid
           || rd_cam_entry->qos.GetQosLevel() == PriorityClass::GPW
           || rd_cam_entry->qos.GetQosLevel() == PriorityClass::TPW)
        {
            std::cerr << "Invalid Qos level get" <<std::endl;
            std::abort();
        }
    }

    if(!expired_candidate_cmd.empty())
    {
        // assert(!expired_candidate_cmd.empty());
        if(!expired_hit_candidate_cmd.empty())
            return GetOldestCamIndex(expired_hit_candidate_cmd);
        return GetOldestCamIndex(expired_candidate_cmd);
    }
    if(!collision_candidate_cmd.empty())
    {
        // assert(!collision_candidate_cmd.empty());
        return GetOldestCamIndex(collision_candidate_cmd);
    }

    if(!is_prefer_hit_than_hpr && !this->IsLprCritical()) // branch 0: hpr-hit > hpr > lpr-hit > lpr
    {
        if(!hpr_hit_candidate_cmd.empty()) // hpr-hit
            return GetOldestCamIndex(hpr_hit_candidate_cmd);
        if(!hpr_candidate_cmd.empty()) // hpr
            return GetOldestCamIndex(hpr_candidate_cmd);
        if(!lpr_hit_candidate_cmd.empty()) // lpr-hit
            return GetOldestCamIndex(lpr_hit_candidate_cmd);
        if(!lpr_candidate_cmd.empty()) // lpr
            return GetOldestCamIndex(lpr_candidate_cmd);
    }
    else if(!is_prefer_hit_than_hpr && this->IsLprCritical()) // branch 1: lpr-hit > lpr > hpr-hit > hpr
    {
        if(!lpr_hit_candidate_cmd.empty()) // lpr-hit
            return GetOldestCamIndex(lpr_hit_candidate_cmd);
        if(!lpr_candidate_cmd.empty()) // lpr
            return GetOldestCamIndex(lpr_candidate_cmd);
        if(!hpr_hit_candidate_cmd.empty()) // hpr-hit
            return GetOldestCamIndex(hpr_hit_candidate_cmd);
        if(!hpr_candidate_cmd.empty()) // hpr
            return GetOldestCamIndex(hpr_candidate_cmd);
    }
    else if(is_prefer_hit_than_hpr && !this->IsLprCritical()) // branch 2: hpr-hit > lpr-hit > hpr > lpr
    {
        if(!hpr_hit_candidate_cmd.empty()) // hpr-hit
            return GetOldestCamIndex(hpr_hit_candidate_cmd);
        if(!lpr_hit_candidate_cmd.empty()) // lpr-hit
            return GetOldestCamIndex(lpr_hit_candidate_cmd);
        if(!hpr_candidate_cmd.empty()) // hpr
            return GetOldestCamIndex(hpr_candidate_cmd);
        if(!lpr_candidate_cmd.empty()) // lpr
            return GetOldestCamIndex(lpr_candidate_cmd);
    }
    else // branch 3: lpr-hit > hpr-hit > lpr > hpr
    {
        if(!lpr_hit_candidate_cmd.empty()) // lpr-hit
            return GetOldestCamIndex(lpr_hit_candidate_cmd);
        if(!hpr_hit_candidate_cmd.empty()) // hpr-hit
            return GetOldestCamIndex(hpr_hit_candidate_cmd);
        if(!lpr_candidate_cmd.empty()) // lpr
            return GetOldestCamIndex(lpr_candidate_cmd);
        if(!hpr_candidate_cmd.empty()) // hpr
            return GetOldestCamIndex(hpr_candidate_cmd);
    }

    std::cerr << "In Rd Cam Filter, no match to find a proper cam " << std::endl;
    // std::exit(1);
    std::abort();
}

CAM_INDEX
RdCamFilter::GetSelectedRdCamIndex(const WaitingList& rd_waiting_list)
{
    return GetSelectedRdCamIndex(rd_waiting_list,true);
}

CAM_INDEX
RdCamFilter::GetOldestCamIndex(const Candidate_Cmd& candidate_cmd)
{
    for(auto& cam_index: _rd_cam.GetOrderList())
    {
        if(candidate_cmd.count(cam_index))
        {
            return cam_index;
        }
    }
    std::cerr << "In Rd Cam Filter, the order list element dont in given candidate_cmd" << std::endl;
    std::abort();
}

    }
}