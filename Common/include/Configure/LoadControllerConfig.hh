#ifndef __LOAD_CONTROLLER_CONFIG_HH__
#define __LOAD_CONTROLLER_CONFIG_HH__

#include <string>
#include "Configure/LoadConfigFromJson.hh"

namespace dmu{

    struct ControllerConfig
    {
        static constexpr std::string_view SUB_DIR = "mcconfig";

        unsigned RD_QOS_LEVEL_0;
        unsigned RD_QOS_LEVEL_1;
        unsigned WR_QOS_LEVEL;

        unsigned GPW_EXPIRED_TIME;
        unsigned GPR_EXPIRED_TIME;

        double PHY_CMD_DELAY;
        double PHY_RDAT_DELAY;
        double PHY_WDAT_DELAY;

        struct SchedulerConfigStruct
        {
            unsigned RD_CAM_DEPTH;
            unsigned WR_CAM_DEPTH;
            unsigned WDAT_BUFFER_DEPTH;
            unsigned RDAT_BUFFER_DEPTH;
            unsigned BSC_NUM;

            bool AUTO_PRECHARGE_ENABLE;
            bool NCWEN;

            bool BANK_HASH_ENABLE;

            unsigned BSC_USED_HIGH_THRESHOLD;
            unsigned BSC_USED_LOW_THRESHOLD;
            bool DYNAMIC_BSC_ENABLE;
            bool DYNAMIC_BSC_BUSY_RELEASE_ENABLE;
            bool BSC_DEEP_RELEASE_ENABLE;
            bool DYNAMIC_BSC_BUSY_MASK_ENABLE;
            bool BSC_TERMINATE_BROKEN_ENABLE;
            bool BSC_TERMINATE_ENABLE;

            unsigned HPR_CREDIT;
            unsigned LPR_CREDIT;
            unsigned TPW_CREDIT;

            bool HPR_FILL_LEVEL_MODE;
            bool LPR_FILL_LEVEL_MODE;
            bool TPW_FILL_LEVEL_MODE;
            bool HPR_STARVE_COUNT_MODE;
            bool LPR_STARVE_COUNT_MODE;
            bool TPW_STARVE_COUNT_MODE;

            unsigned HPR_HIGH_THRESHOLD;
            unsigned HPR_LOW_THRESHOLD;
            unsigned LPR_HIGH_THRESHOLD;
            unsigned LPR_LOW_THRESHOLD;
            unsigned TPW_HIGH_THRESHOLD;
            unsigned TPW_LOW_THRESHOLD;

            bool WR_COMBINE_ENABLE;

            unsigned HPR_MAX_STARVE;
            unsigned LPR_MAX_STARVE;
            unsigned TPW_MAX_STARVE;
            unsigned HPR_CMD_RUNLEN;
            unsigned LPR_CMD_RUNLEN;
            unsigned TPW_CMD_RUNLEN;

            unsigned Max_RW_BEFORE_SWITCH;
            unsigned OPP_HIT_CNT;
            bool MODE_SWITCH_POLICY;

            unsigned WR_HIT_LIMIT_THRESHOLD;
            unsigned RD_HIT_LIMIT_THRESHOLD;

            unsigned RD_CMD_AGING_LIMIT;
            unsigned WE_CMD_AGING_LIMIT;

            bool WR_OPPOSITE_ACT_ENABLE;
            bool RD_OPPOSITE_ACT_ENABLE;

            bool PREFER_HIT_HPR;

        } SchedulerConfig;

        struct RefreshConfigStruct
        {
            bool REFRESH_ENABLE;
            unsigned REFRESH_PENDING_THRESHOLD;
            unsigned REFRESH_PENDING_THRESHOLD_FGR; // MaxPostpone2x：FGR 模式下独立阈值
            bool REFRESH_MANAGER_ENABLE;
            bool REFRESH_POSTPONE_ENABLE;

            unsigned POSTPONE_LOW_THRESHOLD_SAME_BANK;
            unsigned POSTPONE_LOW_THRESHOLD_ALL_BANK;

            bool ACT_MASK_NORMAL_REFRESH_ENABLE;
            bool CRITICAL_REFRESH_MASK_ACT_ENABLE;

            bool REFAB_ENABLE;
            unsigned RAA_THRESHOLD;
            bool REF_STAGGER_ENABLE;
            unsigned MR4_TEMP_MULTIPLIER;

            // RTL 对齐：各 Rank 独立起始偏置（单位 ns，对应 Rank${i}TrefiStartValue）
            std::vector<double> RANK_TREFI_START_VALUES;

            // RTL 对齐：RFM 滑动窗口参数
            unsigned RAAMULT;  // 每发一笔 ACT，RAA 计数器增加的权重
            unsigned RAADEC;   // 每发一笔 REF，RAA 计数器减少的权重（对应 Raadec 寄存器）
            unsigned RAAIMT;   // RAA 初级预警阈值，超过则建议发 RFM（0=不启用）
            unsigned RAAMMT;   // RAA 最大阈值，超过则强制阻断 ACT（0=不启用）
        } RefreshConfig;

