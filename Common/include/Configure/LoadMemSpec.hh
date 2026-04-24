#ifndef __LOAD_MEM_SPEC_HH__
#define __LOAD_MEM_SPEC_HH__

#include <string>

#include "Configure/LoadConfigFromJson.hh"

namespace dmu{

struct DDR5MemSpecBase
{
    static constexpr std::string_view SUB_DIR = "memspec";

    std::string MemoryId;
    std::string MemoryType;

    double phy_cmd_delay;
    double phy_rdat_delay;
    double phy_wdat_delay;

    bool Is_3ds;
    uint8_t RefreshMode;
    uint8_t CmdMode;
    uint8_t FreqRatio;
    uint8_t NumOfChannels;
    uint8_t NumOfSubChannels;
    uint8_t BurstLenth;
    uint8_t MaxBurstLenth;
    struct DeviceStruct
    {
        unsigned width;
        unsigned DataRate;
        unsigned TotalNumOfBanksPerDevice;
        unsigned NumOfBankGroupsPerDevice;
        unsigned NumOfBanksPerBg;
        unsigned NumOfColumns;
        unsigned NumOfRows;
        unsigned RAAIMT;
        unsigned RAAMMT;
        unsigned RAADEC;
    } Device;

    struct SpeedBinsStruct
    {
        double CLK;
        unsigned Freq;
        unsigned CL;
        unsigned nRCD;
        unsigned nRP;
        unsigned CWL;
        double tRCD;
        double tRP;
        double tRAS;
    } SpeedBins;

    struct DataAcTimingStruct
    {
        uint8_t tRPRE;
        float tRPST;
        uint8_t tWPRE;
        float tWPST;
        uint8_t Read_DQS_Offset;
    } DataAcTiming;

};

struct DDR5MemConfig//: public DDR5MemSpecBase
{
    static constexpr std::string_view SUB_DIR = "memspec";

    std::string MemoryId;
    std::string MemoryType;

    double phy_cmd_delay;
    double phy_rdat_delay;
    double phy_wdat_delay;

    bool Is_3ds;
    uint8_t RefreshMode;
    uint8_t CmdMode;
    uint8_t FreqRatio;
    uint8_t NumOfChannels;
    uint8_t NumOfSubChannels;
    uint8_t BurstLenth;
    uint8_t MaxBurstLenth;
    struct DeviceStruct
    {
        unsigned width;
        unsigned DataRate;
        unsigned TotalNumOfBanksPerDevice;
        unsigned NumOfBankGroupsPerDevice;
        unsigned NumOfBanksPerBg;
        unsigned NumOfColumns;
        unsigned NumOfRows;
        unsigned RAAIMT;
        unsigned RAAMMT;
        unsigned RAADEC;
    } Device;

    struct SpeedBinsStruct
    {
        double CLK;
        unsigned Freq;
        unsigned CL;
        unsigned nRCD;
        unsigned nRP;
        unsigned CWL;
        double tRCD;
        double tRP;
        double tRAS;
    } SpeedBins;

    struct DataAcTimingStruct
    {
        uint8_t tRPRE;
        float tRPST;
        uint8_t tWPRE;
        float tWPST;
        uint8_t Read_DQS_Offset;
    } DataAcTiming;

    struct MemArchitectStruct
    {
        uint8_t NumOfDimmRanks;
        uint8_t NumOfPhysicalRanksPerChannel;
        uint8_t NumOfLogicalRanksPerPhysicalRank;
        uint8_t TotalNumOfLogicalRanks;

        uint8_t TotalNumOfDevices;

    } MemArchitect;

    struct AcTimingStruct
    {
        double tCCD_L_slr;
        double tCCD_L_WR_slr;
        double tCCD_L_WR2_slr;

        double tCCD_L_WTR_slr_part;
        double tCCD_M_slr;
        double tCCD_M_WR_slr;
        double tCCD_M_WTR_slr_part;
        unsigned tCCD_S_slr;
        unsigned tCCD_S_WR_slr;

        double tCCD_S_WTR_slr_part;

        double tRRD_L_slr;
        unsigned tRRD_S_slr;
        double tFAW_slr;
        double tRTP_slr;
        unsigned tPPD_slr;
        double tWR_slr;
        double tCCD_dlr;
        double tCCD_WR_dlr;

        double tCCD_WTR_dlr_part;
        double tRRD_dlr;
        double tFAW_dlr;
        unsigned tPPD_dlr;
        unsigned tCCD_WR_dpr;
        unsigned tDCAW;
        unsigned nDCAC;

    } AcTiming;

