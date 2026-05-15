#ifndef __LOAD_CONTROLLER_CONFIG_HH__
#define __LOAD_CONTROLLER_CONFIG_HH__

#include <string>
#include "Configure/LoadConfigFromJson.hh"

namespace dmu{

/*
 * X-Macro nested struct example (using ControllerConfig as reference):
 *
 * 1. Define the nested struct's field list macro:
 *    #define MY_NESTED_FIELDS(M) \
 *        M(unsigned, foo) \
 *        M(bool, bar)
 *
 * 2. Declare the nested struct inside the parent struct:
 *    struct MyNestedStruct { \
 *        #define NESTED_DECLARE(Type, Name) Type Name; \
 *        MY_NESTED_FIELDS(NESTED_DECLARE) \
 *        #undef NESTED_DECLARE \
 *    } myNested;
 *
 * 3. Define JSON mapping for the nested struct itself:
 *    BEGIN_JSON_MAP(ParentStruct::MyNestedStruct) \
 *        #define NESTED_MAP(Type, Name) JSON_FIELD(Type, Name) \
 *        MY_NESTED_FIELDS(NESTED_MAP) \
 *        #undef NESTED_MAP \
 *    END_JSON_MAP()
 *
 * 4. In parent's BEGIN_JSON_MAP, add the nested member:
 *    BEGIN_JSON_MAP(ParentStruct) \
 *        ... parent fields ... \
 *        JSON_NESTED_STRUCT(myNested) \
 *    END_JSON_MAP()
 */

// --------- SchedulerConfigStruct field list ---------
#define SCHEDULER_CONFIG_FIELDS(M) \
    M(unsigned, RD_CAM_DEPTH) \
    M(unsigned, WR_CAM_DEPTH) \
    M(unsigned, WDAT_BUFFER_DEPTH) \
    M(unsigned, RDAT_BUFFER_DEPTH) \
    M(unsigned, BSC_NUM) \
    M(bool, AUTO_PRECHARGE_ENABLE) \
    M(bool, NCWEN) \
    M(bool, BANK_HASH_ENABLE) \
    M(unsigned, BSC_USED_HIGH_THRESHOLD) \
    M(unsigned, BSC_USED_LOW_THRESHOLD) \
    M(bool, DYNAMIC_BSC_ENABLE) \
    M(bool, DYNAMIC_BSC_BUSY_RELEASE_ENABLE) \
    M(bool, BSC_DEEP_RELEASE_ENABLE) \
    M(bool, DYNAMIC_BSC_BUSY_MASK_ENABLE) \
    M(bool, BSC_TERMINATE_BROKEN_ENABLE) \
    M(bool, BSC_TERMINATE_ENABLE) \
    M(unsigned, HPR_CREDIT) \
    M(unsigned, LPR_CREDIT) \
    M(unsigned, TPW_CREDIT) \
    M(bool, HPR_FILL_LEVEL_MODE) \
    M(bool, LPR_FILL_LEVEL_MODE) \
    M(bool, TPW_FILL_LEVEL_MODE) \
    M(bool, HPR_STARVE_COUNT_MODE) \
    M(bool, LPR_STARVE_COUNT_MODE) \
    M(bool, TPW_STARVE_COUNT_MODE) \
    M(unsigned, HPR_HIGH_THRESHOLD) \
    M(unsigned, HPR_LOW_THRESHOLD) \
    M(unsigned, LPR_HIGH_THRESHOLD) \
    M(unsigned, LPR_LOW_THRESHOLD) \
    M(unsigned, TPW_HIGH_THRESHOLD) \
    M(unsigned, TPW_LOW_THRESHOLD) \
    M(bool, WR_COMBINE_ENABLE) \
    M(unsigned, HPR_MAX_STARVE) \
    M(unsigned, LPR_MAX_STARVE) \
    M(unsigned, TPW_MAX_STARVE) \
    M(unsigned, HPR_CMD_RUNLEN) \
    M(unsigned, LPR_CMD_RUNLEN) \
    M(unsigned, TPW_CMD_RUNLEN) \
    M(unsigned, Max_RW_BEFORE_SWITCH) \
    M(unsigned, OPP_HIT_CNT) \
    M(bool, MODE_SWITCH_POLICY) \
    M(unsigned, WR_HIT_LIMIT_THRESHOLD) \
    M(unsigned, RD_HIT_LIMIT_THRESHOLD) \
    M(unsigned, RD_CMD_AGING_LIMIT) \
    M(unsigned, WE_CMD_AGING_LIMIT) \
    M(bool, WR_OPPOSITE_ACT_ENABLE) \
    M(bool, RD_OPPOSITE_ACT_ENABLE) \
    M(bool, PREFER_HIT_HPR) \
    M(bool, RD_CNT_THR_EN) \
    M(unsigned, RD_CNT_THR) \
    M(bool, WR_CNT_THR_EN) \
    M(unsigned, WR_CNT_THR) \
    M(bool, SW_WR_DELAY_EN) \
    M(unsigned, SW_WR_DELAY) \
    M(bool, PREFER_WR) \
    M(bool, SW_WR_CAM_THR_EN) \
    M(unsigned, SW_WR_CAM_THR) \
    M(unsigned, RD_OPEN_BANK_NUM) \
    M(unsigned, WR_OPEN_BANK_NUM) \
    M(unsigned, RD_KEEP_PAGE_NUM) \
    M(unsigned, WR_KEEP_PAGE_NUM) \
    M(unsigned, PAGE_CLOSE_TIME)

// --------- RefreshConfigStruct field list ---------
#define REFRESH_CONFIG_FIELDS(M) \
    M(bool, REFRESH_ENABLE) \
    M(unsigned, REFRESH_PENDING_THRESHOLD) \
    M(bool, REFRESH_MANAGER_ENABLE) \
    M(bool, REFRESH_POSTPONE_ENABLE) \
    M(unsigned, POSTPONE_LOW_THRESHOLD_SAME_BANK) \
    M(unsigned, POSTPONE_LOW_THRESHOLD_ALL_BANK) \
    M(bool, ACT_MASK_NORMAL_REFRESH_ENABLE) \
    M(bool, CRITICAL_REFRESH_MASK_ACT_ENABLE)

// --------- CalIdMappingStruct field list ---------
#define CAL_ID_MAPPING_FIELDS(M) \
    M(bool, QOS_MAPPING) \
    M(bool, SRC_ID_MAPPING) \
    M(unsigned, QOS_THRESHOD_GROUP0) \
    M(unsigned, QOS_THRESHOD_GROUP1) \
    M(unsigned, QOS_THRESHOD_GROUP2) \
    M(std::set<unsigned>, SRC_ID_GROUP0) \
    M(std::set<unsigned>, SRC_ID_GROUP1) \
    M(std::set<unsigned>, SRC_ID_GROUP2)

// --------- PortConfigStruct field list ---------
#define PORT_CONFIG_FIELDS(M) \
    M(unsigned, RD_DAT_INFO_DEPTH) \
    M(unsigned, WR_DAT_BUFFER_DEPTH) \
    M(unsigned, LPR_QUEUE_DEPTH) \
    M(unsigned, HPR_QUEUE_DEPTH) \
    M(unsigned, TPW_QUEUE_DEPTH) \
    M(unsigned, WR_CQ_DEPTH) \
    M(unsigned, RD_CQ_DEPTH) \
    M(unsigned, RSP0_FIFO_DEPTH) \
    M(unsigned, RSP1_FIFO_DEPTH) \
    M(unsigned, RSP2_FIFO_DEPTH) \
    M(unsigned, RSP3_FIFO_DEPTH) \
    M(unsigned, RSP4_FIFO_DEPTH) \
    M(unsigned, RSP5_FIFO_DEPTH) \
    M(bool, RETRY_GPR_EXPIRED_ENABLE) \
    M(bool, RETRY_GPW_EXPIRED_ENABLE) \
    M(unsigned, RETRY_GPR_EXPIRED_TIME) \
    M(unsigned, RETRY_GPW_EXPIRED_TIME) \
    M(unsigned, MIN_LPR_QUEUE_DEPTH) \
    M(unsigned, MIN_HPR_QUEUE_DEPTH) \
    M(bool, PA_RDWR_SWITCH_FAST) \
    M(bool, CBUSY_ENABLE) \
    M(bool, MPAM_ENABLE) \
    M(unsigned, PROTQ_FREE_THRESHOLD_LGPR) \
    M(unsigned, PROTQ_MIDDLE_THRESHOLD_LGPR) \
    M(unsigned, PROTQ_BUSY_THRESHOLD_LGPR) \
    M(unsigned, PROTQ_FREE_THRESHOLD_HPR) \
    M(unsigned, PROTQ_MIDDLE_THRESHOLD_HPR) \
    M(unsigned, PROTQ_BUSY_THRESHOLD_HPR) \
    M(unsigned, PROTQ_FREE_THRESHOLD_WR) \
    M(unsigned, PROTQ_MIDDLE_THRESHOLD_WR) \
    M(unsigned, PROTQ_BUSY_THRESHOLD_WR) \
    M(unsigned, PROTQ_FREE_THRESHOLD_RD) \
    M(unsigned, PROTQ_MIDDLE_THRESHOLD_RD) \
    M(unsigned, PROTQ_BUSY_THRESHOLD_RD) \
    M(unsigned, CAM_FREE_THRESHOLD_LGPR) \
    M(unsigned, CAM_MIDDLE_THRESHOLD_LGPR) \
    M(unsigned, CAM_BUSY_THRESHOLD_LGPR) \
    M(unsigned, CAM_FREE_THRESHOLD_HPR) \
    M(unsigned, CAM_MIDDLE_THRESHOLD_HPR) \
    M(unsigned, CAM_BUSY_THRESHOLD_HPR) \
    M(unsigned, CAM_FREE_THRESHOLD_WR) \
    M(unsigned, CAM_MIDDLE_THRESHOLD_WR) \
    M(unsigned, CAM_BUSY_THRESHOLD_WR) \
    M(bool, CAM_BASED) \
    M(bool, PROTQ_BASED) \
    M(unsigned, PORT_AGING_INIT)

// --------- ControllerConfig top-level field list ---------
#define CONTROLLER_TOP_FIELDS(M) \
    M(unsigned, RD_QOS_LEVEL_0) \
    M(unsigned, RD_QOS_LEVEL_1) \
    M(unsigned, WR_QOS_LEVEL) \
    M(unsigned, GPW_EXPIRED_TIME) \
    M(unsigned, GPR_EXPIRED_TIME) \
    M(double, PHY_CMD_DELAY) \
    M(double, PHY_RDAT_DELAY) \
    M(double, PHY_WDAT_DELAY)

struct ControllerConfig
{
    static constexpr std::string_view SUB_DIR = "mcconfig";