        struct PortConfigStruct{
            unsigned RD_DAT_INFO_DEPTH;
            unsigned WR_DAT_BUFFER_DEPTH;

            unsigned LPR_QUEUE_DEPTH;
            unsigned HPR_QUEUE_DEPTH;
            unsigned TPW_QUEUE_DEPTH;

            unsigned WR_CQ_DEPTH;
            unsigned RD_CQ_DEPTH;

            unsigned RSP0_FIFO_DEPTH;
            unsigned RSP1_FIFO_DEPTH;
            unsigned RSP2_FIFO_DEPTH;
            unsigned RSP3_FIFO_DEPTH;
            unsigned RSP4_FIFO_DEPTH;
            unsigned RSP5_FIFO_DEPTH;

            bool RETRY_GPR_EXPIRED_ENABLE;
            bool RETRY_GPW_EXPIRED_ENABLE;
            unsigned RETRY_GPR_EXPIRED_TIME;
            unsigned RETRY_GPW_EXPIRED_TIME;

            unsigned MIN_LGPR_QUEUE_DEPTH;
            unsigned MIN_HPR_QUEUE_DEPTH;

            bool PA_RDWR_SWITCH_FAST;
            unsigned PORT_AGING_INIT;

        } PortConfig;

    };

    class LoadControllerConfig: public LoadConfigFromJson
    {
        public:
            ControllerConfig controller_config;

            BEGIN_JSON_MAP(ControllerConfig::SchedulerConfigStruct)
                JSON_FIELD(unsigned, RD_CAM_DEPTH)
                JSON_FIELD(unsigned, WR_CAM_DEPTH)
                JSON_FIELD(unsigned, WDAT_BUFFER_DEPTH)
                JSON_FIELD(unsigned, RDAT_BUFFER_DEPTH)
                JSON_FIELD(unsigned, BSC_NUM)
                JSON_FIELD(bool, AUTO_PRECHARGE_ENABLE)
                JSON_FIELD(bool, NCWEN)
                JSON_FIELD(bool, BANK_HASH_ENABLE)
                JSON_FIELD(unsigned, BSC_USED_HIGH_THRESHOLD)
                JSON_FIELD(unsigned, BSC_USED_LOW_THRESHOLD)
                JSON_FIELD(bool, DYNAMIC_BSC_ENABLE)
                JSON_FIELD(bool, DYNAMIC_BSC_BUSY_RELEASE_ENABLE)
                JSON_FIELD(bool, BSC_DEEP_RELEASE_ENABLE)
                JSON_FIELD(bool, DYNAMIC_BSC_BUSY_MASK_ENABLE)
                JSON_FIELD(bool, BSC_TERMINATE_BROKEN_ENABLE)
                JSON_FIELD(bool, BSC_TERMINATE_ENABLE)
                JSON_FIELD(unsigned, HPR_CREDIT)
                JSON_FIELD(unsigned, LPR_CREDIT)
                JSON_FIELD(unsigned, TPW_CREDIT)
                JSON_FIELD(bool, HPR_FILL_LEVEL_MODE)
                JSON_FIELD(bool, LPR_FILL_LEVEL_MODE)
                JSON_FIELD(bool, TPW_FILL_LEVEL_MODE)
                JSON_FIELD(bool, HPR_STARVE_COUNT_MODE)
                JSON_FIELD(bool, LPR_STARVE_COUNT_MODE)
                JSON_FIELD(bool, TPW_STARVE_COUNT_MODE)
                JSON_FIELD(unsigned, HPR_HIGH_THRESHOLD)
                JSON_FIELD(unsigned, HPR_LOW_THRESHOLD)
                JSON_FIELD(unsigned, LPR_HIGH_THRESHOLD)
                JSON_FIELD(unsigned, LPR_LOW_THRESHOLD)
                JSON_FIELD(unsigned, TPW_HIGH_THRESHOLD)
                JSON_FIELD(unsigned, TPW_LOW_THRESHOLD)
                JSON_FIELD(bool, WR_COMBINE_ENABLE)
                JSON_FIELD(unsigned, HPR_MAX_STARVE)
                JSON_FIELD(unsigned, LPR_MAX_STARVE)
                JSON_FIELD(unsigned, TPW_MAX_STARVE)
                JSON_FIELD(unsigned, HPR_CMD_RUNLEN)
                JSON_FIELD(unsigned, LPR_CMD_RUNLEN)
                JSON_FIELD(unsigned, TPW_CMD_RUNLEN)
                JSON_FIELD(unsigned, Max_RW_BEFORE_SWITCH)
                JSON_FIELD(unsigned, OPP_HIT_CNT)
                JSON_FIELD(bool, MODE_SWITCH_POLICY)
                JSON_FIELD(unsigned, WR_HIT_LIMIT_THRESHOLD)
                JSON_FIELD(unsigned, RD_HIT_LIMIT_THRESHOLD)
                JSON_FIELD(unsigned, RD_CMD_AGING_LIMIT)
                JSON_FIELD(unsigned, WE_CMD_AGING_LIMIT)
                JSON_FIELD(bool, WR_OPPOSITE_ACT_ENABLE)
                JSON_FIELD(bool, RD_OPPOSITE_ACT_ENABLE)
                JSON_FIELD(bool, PREFER_HIT_HPR)
            END_JSON_MAP()