    struct RefreshAcTimingStruct {
        double tREFI1;
        double tREFI2;
        double tRFC1_slr;
        double tRFC2_slr;
        double tRFCsb_slr;
        double tRFC1_dlr;
        double tRFC1_dpr;
        double tRFC2_dlr;
        double tRFC2_dpr;
        double tRFCsb_dlr;
        double tREFSBRD_slr;
        double tREFSBRD_dlr;
        double tREFABRD_dlr;
    } RefreshAcTiming;

};

// struct MemSpecNo3ds: public DDR5MemSpecBase
// {
//     struct MemArchitectStruct
//     {
//         uint8_t NumOfDimmRanks;
//         uint8_t NumOfPhysicalRanksPerChannel;
//         uint8_t NumOfLogicalRanksPerPhysicalRank;
//         uint8_t TotalNumOfLogicalRanks;
//         uint8_t TotalNumOfDevices;

//     } MemArchitect;

//     struct AcTimingStruct
//     {
//         double    tCCD_L_slr;
//         double    tCCD_L_WR_slr;
//         double    tCCD_L_WR2_slr;

//         double    tCCD_L_WTR_slr_part;
//         double    tCCD_M_slr;
//         double    tCCD_M_WR_slr;
//         double    tCCD_M_WTR_slr_part;
//         unsigned  tCCD_S_slr;
//         unsigned  tCCD_S_WR_slr;

//         double    tCCD_S_WTR_slr_part;

//         double    tRRD_L_slr;
//         unsigned  tRRD_S_slr;
//         double    tFAW_slr;
//         double    tRTP_slr;
//         unsigned  tPPD_slr;
//         double    tWR_slr;
//         double    tCCD_dlr;
//         double    tCCD_WR_dlr;

//         double    tCCD_WTR_dlr_part;
//         double    tRRD_dlr;
//         double    tFAW_dlr;
//         unsigned  tPPD_dlr;
//         unsigned  tCCD_WR_dpr;
//         unsigned  tDCAW;
//         unsigned  nDCAC;

//     } AcTiming;

//     struct RefreshAcTimingStruct {
//         double tREFI1;
//         double tREFI2;
//         double tRFC1_slr;
//         double tRFC2_slr;
//         double tRFCsb_slr;
//         double tRFC1_dlr;
//         double tRFC1_dpr;
//         double tRFC2_dlr;
//         double tRFC2_dpr;
//         double tRFCsb_dlr;
//         double tREFSBRD_slr;
//         double tREFSBRD_dlr;
//         double tREFABRD_dlr;
//     } RefreshAcTiming;
// };

class LoadDDR5MemConfig : public LoadConfigFromJson
{
    public:
        DDR5MemConfig mem_spec;

