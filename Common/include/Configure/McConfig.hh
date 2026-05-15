#ifndef __MC_CONFIG_HH__
#define __MC_CONFIG_HH__

#include "Configure/LoadControllerConfig.hh"

namespace dmu{
    namespace Controller{

class McConfig{
public:
    const unsigned RD_QOS_LEVEL_0;
    const unsigned RD_QOS_LEVEL_1;
    const unsigned WR_QOS_LEVEL;

    const unsigned GPR_EXPIRED_TIME; // n DFI Cycle
    const unsigned GPW_EXPIRED_TIME; // n DFI Cycle

    const double PHY_CMD_DELAY;
    const double PHY_RDAT_DELAY;
    const double PHY_WDAT_DELAY;

    //Scheduler Config
    const unsigned RD_CAM_DEPTH;
    const unsigned WR_CAM_DEPTH;
    const unsigned WDAT_BUFFER_DEPTH;
    const unsigned RDAT_BUFFER_DEPTH;
    const unsigned BSC_NUM;

    const bool AUTO_PRECHARGE_ENABLE;
    const bool NCWEN;

    const bool BANK_HASH_ENABLE;

    const unsigned BSC_USED_HIGH_THRESHOLD;
    const unsigned BSC_USED_LOW_THRESHOLD;
    const bool DYNAMIC_BSC_ENABLE;
    const bool DYNAMIC_BSC_BUSY_RELEASE_ENABLE;
    const bool BSC_DEEP_RELEASE_ENABLE;
    const bool DYNAMIC_BSC_BUSY_MASK_ENABLE;
    const bool BSC_TERMINATE_BROKEN_ENABLE;
    const bool BSC_TERMINATE_ENABLE;

    const unsigned HPR_CREDIT;
    const unsigned LPR_CREDIT;
    const unsigned TPW_CREDIT;

    const bool HPR_FILL_LEVEL_MODE;
    const bool LPR_FILL_LEVEL_MODE;
    const bool TPW_FILL_LEVEL_MODE;
    const bool HPR_STARVE_COUNT_MODE;
    const bool LPR_STARVE_COUNT_MODE;
    const bool TPW_STARVE_COUNT_MODE;

    const unsigned HPR_HIGH_THRESHOLD;
    const unsigned HPR_LOW_THRESHOLD;
    const unsigned LPR_HIGH_THRESHOLD;
    const unsigned LPR_LOW_THRESHOLD;
    const unsigned TPW_HIGH_THRESHOLD;
    const unsigned TPW_LOW_THRESHOLD;

    const bool WR_COMBINE_ENABLE;

    const unsigned HPR_MAX_STARVE;
    const unsigned LPR_MAX_STARVE;
    const unsigned TPW_MAX_STARVE;
    const unsigned HPR_CMD_RUNLEN;
    const unsigned LPR_CMD_RUNLEN;
    const unsigned TPW_CMD_RUNLEN;

    const unsigned Max_RW_BEFORE_SWITCH;
    const unsigned OPP_HIT_CNT;
    const bool MODE_SWITCH_POLICY;

    const unsigned WR_HIT_LIMIT_THRESHOLD;
    const unsigned RD_HIT_LIMIT_THRESHOLD;

    const unsigned RD_CMD_AGING_LIMIT;
    const unsigned WE_CMD_AGING_LIMIT;

    const bool WR_OPPOSITE_ACT_ENABLE;
    const bool RD_OPPOSITE_ACT_ENABLE;

    const bool PREFER_HIT_HPR;

// mrdimm
        const bool RD_CNT_THR_EN;
        const unsigned RD_CNT_THR;
        const bool WR_CNT_THR_EN;
        const unsigned WR_CNT_THR;
        const bool SW_WR_DELAY_EN;
        const unsigned SW_WR_DELAY;
        const bool PREFER_WR;
        const bool SW_WR_CAM_THR_EN;
        const unsigned SW_WR_CAM_THR;

        const unsigned RD_OPEN_BANK_NUM;
        const unsigned WR_OPEN_BANK_NUM;
        const unsigned RD_KEEP_PAGE_NUM;
        const unsigned WR_KEEP_PAGE_NUM;
        const unsigned PAGE_CLOSE_TIME;