    #define CTRL_TOP_DECLARE(Type, Name) Type Name;
    CONTROLLER_TOP_FIELDS(CTRL_TOP_DECLARE)
    #undef CTRL_TOP_DECLARE

    struct SchedulerConfigStruct
    {
        #define SCHEDULER_DECLARE(Type, Name) Type Name;
        SCHEDULER_CONFIG_FIELDS(SCHEDULER_DECLARE)
        #undef SCHEDULER_DECLARE
    } SchedulerConfig;

    struct RefreshConfigStruct
    {
        #define REFRESH_DECLARE(Type, Name) Type Name;
        REFRESH_CONFIG_FIELDS(REFRESH_DECLARE)
        #undef REFRESH_DECLARE
    } RefreshConfig;

    struct PortConfigStruct
    {
        #define PORT_DECLARE(Type, Name) Type Name;
        PORT_CONFIG_FIELDS(PORT_DECLARE)
        #undef PORT_DECLARE

        struct CalIdMappingStruct
        {
            #define CAL_ID_DECLARE(Type, Name) Type Name;
            CAL_ID_MAPPING_FIELDS(CAL_ID_DECLARE)
            #undef CAL_ID_DECLARE
        } CalIdMapping;
    } PortConfig;
};

class LoadControllerConfig: public LoadConfigFromJson
{
    public:
        ControllerConfig controller_config;