            BEGIN_JSON_MAP(ControllerConfig::RefreshConfigStruct)
                JSON_FIELD(bool, REFRESH_ENABLE)
                JSON_FIELD(unsigned, REFRESH_PENDING_THRESHOLD)
                JSON_FIELD(unsigned, REFRESH_PENDING_THRESHOLD_FGR)
                JSON_FIELD(bool, REFRESH_MANAGER_ENABLE)
                JSON_FIELD(bool, REFRESH_POSTPONE_ENABLE)
                JSON_FIELD(unsigned, POSTPONE_LOW_THRESHOLD_SAME_BANK)
                JSON_FIELD(unsigned, POSTPONE_LOW_THRESHOLD_ALL_BANK)
                JSON_FIELD(bool, ACT_MASK_NORMAL_REFRESH_ENABLE)
                JSON_FIELD(bool, CRITICAL_REFRESH_MASK_ACT_ENABLE)
                JSON_FIELD(bool, REFAB_ENABLE)
                JSON_FIELD(unsigned, RAA_THRESHOLD)
                JSON_FIELD(bool, REF_STAGGER_ENABLE)
                JSON_FIELD(unsigned, MR4_TEMP_MULTIPLIER)
                JSON_FIELD(std::vector<double>, RANK_TREFI_START_VALUES)
                JSON_FIELD(unsigned, RAAMULT)
                JSON_FIELD(unsigned, RAADEC)
                JSON_FIELD(unsigned, RAAIMT)
                JSON_FIELD(unsigned, RAAMMT)
            END_JSON_MAP()

            BEGIN_JSON_MAP(ControllerConfig::PortConfigStruct)
                JSON_FIELD(unsigned, RD_DAT_INFO_DEPTH)
                JSON_FIELD(unsigned, WR_DAT_BUFFER_DEPTH)
                JSON_FIELD(unsigned, LPR_QUEUE_DEPTH)
                JSON_FIELD(unsigned, HPR_QUEUE_DEPTH)
                JSON_FIELD(unsigned, TPW_QUEUE_DEPTH)
                JSON_FIELD(unsigned, WR_CQ_DEPTH)
                JSON_FIELD(unsigned, RD_CQ_DEPTH)
                JSON_FIELD(unsigned, RSP0_FIFO_DEPTH)
                JSON_FIELD(unsigned, RSP1_FIFO_DEPTH)
                JSON_FIELD(unsigned, RSP2_FIFO_DEPTH)
                JSON_FIELD(unsigned, RSP3_FIFO_DEPTH)
                JSON_FIELD(unsigned, RSP4_FIFO_DEPTH)
                JSON_FIELD(unsigned, RSP5_FIFO_DEPTH)
                JSON_FIELD(bool, RETRY_GPR_EXPIRED_ENABLE)
                JSON_FIELD(bool, RETRY_GPW_EXPIRED_ENABLE)
                JSON_FIELD(unsigned, RETRY_GPR_EXPIRED_TIME)
                JSON_FIELD(unsigned, RETRY_GPW_EXPIRED_TIME)
                JSON_FIELD(unsigned, MIN_LGPR_QUEUE_DEPTH)
                JSON_FIELD(unsigned, MIN_HPR_QUEUE_DEPTH)
                JSON_FIELD(bool, PA_RDWR_SWITCH_FAST)
                JSON_FIELD(unsigned, PORT_AGING_INIT)
            END_JSON_MAP()

            BEGIN_JSON_MAP(ControllerConfig)
                JSON_FIELD(unsigned, RD_QOS_LEVEL_0)
                JSON_FIELD(unsigned, RD_QOS_LEVEL_1)
                JSON_FIELD(unsigned, WR_QOS_LEVEL)
                JSON_FIELD(unsigned, GPW_EXPIRED_TIME)
                JSON_FIELD(unsigned, GPR_EXPIRED_TIME)
                JSON_FIELD(double, PHY_CMD_DELAY)
                JSON_FIELD(double, PHY_RDAT_DELAY)
                JSON_FIELD(double, PHY_WDAT_DELAY)
                JSON_NESTED_STRUCT(SchedulerConfig)
                JSON_NESTED_STRUCT(RefreshConfig)
                JSON_NESTED_STRUCT(PortConfig)
            END_JSON_MAP()

            explicit LoadControllerConfig(const std::string& filename) {
                LoadFromJson(filename);
            }

            bool ParseJson() override {
                if (!doc.IsObject()) {
                    std::cerr << "JSON root is not an object!" << std::endl;
                    return false;
                }

                bool parse_result = ParseStruct(doc, controller_config);

                return parse_result;
            }
    };


    
}

#endif