        // ===== 为结构体定义JSON映射规则 =====
        BEGIN_JSON_MAP(DDR5MemConfig::DeviceStruct)
            JSON_FIELD(unsigned, width)
            JSON_FIELD(unsigned, DataRate)
            JSON_FIELD(unsigned, TotalNumOfBanksPerDevice)
            JSON_FIELD(unsigned, NumOfBankGroupsPerDevice)
            JSON_FIELD(unsigned, NumOfBanksPerBg)
            JSON_FIELD(unsigned, NumOfColumns)
            JSON_FIELD(unsigned, NumOfRows)
            JSON_FIELD(unsigned, RAAIMT)
            JSON_FIELD(unsigned, RAAMMT)
            JSON_FIELD(unsigned, RAADEC)
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::SpeedBinsStruct)
            JSON_FIELD(double, CLK)
            JSON_FIELD(unsigned, Freq)
            JSON_FIELD(unsigned, CL)
            JSON_FIELD(unsigned, nRCD)
            JSON_FIELD(unsigned, nRP)
            JSON_FIELD(unsigned, CWL)
            JSON_FIELD(double, tRCD)
            JSON_FIELD(double, tRP)
            JSON_FIELD(double, tRAS)
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::DataAcTimingStruct)
            JSON_FIELD(uint8_t, tRPRE)
            JSON_FIELD(float, tRPST)
            JSON_FIELD(uint8_t, tWPRE)
            JSON_FIELD(float, tWPST)
            JSON_FIELD(uint8_t, Read_DQS_Offset)
        END_JSON_MAP()

        // BEGIN_JSON_MAP(DDR5MemConfig)
        //     JSON_FIELD(std::string, MemoryId)
        //     JSON_FIELD(std::string, MemoryType)
        //     JSON_FIELD(bool, Is_3ds)
        //     JSON_FIELD(uint8_t, NumOfChannels)
        //     JSON_FIELD(uint8_t, NumOfSubChannels)
        //     JSON_FIELD(uint8_t, CmdMode)
        //     JSON_FIELD(uint8_t, FreqRatio)
        //     JSON_FIELD(uint8_t, RefreshMode)
        //     JSON_FIELD(uint8_t, BurstLenth)
        //     JSON_FIELD(uint8_t, MaxBurstLenth)
        //     JSON_NESTED_STRUCT(Device)
        //     JSON_NESTED_STRUCT(SpeedBins)
        //     JSON_NESTED_STRUCT(DataAcTiming)
        // END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::MemArchitectStruct)
            JSON_FIELD(uint8_t, NumOfDimmRanks)
            JSON_FIELD(uint8_t, NumOfPhysicalRanksPerChannel)
            JSON_FIELD(uint8_t, NumOfLogicalRanksPerPhysicalRank)
            JSON_FIELD(uint8_t, TotalNumOfLogicalRanks)
            JSON_FIELD(uint8_t, TotalNumOfDevices)
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::AcTimingStruct)
            JSON_FIELD(double, tCCD_L_slr)
            JSON_FIELD(double, tCCD_L_WR_slr)
            JSON_FIELD(double, tCCD_L_WR2_slr)
            JSON_FIELD(double, tCCD_L_WTR_slr_part)
            JSON_FIELD(double, tCCD_M_slr)
            JSON_FIELD(double, tCCD_M_WR_slr)
            JSON_FIELD(double, tCCD_M_WTR_slr_part)
            JSON_FIELD(unsigned, tCCD_S_slr)
            JSON_FIELD(unsigned, tCCD_S_WR_slr)
            JSON_FIELD(double, tCCD_S_WTR_slr_part)
            JSON_FIELD(double, tRRD_L_slr)
            JSON_FIELD(unsigned, tRRD_S_slr)
            JSON_FIELD(double, tFAW_slr)
            JSON_FIELD(double, tRTP_slr)
            JSON_FIELD(unsigned, tPPD_slr)
            JSON_FIELD(double, tWR_slr)
            JSON_FIELD(double, tCCD_dlr)
            JSON_FIELD(double, tCCD_WR_dlr)
            JSON_FIELD(double, tCCD_WTR_dlr_part)
            JSON_FIELD(double, tRRD_dlr)
            JSON_FIELD(double, tFAW_dlr)
            JSON_FIELD(unsigned, tPPD_dlr)
            JSON_FIELD(unsigned, tCCD_WR_dpr)
            JSON_FIELD(unsigned, tDCAW)
            JSON_FIELD(unsigned, nDCAC)
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig::RefreshAcTimingStruct)
            JSON_FIELD(double, tREFI1)
            JSON_FIELD(double, tREFI2)
            JSON_FIELD(double, tRFC1_slr)
            JSON_FIELD(double, tRFC2_slr)
            JSON_FIELD(double, tRFCsb_slr)
            JSON_FIELD(double, tRFC1_dlr)
            JSON_FIELD(double, tRFC1_dpr)
            JSON_FIELD(double, tRFC2_dlr)
            JSON_FIELD(double, tRFC2_dpr)
            JSON_FIELD(double, tRFCsb_dlr)
            JSON_FIELD(double, tREFSBRD_slr)
            JSON_FIELD(double, tREFSBRD_dlr)
            JSON_FIELD(double, tREFABRD_dlr)
        END_JSON_MAP()

        BEGIN_JSON_MAP(DDR5MemConfig)
            // 先解析基类字段
            JSON_FIELD(std::string, MemoryId)
            JSON_FIELD(std::string, MemoryType)
            JSON_FIELD(bool, Is_3ds)
            JSON_FIELD(uint8_t, NumOfChannels)
            JSON_FIELD(uint8_t, NumOfSubChannels)
            JSON_FIELD(uint8_t, CmdMode)
            JSON_FIELD(uint8_t, FreqRatio)
            JSON_FIELD(uint8_t, RefreshMode)
            JSON_FIELD(uint8_t, BurstLenth)
            JSON_FIELD(uint8_t, MaxBurstLenth)
            JSON_NESTED_STRUCT(Device)
            JSON_NESTED_STRUCT(SpeedBins)
            JSON_NESTED_STRUCT(DataAcTiming)

            // 再解析派生类字段
            JSON_NESTED_STRUCT(MemArchitect)
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

            assert(mem_spec.MemArchitect.TotalNumOfLogicalRanks == mem_spec.MemArchitect.NumOfLogicalRanksPerPhysicalRank *
                   mem_spec.MemArchitect.NumOfPhysicalRanksPerChannel);

            return parse_result;
        }

};

} // namespace dmu

#endif