        //Refresh
        const bool REFRESH_ENABLE;
        const unsigned REFRESH_PENDING_THRESHOLD;
        const bool REFRESH_MANAGER_ENABLE;
        const bool REFRESH_POSTPONE_ENABLE;

        const unsigned POSTPONE_LOW_THRESHOLD_SAME_BANK;
        const unsigned POSTPONE_LOW_THRESHOLD_ALL_BANK;

        const bool ACT_MASK_NORMAL_REFRESH_ENABLE;
        const bool CRITICAL_REFRESH_MASK_ACT_ENABLE;

        //CHI Port
        const unsigned RD_DAT_INFO_DEPTH;
        const unsigned WR_DAT_BUFFER_DEPTH;

        const unsigned LPR_QUEUE_DEPTH;
        const unsigned HPR_QUEUE_DEPTH;
        const unsigned TPW_QUEUE_DEPTH;

        const unsigned WR_CQ_DEPTH;
        const unsigned RD_CQ_DEPTH;

        const unsigned RSP0_FIFO_DEPTH;
        const unsigned RSP1_FIFO_DEPTH;
        const unsigned RSP2_FIFO_DEPTH;
        const unsigned RSP3_FIFO_DEPTH;
        const unsigned RSP4_FIFO_DEPTH;
        const unsigned RSP5_FIFO_DEPTH;

        const bool RETRY_GPR_EXPIRED_ENABLE;
        const bool RETRY_GPW_EXPIRED_ENABLE;
        const unsigned RETRY_GPR_EXPIRED_TIME;
        const unsigned RETRY_GPW_EXPIRED_TIME;

        const unsigned MIN_LGPR_QUEUE_DEPTH;
        const unsigned MIN_HPR_QUEUE_DEPTH;

        const bool PA_RDWR_SWITCH_FAST;
        const unsigned PORT_AGING_INIT; // DFI cycle

        const bool CBUSY_ENABLE;
        const bool MPAM_ENABLE;

        const bool QOS_MAPPING;
        const bool SRC_ID_MAPPING;
        const unsigned QOS_THRESHOD_GROUP0;
        const unsigned QOS_THRESHOD_GROUP1;
        const unsigned QOS_THRESHOD_GROUP2;
        const std::set<unsigned> SRC_ID_GROUP0;
        const std::set<unsigned> SRC_ID_GROUP1;
        const std::set<unsigned> SRC_ID_GROUP2;

        const unsigned PROTQ_FREE_THRESHOLD_LGPR;
        const unsigned PROTQ_MIDDLE_THRESHOLD_LGPR;
        const unsigned PROTQ_BUSY_THRESHOLD_LGPR;
        const unsigned PROTQ_FREE_THRESHOLD_HPR;
        const unsigned PROTQ_MIDDLE_THRESHOLD_HPR;
        const unsigned PROTQ_BUSY_THRESHOLD_HPR;
        const unsigned PROTQ_FREE_THRESHOLD_WR;
        const unsigned PROTQ_MIDDLE_THRESHOLD_WR;
        const unsigned PROTQ_BUSY_THRESHOLD_WR;
        const unsigned PROTQ_FREE_THRESHOLD_RD;
        const unsigned PROTQ_MIDDLE_THRESHOLD_RD;
        const unsigned PROTQ_BUSY_THRESHOLD_RD;

        const unsigned CAM_FREE_THRESHOLD_LGPR;
        const unsigned CAM_MIDDLE_THRESHOLD_LGPR;
        const unsigned CAM_BUSY_THRESHOLD_LGPR;
        const unsigned CAM_FREE_THRESHOLD_HPR;
        const unsigned CAM_MIDDLE_THRESHOLD_HPR;
        const unsigned CAM_BUSY_THRESHOLD_HPR;
        const unsigned CAM_FREE_THRESHOLD_WR;
        const unsigned CAM_MIDDLE_THRESHOLD_WR;
        const unsigned CAM_BUSY_THRESHOLD_WR;

        const bool CAM_BASED;
        const bool PROTQ_BASED;

        explicit McConfig(const ControllerConfig& controller_config);
    };



}
}



#endif