        BEGIN_JSON_MAP(ControllerConfig::SchedulerConfigStruct)
            #define SCHEDULER_MAP(Type, Name) JSON_FIELD(Type, Name)
            SCHEDULER_CONFIG_FIELDS(SCHEDULER_MAP)
            #undef SCHEDULER_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(ControllerConfig::RefreshConfigStruct)
            #define REFRESH_MAP(Type, Name) JSON_FIELD(Type, Name)
            REFRESH_CONFIG_FIELDS(REFRESH_MAP)
            #undef REFRESH_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(ControllerConfig::PortConfigStruct::CalIdMappingStruct)
            #define CAL_ID_MAP(Type, Name) JSON_FIELD(Type, Name)
            CAL_ID_MAPPING_FIELDS(CAL_ID_MAP)
            #undef CAL_ID_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(ControllerConfig::PortConfigStruct)
            #define PORT_MAP(Type, Name) JSON_FIELD(Type, Name)
            PORT_CONFIG_FIELDS(PORT_MAP)
            #undef PORT_MAP
            JSON_NESTED_STRUCT(CalIdMapping)
        END_JSON_MAP()

        BEGIN_JSON_MAP(ControllerConfig)
            #define CTRL_TOP_MAP(Type, Name) JSON_FIELD(Type, Name)
            CONTROLLER_TOP_FIELDS(CTRL_TOP_MAP)
            #undef CTRL_TOP_MAP
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