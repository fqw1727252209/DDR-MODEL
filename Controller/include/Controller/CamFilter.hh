#ifndef __CAM_FILTER_HH__
#define __CAM_FILTER_HH__

#include <unordered_set>

#include "Controller/WrCam.hh"
#include "Controller/RdCam.hh"

namespace dmu{
    namespace Controller{

class WrCamFilter{
    using Candidate_Cmd = std::unordered_set<CAM_INDEX>;
    public:
        explicit WrCamFilter(WrCam& wr_cam): _wr_cam(wr_cam) {}
        ~WrCamFilter() = default;
        CAM_INDEX GetSelectedWrCamIndex(const WaitingList& wr_waiting_list, bool IsPageHitLimit);
        CAM_INDEX GetSelectedWrCamIndex(const WaitingList& wr_waiting_list);
        CAM_INDEX GetOldestCamIndex(const Candidate_Cmd& candidate_cmd);

    private:
        WrCam& _wr_cam;

};

class RdCamFilter{
    using Candidate_Cmd = std::unordered_set<CAM_INDEX>;
    private:
        RdCam& _rd_cam;
        const bool is_prefer_hit_than_hpr; // reg configure
        // bool is_lpr_critical; // state info, this is taken as a func param
        bool IsLprCritical() {return _rd_cam.IsLprCritical();}
    public:
        explicit RdCamFilter(RdCam& rd_cam, bool _is_prefer_hit_than_hpr)
        : _rd_cam(rd_cam)
        , is_prefer_hit_than_hpr(_is_prefer_hit_than_hpr)
        {}
        ~RdCamFilter() = default;
        CAM_INDEX GetSelectedRdCamIndex(const WaitingList& rd_waiting_list,bool IsPageHitLimit);
        CAM_INDEX GetSelectedRdCamIndex(const WaitingList& rd_waiting_list);
        CAM_INDEX GetOldestCamIndex(const Candidate_Cmd& candidate_cmd);

};

    } // namespace Controller
} // namespace dmu

#endif