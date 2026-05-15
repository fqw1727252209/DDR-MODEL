#ifndef __LOAD_MEM_SPEC_HH__
#define __LOAD_MEM_SPEC_HH__

#include <cstdint>
#include <string>

#include "Configure/LoadConfigFromJson.hh"

namespace dmu{

#define MEM_DEVICE_FIELDS(M) \
    M(unsigned, DeviceWidth) \
    M(unsigned, DataRate) \
    M(unsigned, NumOfBankGroupsPerDevice) \
    M(unsigned, NumOfBanksPerBankGroup) \
    M(unsigned, NumOfColumns) \
    M(unsigned, NumOfRows) \
    M(unsigned, RAAIMT) \
    M(unsigned, RAAMMT) \
    M(unsigned, RAADEC)

#define MEM_SPEEDBINS_FIELDS(M) \
    M(double, CLK) \
    M(unsigned, Freq) \
    M(unsigned, CL) \
    M(unsigned, nRCD) \
    M(unsigned, nRP) \
    M(unsigned, CWL) \
    M(double, tRCD) \
    M(double, tRP) \
    M(double, tRAS)

#define MEM_DATAACTIMING_FIELDS(M) \
    M(uint8_t, tRPRE) \
    M(float, tRPST) \
    M(uint8_t, tWPRE) \
    M(float, tWPST) \
    M(uint8_t, Read_DQS_Offset)

#define MEM_MEMARCHITECT_FIELDS(M) \
    M(uint8_t, NumOfChannels) \
    M(uint8_t, NumOfSubChannelsPerChannel) \
    M(uint8_t, NumOfPseudoChannelsPerSubChannel) \
    M(uint8_t, NumOfPhysicalRanksPerPseudoChannel) \
    M(uint8_t, NumOfLogicalRanksPerPhysicalRank)

#define MEM_RANKSWITCH_FIELDS(M) \
    M(unsigned, Wr2WrPrank) \
    M(unsigned, Wr2RdPrank) \
    M(unsigned, Rd2RdPrank) \
    M(unsigned, Rd2WrPrank) \
    M(unsigned, Wr2WrLrank) \
    M(unsigned, Wr2RdLrank) \
    M(unsigned, Rd2RdLrank) \
    M(unsigned, Rd2WrLrank)

#define MEM_ACTIMING_FIELDS(M) \
    M(double, tCCD_L_slr) \
    M(double, tCCD_L_WR_slr) \
    M(double, tCCD_L_WR2_slr) \
    M(double, tCCD_L_WTR_slr_part) \
    M(double, tCCD_M_slr) \
    M(double, tCCD_M_WR_slr) \
    M(double, tCCD_M_WTR_slr_part) \
    M(unsigned, tCCD_S_slr) \
    M(unsigned, tCCD_S_WR_slr) \
    M(double, tCCD_S_WTR_slr_part) \
    M(double, tRRD_L_slr) \
    M(unsigned, tRRD_S_slr) \
    M(double, tFAW_slr) \
    M(double, tRTP_slr) \
    M(unsigned, tPPD_slr) \
    M(double, tWR_slr) \
    M(double, tCCD_dlr) \
    M(double, tCCD_WR_dlr) \
    M(double, tCCD_WTR_dlr_part) \
    M(double, tRRD_dlr) \
    M(double, tFAW_dlr) \
    M(unsigned, tPPD_dlr) \
    M(double, tCCD_WR_dpr) \
    M(unsigned, tDCAW) \
    M(unsigned, nDCAC)

#define MEM_REFRESHACTIMING_FIELDS(M) \
    M(double, tREFI1) \
    M(double, tREFI2) \
    M(double, tRFC1_slr) \
    M(double, tRFC2_slr) \
    M(double, tRFCsb_slr) \
    M(double, tRFC1_dlr) \
    M(double, tRFC1_dpr) \
    M(double, tRFC2_dlr) \
    M(double, tRFC2_dpr) \
    M(double, tRFCsb_dlr) \
    M(double, tREFSBRD_slr) \
    M(double, tREFSBRD_dlr) \
    M(double, tREFABRD_dlr)

#define MEM_TOP_FIELDS(M) \
    M(std::string, MemoryId) \
    M(std::string, MemoryType) \
    M(bool, Is_3ds) \
    M(uint8_t, CmdMode) \
    M(uint8_t, FreqRatio) \
    M(uint8_t, RefreshMode) \
    M(uint8_t, BurstLenth) \
    M(uint8_t, MaxBurstLenth)

struct DDR5MemConfig
{
    static constexpr std::string_view SUB_DIR = "memspec";

    #define MEM_TOP_DECLARE(Type, Name) Type Name;
    MEM_TOP_FIELDS(MEM_TOP_DECLARE)
    #undef MEM_TOP_DECLARE

    struct DeviceStruct
    {
        #define MEM_DEVICE_DECLARE(Type, Name) Type Name;
        MEM_DEVICE_FIELDS(MEM_DEVICE_DECLARE)
        #undef MEM_DEVICE_DECLARE
    } Device;

    struct SpeedBinsStruct
    {
        #define MEM_SPEEDBINS_DECLARE(Type, Name) Type Name;
        MEM_SPEEDBINS_FIELDS(MEM_SPEEDBINS_DECLARE)
        #undef MEM_SPEEDBINS_DECLARE
    } SpeedBins;

