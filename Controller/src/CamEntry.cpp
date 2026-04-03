#include <cassert>
#include <utility>
#include "Controller/InputProcess.hh"
#include "Controller/CamEntry.hh"

namespace dmu{
    namespace Controller{

CamEntry::CamEntry(InputProcessReq& pip_req)
: sdram_addr(std::move(pip_req.sdram_addr))
, cmd_type(std::move(pip_req.cmd_type))
, allocated_cam_index(pip_req.cam_index)
, qos(pip_req._qos)
, is_rmw(pip_req.cmd_type == CmdType::RMW)
, expired_time(pip_req.expired_time)
, _request(pip_req.GetRequest())
{
    _request->acquire();
}

void
CamEntry::SetAllocateBsc(BSC_INDEX allocated_bsc_index)
{
    is_page_hit = false;
    allocated_bsc_index = allocated_bsc_index;
    is_allocated = true;
}

void
CamEntry::SetReleaseBsc()
{
    is_page_hit = false;
    allocated_bsc_index = 0;
    is_allocated = false;
}

void
CamEntry::SetBaMatch(BSC_INDEX matched_bsc_index, bool _is_page_hit)
{
    is_page_hit = _is_page_hit;
    is_allocated = true;
    allocated_bsc_index = matched_bsc_index;
}

RdCamEntry::RdCamEntry(InputProcessReq& pip_req)
: CamEntry(pip_req)
, rmw_related_wr_cam_index(pip_req.rmw_related_cam_index)
, cmd_id(pip_req.cmd_id)
{
}

WrCamEntry::WrCamEntry(InputProcessReq& pip_req)
: CamEntry(pip_req)
, rmw_related_rd_cam_index(pip_req.rmw_related_cam_index)
, data_ready(false)
{
}

    }
}