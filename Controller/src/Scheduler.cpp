#include <cassert>
#include <iostream>
#include <memory>

#include "Controller/Scheduler.hh"
#include "Controller/RdCam.hh"
#include "Controller/WrCam.hh"
#include "Controller/CamFilter.hh"
#include "Controller/InputProcess.hh"
#include "Controller/BankSlice.hh"
#include "Common/CommonDefine.hh"
#include "Common/StatisticExtension.hh"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/utils/sc_report.h"

namespace dmu{
    namespace Controller{

Scheduler::Scheduler(const Configure& config)
: mc_cycle_time(config.mem_spec->tCK_mc)
, ntt_store(Ntt(config.controller_config->BSC_NUM))
{
    rd_cam = std::make_unique<RdCam>(config);
    wr_cam = std::make_unique<WrCam>(config);
    rd_cam_filter = std::make_unique<RdCamFilter>(*rd_cam.get(),config.controller_config->PREFER_HIT_HPR);
    wr_cam_filter = std::make_unique<WrCamFilter>(*wr_cam.get());
    // wr_update_ntt_temp = std::vector<std::vector<std::deque<BSC_INDEX>>>(config.controller_config->BSC_NUM,
    //                        std::vector<std::deque<BSC_INDEX>>(static_cast<size_t>(UpdateType::Invalid)));
    // rd_update_ntt_temp = std::vector<std::vector<std::deque<BSC_INDEX>>>(config.controller_config->BSC_NUM,
    //                        std::vector<std::deque<BSC_INDEX>>(static_cast<size_t>(UpdateType::Invalid)));
    // ntt_store = Ntt(config.controller_config->BSC_NUM);
}

Scheduler::~Scheduler()
{
}

void
Scheduler::StoreRdRequest(InputProcessReq& rd_input_request)
{
    RealBaIndex request_ba = rd_input_request.sdram_addr.real_ba;
    rd_input_request.print();
    rd_cam->StoreRequest(rd_input_request);
    rd_input_request.GetRequest()->get_extension<StatisticExtension>()->RecordInCamTime(sc_core::sc_time_stamp());
    if(IsBscMatch(request_ba))
    {
        CAM_INDEX request_cam_index = rd_input_request.cam_index;
        auto bank_slice = bsc_index_2_bankslice->at(ba2bsc_table->at(request_ba)).get();
        bool is_page_hit = bank_slice->IsPageOpen() && (bank_slice->GetOpenPage() == rd_input_request.sdram_addr.row);
        rd_cam->GetCamEntry(request_cam_index)->SetBaMatch(ba2bsc_table->at(request_ba), is_page_hit);
        if(is_page_hit || (!is_page_hit && (!bank_slice->IsActiving() || bank_slice->IsRdNttValid())))
        {
            UpdateRdNttPip(ba2bsc_table->at(request_ba),request_ba,UpdateType::NewCmdStore);
        }
        else
        {
            DPRINT_WARNING(false, "Scheduler Rd Store", "the new command page hit: %d, the bank is in Activing: %d, activing end time: %s, the ntt is valid: %d",
            is_page_hit,bank_slice->IsActiving(),bank_slice->GetACTEndTime().to_string().c_str(),bank_slice->IsRdNttValid());
        }
    }
    rd_cam->GetCamEntry(rd_input_request.cam_index)->print();
}

void
Scheduler::StoreWrRequest(InputProcessReq& wr_input_request)
{
    RealBaIndex request_ba = wr_input_request.sdram_addr.real_ba;
    wr_cam->StoreRequest(wr_input_request);
    wr_input_request.GetRequest()->get_extension<StatisticExtension>()->RecordInCamTime(sc_core::sc_time_stamp());
    if(IsBscMatch(request_ba))
    {
        CAM_INDEX request_cam_index = wr_input_request.cam_index;
        auto bank_slice = bsc_index_2_bankslice->at(ba2bsc_table->at(request_ba)).get();
        bool is_page_hit = bank_slice->IsPageOpen() && (bank_slice->GetOpenPage() == wr_input_request.sdram_addr.row);
        wr_cam->GetCamEntry(request_cam_index)->SetBaMatch(ba2bsc_table->at(request_ba), is_page_hit);
        // if(is_page_hit || (!is_page_hit && (!bank_slice->IsActiving() || bank_slice->IsWrNttValid())))
        // {
        //     UpdateWrNttPip(ba2bsc_table->at(request_ba),request_ba,UpdateType::NewCmdStore);
        // }
    }
}

void
Scheduler::UpdateNttPip(BSC_INDEX bsc_index, RealBaIndex ba_addr, UpdateType update_type, bool is_rd)
{
    if(is_rd)
    {
        UpdateRdNttPip(bsc_index,ba_addr, update_type);
        return;
    }
    else
    {
        UpdateWrNttPip(bsc_index,ba_addr, update_type);
        return;
    }
}

void
Scheduler::UpdateRdNttPip(BSC_INDEX bsc_index, RealBaIndex ba_addr, UpdateType update_type)
{
    DPRINT_INFO(NTT, "Rd Ntt Update: ", "updated bsc: %d, Type: %s", bsc_index, UpdateTypeStr[static_cast<size_t>(update_type)].c_str());

    if(update_type == UpdateType::NewCmdStore && !rd_cam->IsBaOrderListEmpty(ba_addr)&& rd_cam->IsBaListAvail(ba_addr))
    {
        auto updated_cam_index = rd_cam_filter->GetSelectedRdCamIndex(rd_cam->GetBaOrderList(ba_addr));

        DPRINT_ASSERT(rd_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba == ba_addr,"Rd Ntt Update:",
        "ba_addr mismatch, the read updated cam index ba is %d, but the bsc ba_addr is %ld",(rd_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba),ba_addr);

        // rd_updated_bsc_set.insert(bsc_index);
        // rd_update_ntt_temp.at(bsc_index).at(static_cast<size_t>(update_type)).push_back(
        //     rd_cam_filter->GetSelectedRdCamIndex(rd_cam->GetBaOrderList(ba_addr))
        // );
        sc_core::sc_time updating_time = sc_core::sc_time_stamp() + mc_cycle_time;
        ntt_store.RecordNttsBsc(true,bsc_index,updating_time);
        ntt_store.RdNttStore(bsc_index,update_type,updated_cam_index,
        updating_time);
    }
    else if(update_type == UpdateType::CmdExe && !rd_cam->IsBaOrderListEmpty(ba_addr) && rd_cam->IsBaListAvail(ba_addr))
    {
        auto updated_cam_index = rd_cam_filter->GetSelectedRdCamIndex(rd_cam->GetBaOrderList(ba_addr));

        DPRINT_ASSERT(rd_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba == ba_addr,"Rd Ntt Update:",
        "ba_addr mismatch, the read updated cam index ba is %d, but the bsc ba_addr is %ld",(rd_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba),ba_addr);

        sc_core::sc_time updating_time = sc_core::sc_time_stamp() + mc_cycle_time;
        ntt_store.RecordNttsBsc(true,bsc_index,updating_time);
        ntt_store.RdNttStore(bsc_index,update_type,updated_cam_index,
        updating_time);

    }
    else if(update_type == UpdateType::Pre_Act && !rd_cam->IsBaOrderListEmpty(ba_addr)&& rd_cam->IsBaListAvail(ba_addr))
    {
        auto updated_cam_index = rd_cam_filter->GetSelectedRdCamIndex(rd_cam->GetBaOrderList(ba_addr));

        DPRINT_ASSERT(rd_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba == ba_addr,"Rd Ntt Update:",
        "ba_addr mismatch, the read updated cam index ba is %d, but the bsc ba_addr is %ld",(rd_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba),ba_addr);
        // rd_updated_bsc_set.insert(bsc_index);
        // rd_update_ntt_temp.at(bsc_index).at(static_cast<size_t>(update_type)).push_back(
        //     rd_cam_filter->GetSelectedRdCamIndex(rd_cam->GetBaOrderList(ba_addr))
        // );
        sc_core::sc_time updating_time = sc_core::sc_time_stamp() + mc_cycle_time;
        ntt_store.RecordNttsBsc(true,bsc_index,updating_time);
        ntt_store.RdNttStore(bsc_index,update_type,updated_cam_index,
        updating_time);
    }
    else if(update_type == UpdateType::BscAllocate && !rd_cam->IsBaOrderListEmpty(ba_addr) && rd_cam->IsBaListAvail(ba_addr))
    {
        auto updated_cam_index = rd_cam_filter->GetSelectedRdCamIndex(rd_cam->GetBaOrderList(ba_addr));

        DPRINT_ASSERT(rd_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba == ba_addr,"Rd Ntt Update:",
        "ba_addr mismatch, the read updated cam index ba is %d, but the bsc ba_addr is %ld",(rd_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba),ba_addr);
        sc_core::sc_time updating_time = sc_core::sc_time_stamp() + mc_cycle_time;
        ntt_store.RecordNttsBsc(true,bsc_index,updating_time);
        ntt_store.RdNttStore(bsc_index,update_type,rd_cam_filter->GetSelectedRdCamIndex(rd_cam->GetBaOrderList(ba_addr)),
        updating_time);
    }
    else if(update_type == UpdateType::BrokenTerminate && !rd_cam->IsBaOrderListEmpty(ba_addr)&& rd_cam->IsBaListAvail(ba_addr))
    {
        ;
    }
    else
    {
        ;
    }
}

void
Scheduler::UpdateRdNttPip(RealBaIndex ba_addr, UpdateType update_type)
{
    BSC_INDEX bsc_index = ba2bsc_table->at(ba_addr);
    UpdateRdNttPip(bsc_index,ba_addr,update_type);
}

void
Scheduler::UpdateWrNttPip(BSC_INDEX bsc_index, RealBaIndex ba_addr, UpdateType update_type)
{
    DPRINT_INFO(NTT, "Wr Ntt Update: ", "updated bsc: %d, Ntt Type: %s", bsc_index, UpdateTypeStr[static_cast<size_t>(update_type)].c_str());

    if(update_type == UpdateType::NewCmdStore && !wr_cam->IsBaOrderListEmpty(ba_addr) && wr_cam->IsBaListAvail(ba_addr))
    {
        auto updated_cam_index = wr_cam_filter->GetSelectedWrCamIndex(wr_cam->GetBaOrderList(ba_addr));

        DPRINT_ASSERT(wr_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba == ba_addr,"Wr Ntt Update:",
        "ba_addr mismatch, the write updated cam index ba is %d, but the bsc ba_addr is %ld",(wr_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba),ba_addr);
        sc_core::sc_time updating_time = sc_core::sc_time_stamp() + mc_cycle_time;
        ntt_store.RecordNttsBsc(false,bsc_index,updating_time);
        ntt_store.WrNttStore(bsc_index,update_type,updated_cam_index,
        updating_time);
    }
    else if(update_type == UpdateType::CmdExe && !wr_cam->IsBaOrderListEmpty(ba_addr) && wr_cam->IsBaListAvail(ba_addr))
    {
        auto updated_cam_index = wr_cam_filter->GetSelectedWrCamIndex(wr_cam->GetBaOrderList(ba_addr));

        DPRINT_ASSERT(wr_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba == ba_addr,"Wr Ntt Update:",
        "ba_addr mismatch, the write updated cam index ba is %d, but the bsc ba_addr is %ld",(wr_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba),ba_addr);

        sc_core::sc_time updating_time = sc_core::sc_time_stamp() + mc_cycle_time;
        ntt_store.RecordNttsBsc(false,bsc_index,updating_time);
        ntt_store.WrNttStore(bsc_index,update_type,updated_cam_index,
        updating_time);

    }
    else if(update_type == UpdateType::Pre_Act && !wr_cam->IsBaOrderListEmpty(ba_addr) && wr_cam->IsBaListAvail(ba_addr))
    {
        auto updated_cam_index = wr_cam_filter->GetSelectedWrCamIndex(wr_cam->GetBaOrderList(ba_addr));

        DPRINT_ASSERT(wr_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba == ba_addr,"Wr Ntt Update:",
        "ba_addr mismatch, the write updated cam index ba is %d, but the bsc ba_addr is %ld",(wr_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba),ba_addr);

        sc_core::sc_time updating_time = sc_core::sc_time_stamp() + mc_cycle_time;
        ntt_store.RecordNttsBsc(false,bsc_index,updating_time);
        ntt_store.WrNttStore(bsc_index,update_type,updated_cam_index,
        updating_time);

    }
    else if(update_type == UpdateType::WrCombine && !wr_cam->IsBaOrderListEmpty(ba_addr) && wr_cam->IsBaListAvail(ba_addr))
    {
        ;
    }
    else if(update_type == UpdateType::BscAllocate && !wr_cam->IsBaOrderListEmpty(ba_addr) && wr_cam->IsBaListAvail(ba_addr))
    {
        auto updated_cam_index = wr_cam_filter->GetSelectedWrCamIndex(wr_cam->GetBaOrderList(ba_addr));

        DPRINT_ASSERT(wr_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba == ba_addr,"Wr Ntt Update:",
        "ba_addr mismatch, the write updated cam index ba is %d, but the bsc ba_addr is %ld",(wr_cam->GetCamEntry(updated_cam_index)->sdram_addr.real_ba),ba_addr);

        sc_core::sc_time updating_time = sc_core::sc_time_stamp() + mc_cycle_time;
        ntt_store.RecordNttsBsc(false,bsc_index,updating_time);
        ntt_store.WrNttStore(bsc_index,update_type,updated_cam_index,
        updating_time);
    }
    else if(update_type == UpdateType::BrokenTerminate && !wr_cam->IsBaOrderListEmpty(ba_addr) && wr_cam->IsBaListAvail(ba_addr))
    {
        ;
    }
    else
    {
        ;
    }
    DPRINT_INFO(false, "NTT", "Write Ntt Update End");
}

void
Scheduler::UpdateWrNttPip(RealBaIndex ba_addr, UpdateType update_type)
{
    BSC_INDEX bsc_index = ba2bsc_table->at(ba_addr);
    UpdateWrNttPip(bsc_index,ba_addr,update_type);
}

const CAM_INDEX
Scheduler::GetUpdateNttTemp(BSC_INDEX bsc_index, bool is_rd)
{
    if(is_rd)
    {
        return GetUpdateRdNttTemp(bsc_index);
    }
    else
    {
        return GetUpdateWrNttTemp(bsc_index);
    }
}

const CAM_INDEX
Scheduler::GetUpdateRdNttTemp(BSC_INDEX bsc_index)
{
    return ntt_store.GetRdNtt(bsc_index);
}

const CAM_INDEX
Scheduler::GetUpdateWrNttTemp(BSC_INDEX bsc_index)
{
    return ntt_store.GetWrNtt(bsc_index);
}
}// Controller
} // dmu