    struct DataAcTimingStruct
    {
        #define MEM_DATAACTIMING_DECLARE(Type, Name) Type Name;
        MEM_DATAACTIMING_FIELDS(MEM_DATAACTIMING_DECLARE)
        #undef MEM_DATAACTIMING_DECLARE
    } DataAcTiming;

    struct MemArchitectStruct
    {
        #define MEM_MEMARCHITECT_DECLARE(Type, Name) Type Name;
        MEM_MEMARCHITECT_FIELDS(MEM_MEMARCHITECT_DECLARE)
        #undef MEM_MEMARCHITECT_DECLARE
    } MemArchitect;

    struct RankSwitchDelayStruct
    {
        #define MEM_RANKSWITCH_DECLARE(Type, Name) Type Name;
        MEM_RANKSWITCH_FIELDS(MEM_RANKSWITCH_DECLARE)
        #undef MEM_RANKSWITCH_DECLARE
    } RankSwitchDelay;

    struct AcTimingStruct
    {
        #define MEM_ACTIMING_DECLARE(Type, Name) Type Name;
        MEM_ACTIMING_FIELDS(MEM_ACTIMING_DECLARE)
        #undef MEM_ACTIMING_DECLARE
    } AcTiming;

    struct RefreshAcTimingStruct
    {
        #define MEM_REFRESHACTIMING_DECLARE(Type, Name) Type Name;
        MEM_REFRESHACTIMING_FIELDS(MEM_REFRESHACTIMING_DECLARE)
        #undef MEM_REFRESHACTIMING_DECLARE
    } RefreshAcTiming;
};

class LoadDDR5MemConfig : public LoadConfigFromJson
{
    public:
        DDR5MemConfig mem_spec;

        BEGIN_JSON_MAP(DDR5MemConfig::DeviceStruct)
            #define MEM_DEVICE_MAP(Type, Name) JSON_FIELD(Type, Name)
            MEM_DEVICE_FIELDS(MEM_DEVICE_MAP)
            #undef MEM_DEVICE_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::SpeedBinsStruct)
            #define MEM_SPEEDBINS_MAP(Type, Name) JSON_FIELD(Type, Name)
            MEM_SPEEDBINS_FIELDS(MEM_SPEEDBINS_MAP)
            #undef MEM_SPEEDBINS_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::DataAcTimingStruct)
            #define MEM_DATAACTIMING_MAP(Type, Name) JSON_FIELD(Type, Name)
            MEM_DATAACTIMING_FIELDS(MEM_DATAACTIMING_MAP)
            #undef MEM_DATAACTIMING_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::MemArchitectStruct)
            #define MEM_MEMARCHITECT_MAP(Type, Name) JSON_FIELD(Type, Name)
            MEM_MEMARCHITECT_FIELDS(MEM_MEMARCHITECT_MAP)
            #undef MEM_MEMARCHITECT_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::RankSwitchDelayStruct)
            #define MEM_RANKSWITCH_MAP(Type, Name) JSON_FIELD(Type, Name)
            MEM_RANKSWITCH_FIELDS(MEM_RANKSWITCH_MAP)
            #undef MEM_RANKSWITCH_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::AcTimingStruct)
            #define MEM_ACTIMING_MAP(Type, Name) JSON_FIELD(Type, Name)
            MEM_ACTIMING_FIELDS(MEM_ACTIMING_MAP)
            #undef MEM_ACTIMING_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::RefreshAcTimingStruct)
            #define MEM_REFRESHACTIMING_MAP(Type, Name) JSON_FIELD(Type, Name)
            MEM_REFRESHACTIMING_FIELDS(MEM_REFRESHACTIMING_MAP)
            #undef MEM_REFRESHACTIMING_MAP
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig)
            #define MEM_TOP_MAP(Type, Name) JSON_FIELD(Type, Name)
            MEM_TOP_FIELDS(MEM_TOP_MAP)
            #undef MEM_TOP_MAP
            JSON_NESTED_STRUCT(Device)
            JSON_NESTED_STRUCT(SpeedBins)
            JSON_NESTED_STRUCT(DataAcTiming)
            JSON_NESTED_STRUCT(MemArchitect)
            JSON_NESTED_STRUCT(RankSwitchDelay)
            JSON_NESTED_STRUCT(AcTiming)
            JSON_NESTED_STRUCT(RefreshAcTiming)
        END_JSON_MAP()

        explicit LoadDDR5MemConfig(const std::string& filename) {
            LoadFromJson(filename);
        }

        bool ParseJson() override {
            if (!doc.IsObject()) {
                std::cerr << "JSON root is not an object!" << std::endl;
                return false;
            }

            bool parse_result = ParseStruct(doc, mem_spec);

            assert(mem_spec.CmdMode == 0 || mem_spec.CmdMode == 1);
            assert(mem_spec.RefreshMode == 0 || mem_spec.RefreshMode == 1);
            assert(mem_spec.SpeedBins.CWL == mem_spec.SpeedBins.CL-2);

            assert(mem_spec.AcTiming.tCCD_S_slr == mem_spec.BurstLenth/2 &&
                   mem_spec.AcTiming.tCCD_S_WR_slr == mem_spec.BurstLenth/2);

            return parse_result;
        }
};

}

#endif