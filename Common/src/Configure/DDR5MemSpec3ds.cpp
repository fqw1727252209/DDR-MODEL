#include <iostream>
#include <string_view>

#include "Configure/DDR5MemSpec3ds.hh"

namespace dmu{
    namespace Controller{
using namespace sc_core;
DDR5MemSpec3ds::DDR5MemSpec3ds(const DDR5MemConfig& mem_spec, const std::string& output_dir)
:DDR5MemSpec(mem_spec)
// ,NumOfPhysicalRanksPerSubChannel (mem_spec.MemArchitect.NumOfPhysicalRanksPerSubChannel)
// ,NumOfLogicalRanksPerPhysicalRank (mem_spec.MemArchitect.NumOfLogicalRanksPerPhysicalRank)
// ,TotalNumOfLogicalRanks (mem_spec.MemArchitect.TotalNumOfLogicalRanks)
// ,TotalNumOfDevices (mem_spec.MemArchitect.TotalNumOfDevices)

,Wr2WrPrank (mem_spec.RankSwitchDelay.Wr2WrPrank * tCK_mc)
,Wr2RdPrank (mem_spec.RankSwitchDelay.Wr2RdPrank * tCK_mc)
,Rd2RdPrank (mem_spec.RankSwitchDelay.Rd2RdPrank * tCK_mc)
,Rd2WrPrank (mem_spec.RankSwitchDelay.Rd2WrPrank * tCK_mc)
,Wr2WrLrank (mem_spec.RankSwitchDelay.Wr2WrLrank * tCK_mc)
,Wr2RdLrank (mem_spec.RankSwitchDelay.Wr2RdLrank * tCK_mc)
,Rd2RdLrank (mem_spec.RankSwitchDelay.Rd2RdLrank * tCK_mc)
,Rd2WrLrank (mem_spec.RankSwitchDelay.Rd2WrLrank * tCK_mc)

,tCCD_L_slr(sc_time(mem_spec.AcTiming.tCCD_L_slr,SC_NS))
,tCCD_L_WR_slr(sc_time(mem_spec.AcTiming.tCCD_L_WR_slr,SC_NS))
,tCCD_L_WR2_slr(sc_time(mem_spec.AcTiming.tCCD_L_WR2_slr,SC_NS))
,tCCD_L_RTW_slr(tCL - tCWL + tBurst + 2 * tCK - Read_DQS_Offset + tRPST - 0.5 * tCK + tWPRE)
,tCCD_L_WTR_slr(tCWL + tBurst + sc_time(mem_spec.AcTiming.tCCD_L_WTR_slr_part,SC_NS))
,tCCD_M_slr(sc_time(mem_spec.AcTiming.tCCD_M_slr,SC_NS))
,tCCD_M_WR_slr(sc_time(mem_spec.AcTiming.tCCD_M_WR_slr,SC_NS))
,tCCD_M_WTR_slr(tCWL + tBurst + sc_time(mem_spec.AcTiming.tCCD_M_WTR_slr_part,SC_NS))
,tCCD_S_slr(mem_spec.AcTiming.tCCD_S_slr * tCK)
,tCCD_S_WR_slr(mem_spec.AcTiming.tCCD_S_WR_slr * tCK)
,tCCD_S_RTW_slr(tCL - tCWL + tBurst + 2 * tCK - Read_DQS_Offset + tRPST - 0.5 * tCK + tWPRE)
,tCCD_S_WTR_slr(tCWL + tBurst + sc_time(mem_spec.AcTiming.tCCD_S_WTR_slr_part,SC_NS))
,tCCD_WTRA_slr(tCWL + tBurst + sc_time(mem_spec.AcTiming.tWR_slr,SC_NS) - sc_time(mem_spec.AcTiming.tRTP_slr,SC_NS))//
,tRRD_L_slr(sc_time(mem_spec.AcTiming.tRRD_L_slr,SC_NS))
,tRRD_S_slr(mem_spec.AcTiming.tRRD_S_slr * tCK)
,tFAW_slr(sc_time(mem_spec.AcTiming.tFAW_slr,SC_NS))
,tRTP_slr(sc_time(mem_spec.AcTiming.tRTP_slr,SC_NS))
,tPPD_slr(mem_spec.AcTiming.tPPD_slr * tCK)
,tWR_slr(sc_time(mem_spec.AcTiming.tWR_slr,SC_NS))
,tCCD_dlr(sc_time(mem_spec.AcTiming.tCCD_dlr,SC_NS))
,tCCD_WR_dlr(sc_time(mem_spec.AcTiming.tCCD_WR_dlr,SC_NS))
,tCCD_RTW_dlr(tCL - tCWL + tBurst + 2 * tCK - Read_DQS_Offset + tRPST - 0.5 * tCK + tWPRE)
,tCCD_WTR_dlr(tCWL + tBurst + sc_time(mem_spec.AcTiming.tCCD_WTR_dlr_part,SC_NS))
,tRRD_dlr(sc_time(mem_spec.AcTiming.tRRD_dlr,SC_NS))
,tFAW_dlr(sc_time(mem_spec.AcTiming.tFAW_dlr,SC_NS))
,tPPD_dlr(mem_spec.AcTiming.tPPD_dlr * tCK)
,tCCD_WR_dpr(mem_spec.AcTiming.tCCD_WR_dpr * tCK)
,tDCAW(mem_spec.AcTiming.tDCAW * tCK)
,nDCAC(mem_spec.AcTiming.nDCAC)

,tREFI1(sc_time(mem_spec.RefreshAcTiming.tREFI1, SC_NS))
,tREFI2(sc_time(mem_spec.RefreshAcTiming.tREFI2, SC_NS))
,tRFC1_slr(sc_time(mem_spec.RefreshAcTiming.tRFC1_slr, SC_NS))
,tRFC2_slr(sc_time(mem_spec.RefreshAcTiming.tRFC2_slr, SC_NS))
,tRFCsb_slr(sc_time(mem_spec.RefreshAcTiming.tRFCsb_slr, SC_NS))
,tRFC1_dlr(sc_time(mem_spec.RefreshAcTiming.tRFC1_dlr, SC_NS))
,tRFC1_dpr(sc_time(mem_spec.RefreshAcTiming.tRFC1_dpr, SC_NS))
,tRFC2_dlr(sc_time(mem_spec.RefreshAcTiming.tRFC2_dlr, SC_NS))
,tRFC2_dpr(sc_time(mem_spec.RefreshAcTiming.tRFC2_dpr, SC_NS))
,tRFCsb_dlr(sc_time(mem_spec.RefreshAcTiming.tRFCsb_dlr, SC_NS))
,tREFSBRD_slr(sc_time(mem_spec.RefreshAcTiming.tREFSBRD_slr, SC_NS))
,tREFSBRD_dlr(sc_time(mem_spec.RefreshAcTiming.tREFSBRD_dlr, SC_NS))
,tREFABRD_dlr(sc_time(mem_spec.RefreshAcTiming.tREFABRD_dlr, SC_NS))

//MC clock domain
,tCCD_L_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_L_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_L_WR_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_L_WR_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_L_WR2_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_L_WR2_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_L_RTW_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_L_RTW_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_L_WTR_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_L_WTR_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_M_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_M_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_M_WR_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_M_WR_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_M_WTR_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_M_WTR_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_S_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_S_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_S_WR_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_S_WR_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_S_RTW_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_S_RTW_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_S_WTR_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_S_WTR_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_WTRA_slr_mc(Tranform2McClk(AcTimingRounding(tCCD_WTRA_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRRD_L_slr_mc(Tranform2McClk(AcTimingRounding(tRRD_L_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRRD_S_slr_mc(Tranform2McClk(AcTimingRounding(tRRD_S_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tFAW_slr_mc(Tranform2McClk(AcTimingRounding(tFAW_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRTP_slr_mc(Tranform2McClk(AcTimingRounding(tRTP_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tPPD_slr_mc(Tranform2McClk(AcTimingRounding(tPPD_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tWR_slr_mc(Tranform2McClk(AcTimingRounding(tWR_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_dlr_mc(Tranform2McClk(AcTimingRounding(tCCD_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_WR_dlr_mc(Tranform2McClk(AcTimingRounding(tCCD_WR_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_RTW_dlr_mc(Tranform2McClk(AcTimingRounding(tCCD_RTW_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_WTR_dlr_mc(Tranform2McClk(AcTimingRounding(tCCD_WTR_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRRD_dlr_mc(Tranform2McClk(AcTimingRounding(tRRD_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tFAW_dlr_mc(Tranform2McClk(AcTimingRounding(tFAW_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tPPD_dlr_mc(Tranform2McClk(AcTimingRounding(tPPD_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tCCD_WR_dpr_mc(Tranform2McClk(AcTimingRounding(tCCD_WR_dpr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tDCAW_mc(Tranform2McClk(AcTimingRounding(tDCAW.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)

,tBurst_mc(Tranform2McClk(AcTimingRounding(tBurst.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)

,tREFI1_mc(Tranform2McClk(AcTimingRounding(tREFI1.to_double(),tCK.to_double(),false),FreqRatio) * tCK_mc)
,tREFI2_mc(Tranform2McClk(AcTimingRounding(tREFI2.to_double(),tCK.to_double(),false),FreqRatio) * tCK_mc)
,tRFC1_slr_mc(Tranform2McClk(AcTimingRounding(tRFC1_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRFC2_slr_mc(Tranform2McClk(AcTimingRounding(tRFC2_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRFCsb_slr_mc(Tranform2McClk(AcTimingRounding(tRFCsb_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRFC1_dlr_mc(Tranform2McClk(AcTimingRounding(tRFC1_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRFC1_dpr_mc(Tranform2McClk(AcTimingRounding(tRFC1_dpr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRFC2_dlr_mc(Tranform2McClk(AcTimingRounding(tRFC2_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRFC2_dpr_mc(Tranform2McClk(AcTimingRounding(tRFC2_dpr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tRFCsb_dlr_mc(Tranform2McClk(AcTimingRounding(tRFCsb_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tREFSBRD_slr_mc(Tranform2McClk(AcTimingRounding(tREFSBRD_slr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tREFSBRD_dlr_mc(Tranform2McClk(AcTimingRounding(tREFSBRD_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)
,tREFABRD_dlr_mc(Tranform2McClk(AcTimingRounding(tREFABRD_dlr.to_double(),tCK.to_double(),true),FreqRatio) * tCK_mc)

{

    if (RefMode == RefModeTypeDDR5::Normal)
    {
        tREFI_mc = tREFI1_mc;
        tRFC_slr_mc = tRFC1_slr_mc;
        tRFC_dlr_mc = tRFC1_dlr_mc;
        tRFC_dpr_mc = tRFC1_dpr_mc;
    }
    else if (RefMode == RefModeTypeDDR5::FGR)
    {
        tREFI_mc = tREFI2_mc;
        tRFC_slr_mc = tRFC2_slr_mc;
        tRFC_dlr_mc = tRFC2_dlr_mc;
        tRFC_dpr_mc = tRFC2_dpr_mc;
    }
    else
    {
        std::cerr<<"Sdram Constraint use invalid RefMode"<<std::endl;
        std::abort();
    }

    DeviceMemorySizeBytes = static_cast<uint64_t>(DeviceWidth /8 *  NumOfColumns * NumOfRows * NumOfBanksPerDevice);
    MemorySizeBytes = NumOfDevicesPerChannel * DeviceMemorySizeBytes;

    static constexpr std::string_view DOT_LINE = "\n---------------------------------------------------------------------------------------------------------";
    // 输出所有成员变量到文件
    std::ofstream outFile(output_dir+ "/" +"DDR5MemSpec.txt");
    if (!outFile.is_open()) {
        outFile.open(output_dir+ "/" +"DDR5MemSpec.txt", std::ios::out | std::ios::trunc);
    }

    if (outFile.is_open()) {
        
        outFile << " Memory Configuration:"<<std::endl;
        outFile << " Memory type:                         " << (Is_3DS ? "DDR5 3DS" : "DDR5")<< std::endl;
        outFile << " Memory size in Bytes:                " << MemorySizeBytes << std::endl;
        outFile << " Memory size in GB:                   " << (MemorySizeBytes>>30) << std::endl;
        outFile << DOT_LINE << std::endl;
        outFile << " Sub Channel Configuration:           "<<std::endl;
        outFile << " Pseudo Channels Per Sub-Ch:          " << NumOfPseudoChannelsPerSubChannel << std::endl;
        outFile << " Physical Ranks Per Sub-Ch:           " << NumOfPhysicalRanksPerSubChannel << std::endl;
        outFile << " Logical Ranks Per Sub-Ch:            " << NumOfLogicalRanksPerSubChannel << std::endl;
        outFile << " Total Bank Groups Per Sub-Ch:        " << NumOfBankGroupsPerSubChannel << std::endl;
        outFile << " Total Banks Per Sub-Ch:              " << NumOfBanksPerSubChannel << std::endl;
        outFile << DOT_LINE << std::endl;
        outFile << " Pseudo Channels Configuration:       "<<std::endl;
        outFile << " Physical Ranks Per P-Ch:             " << NumOfPhysicalRanksPerPseudoChannel << std::endl;
        outFile << " Logical Ranks Per P-Ch:              " << NumOfLogicalRanksPerPseudoChannel << std::endl;
        outFile << " Total Bank Groups Per P-Ch:          " << NumOfBankGroupsPerPseudoChannel << std::endl;
        outFile << " Total Banks Per P-Ch:                " << NumOfBanksPerPseudoChannel << std::endl;
        outFile << DOT_LINE << std::endl;
        outFile << " Rank & Device Configuration:         "<<std::endl;
        outFile << " Logical Ranks Per P-Rank:            " << NumOfLogicalRanksPerPhysicalRank << std::endl;
        outFile << " Devices Per L-Rank:                  " << NumOfDevicesPerLogicalRank << std::endl;
        outFile << " Bank Groups Per L-Rank:              " << NumOfBankGroupsPerLogicalRank << std::endl;
        outFile << " Banks Per L-Rank:                    " << NumOfBanksPerLogicalRank << std::endl;
        outFile << " Rows Per Bank:                       " << NumOfRows << std::endl;
        outFile << " Columns Per Row:                     " << NumOfColumns << std::endl;
        outFile << " Device Width in Bits:                " << DeviceWidth << std::endl;
        outFile << " Devices Per L-Rank:                  " << NumOfDevicesPerLogicalRank << std::endl;
        outFile << " Device Size in Bytes:                " << DeviceMemorySizeBytes << std::endl;
        outFile << " Device Size in GB:                   " << static_cast<double>(DeviceMemorySizeBytes>>30) << std::endl;
        outFile << DOT_LINE<<std::endl;
        outFile << "--------DDR Speed: " << fCKMHz*2 << " MT/s," << " MC Freq: " << static_cast<double>(fCKMHz)/FreqRatio << " MHz--------"<<std::endl;

#define OutputAcTiming(DDR_TIMING,MC_TIMING,DDR_TIMING_CLK,MC_TIMING_CLK) \
        outFile << std::left<<"[DDR] "<<std::setw(18)<< #DDR_TIMING << ": " \
        <<std::setw(12)<<DDR_TIMING.to_seconds()*1e9 <<" (ns) --> " \
        <<std::setw(8)<<DDR_TIMING_CLK <<" (nCK) --> " \
        <<"[MC] "<<std::setw(24)<< #MC_TIMING<< ": " \
        <<std::setw(8)<<MC_TIMING_CLK <<" (nMC_CK) --> " \
        <<std::setw(12)<<MC_TIMING.to_seconds()*1e9 <<" (ns)" \
        <<std::endl;

        OutputAcTiming(tCK,tCK_mc,1,1)
        OutputAcTiming(tRCD,tRCD_mc,nRCD,tRCD_mc/tCK_mc)
        OutputAcTiming(tRASmin,tRASmin_mc,AcTimingRounding(tRASmin.to_double(),tCK.to_double(),true),tRASmin_mc/tCK_mc)
        OutputAcTiming(tRP,tRP_mc,nRP,tRP_mc/tCK_mc)
        OutputAcTiming(tCL,tCL_mc,CL,tCL_mc/tCK_mc)
        OutputAcTiming(tCWL,tCWL_mc,CWL,tCWL_mc/tCK_mc)

        OutputAcTiming( tCCD_L_slr,tCCD_L_slr_mc,AcTimingRounding(tCCD_L_slr.to_double(),tCK.to_double(),true),tCCD_L_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_L_WR_slr,tCCD_L_WR_slr_mc,AcTimingRounding(tCCD_L_WR_slr.to_double(),tCK.to_double(),true),tCCD_L_WR_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_L_WR2_slr,tCCD_L_WR2_slr_mc,AcTimingRounding(tCCD_L_WR2_slr.to_double(),tCK.to_double(),true),tCCD_L_WR2_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_L_RTW_slr,tCCD_L_RTW_slr_mc,AcTimingRounding(tCCD_L_RTW_slr.to_double(),tCK.to_double(),true),tCCD_L_RTW_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_L_WTR_slr,tCCD_L_WTR_slr_mc,AcTimingRounding(tCCD_L_WTR_slr.to_double(),tCK.to_double(),true),tCCD_L_WTR_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_M_slr,tCCD_M_slr_mc,AcTimingRounding(tCCD_M_slr.to_double(),tCK.to_double(),true),tCCD_M_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_M_WR_slr,tCCD_M_WR_slr_mc,AcTimingRounding(tCCD_M_WR_slr.to_double(),tCK.to_double(),true),tCCD_M_WR_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_M_WTR_slr,tCCD_M_WTR_slr_mc,AcTimingRounding(tCCD_M_WTR_slr.to_double(),tCK.to_double(),true),tCCD_M_WTR_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_S_slr,tCCD_S_slr_mc,AcTimingRounding(tCCD_S_slr.to_double(),tCK.to_double(),true),tCCD_S_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_S_WR_slr,tCCD_S_WR_slr_mc,AcTimingRounding(tCCD_S_WR_slr.to_double(),tCK.to_double(),true),tCCD_S_WR_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_S_RTW_slr,tCCD_S_RTW_slr_mc,AcTimingRounding(tCCD_S_RTW_slr.to_double(),tCK.to_double(),true),tCCD_S_RTW_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_S_WTR_slr,tCCD_S_WTR_slr_mc,AcTimingRounding(tCCD_S_WTR_slr.to_double(),tCK.to_double(),true),tCCD_S_WTR_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_WTRA_slr,tCCD_WTRA_slr_mc,AcTimingRounding(tCCD_WTRA_slr.to_double(),tCK.to_double(),true),tCCD_WTRA_slr_mc/tCK_mc)
        OutputAcTiming( tRRD_L_slr,tRRD_L_slr_mc,AcTimingRounding(tRRD_L_slr.to_double(),tCK.to_double(),true),tRRD_L_slr_mc/tCK_mc)
        OutputAcTiming( tRRD_S_slr,tRRD_S_slr_mc,AcTimingRounding(tRRD_S_slr.to_double(),tCK.to_double(),true),tRRD_S_slr_mc/tCK_mc)
        OutputAcTiming( tFAW_slr,tFAW_slr_mc,AcTimingRounding(tFAW_slr.to_double(),tCK.to_double(),true),tFAW_slr_mc/tCK_mc)
        OutputAcTiming( tRTP_slr,tRTP_slr_mc,AcTimingRounding(tRTP_slr.to_double(),tCK.to_double(),true),tRTP_slr_mc/tCK_mc)
        OutputAcTiming( tPPD_slr,tPPD_slr_mc,AcTimingRounding(tPPD_slr.to_double(),tCK.to_double(),true),tPPD_slr_mc/tCK_mc)
        OutputAcTiming( tWR_slr,tWR_slr_mc,AcTimingRounding(tWR_slr.to_double(),tCK.to_double(),true),tWR_slr_mc/tCK_mc)
        OutputAcTiming( tCCD_dlr,tCCD_dlr_mc,AcTimingRounding(tCCD_dlr.to_double(),tCK.to_double(),true),tCCD_dlr_mc/tCK_mc)
        OutputAcTiming( tCCD_WR_dlr,tCCD_WR_dlr_mc,AcTimingRounding(tCCD_WR_dlr.to_double(),tCK.to_double(),true),tCCD_WR_dlr_mc/tCK_mc)
        OutputAcTiming( tCCD_RTW_dlr,tCCD_RTW_dlr_mc,AcTimingRounding(tCCD_RTW_dlr.to_double(),tCK.to_double(),true),tCCD_RTW_dlr_mc/tCK_mc)
        OutputAcTiming( tCCD_WTR_dlr,tCCD_WTR_dlr_mc,AcTimingRounding(tCCD_WTR_dlr.to_double(),tCK.to_double(),true),tCCD_WTR_dlr_mc/tCK_mc)
        OutputAcTiming( tRRD_dlr,tRRD_dlr_mc,AcTimingRounding(tRRD_dlr.to_double(),tCK.to_double(),true),tRRD_dlr_mc/tCK_mc)
        OutputAcTiming( tFAW_dlr,tFAW_dlr_mc,AcTimingRounding(tFAW_dlr.to_double(),tCK.to_double(),true),tFAW_dlr_mc/tCK_mc)
        OutputAcTiming( tPPD_dlr,tPPD_dlr_mc,AcTimingRounding(tPPD_dlr.to_double(),tCK.to_double(),true),tPPD_dlr_mc/tCK_mc)
        OutputAcTiming( tCCD_WR_dpr,tCCD_WR_dpr_mc,AcTimingRounding(tCCD_WR_dpr.to_double(),tCK.to_double(),true),tCCD_WR_dpr_mc/tCK_mc)
        OutputAcTiming( tDCAW,tDCAW_mc,AcTimingRounding(tDCAW.to_double(),tCK.to_double(),true),tDCAW_mc/tCK_mc)

        OutputAcTiming( tBurst,tBurst_mc,AcTimingRounding(tBurst.to_double(),tCK.to_double(),true),tBurst_mc / tCK_mc)

        OutputAcTiming( tREFI1,tREFI1_mc,  AcTimingRounding(tREFI1.to_double(),tCK.to_double(),false),   tREFI1_mc / tCK_mc)
        OutputAcTiming( tREFI2,tREFI2_mc,  AcTimingRounding(tREFI2.to_double(),tCK.to_double(),false),   tREFI2_mc / tCK_mc)
        OutputAcTiming( tRFC1_slr,tRFC1_slr_mc, AcTimingRounding(tRFC1_slr.to_double(),tCK.to_double(),true),      tRFC1_slr_mc / tCK_mc)
        OutputAcTiming( tRFC2_slr,tRFC2_slr_mc, AcTimingRounding(tRFC2_slr.to_double(),tCK.to_double(),true),      tRFC2_slr_mc / tCK_mc)
        OutputAcTiming( tRFCsb_slr,tRFCsb_slr_mc, AcTimingRounding(tRFCsb_slr.to_double(),tCK.to_double(),true),     tRFCsb_slr_mc / tCK_mc)
        OutputAcTiming( tRFC1_dlr,tRFC1_dlr_mc, AcTimingRounding(tRFC1_dlr.to_double(),tCK.to_double(),true),      tRFC1_dlr_mc / tCK_mc)
        OutputAcTiming( tRFC1_dpr,tRFC1_dpr_mc, AcTimingRounding(tRFC1_dpr.to_double(),tCK.to_double(),true),      tRFC1_dpr_mc / tCK_mc)
        OutputAcTiming( tRFC2_dlr,tRFC2_dlr_mc, AcTimingRounding(tRFC2_dlr.to_double(),tCK.to_double(),true),      tRFC2_dlr_mc / tCK_mc)
        OutputAcTiming( tRFC2_dpr,tRFC2_dpr_mc, AcTimingRounding(tRFC2_dpr.to_double(),tCK.to_double(),true),      tRFC2_dpr_mc / tCK_mc)
        OutputAcTiming( tRFCsb_dlr,tRFCsb_dlr_mc, AcTimingRounding(tRFCsb_dlr.to_double(),tCK.to_double(),true),     tRFCsb_dlr_mc / tCK_mc)
        OutputAcTiming( tREFSBRD_slr,tREFSBRD_slr_mc, AcTimingRounding(tREFSBRD_slr.to_double(),tCK.to_double(),true), tREFSBRD_slr_mc / tCK_mc)
        OutputAcTiming( tREFSBRD_dlr,tREFSBRD_dlr_mc, AcTimingRounding(tREFSBRD_dlr.to_double(),tCK.to_double(),true), tREFSBRD_dlr_mc / tCK_mc)
        OutputAcTiming( tREFABRD_dlr,tREFABRD_dlr_mc, AcTimingRounding(tREFABRD_dlr.to_double(),tCK.to_double(),true), tREFABRD_dlr_mc / tCK_mc)

        OutputAcTiming( (Wr2WrPrank   * FreqRatio), Wr2WrPrank,  (Wr2WrPrank/tCK_mc   * FreqRatio), Wr2WrPrank/tCK_mc)
        OutputAcTiming( (Wr2RdPrank   * FreqRatio), Wr2RdPrank,  (Wr2RdPrank/tCK_mc   * FreqRatio), Wr2RdPrank/tCK_mc)
        OutputAcTiming( (Rd2RdPrank   * FreqRatio), Rd2RdPrank,  (Rd2RdPrank/tCK_mc   * FreqRatio), Rd2RdPrank/tCK_mc)
        OutputAcTiming( (Rd2WrPrank   * FreqRatio), Rd2WrPrank,  (Rd2WrPrank/tCK_mc   * FreqRatio), Rd2WrPrank/tCK_mc)
        OutputAcTiming( (Wr2WrLrank   * FreqRatio), Wr2WrLrank,  (Wr2WrLrank/tCK_mc   * FreqRatio), Wr2WrLrank/tCK_mc)
        OutputAcTiming( (Wr2RdLrank   * FreqRatio), Wr2RdLrank,  (Wr2RdLrank/tCK_mc   * FreqRatio), Wr2RdLrank/tCK_mc)
        OutputAcTiming( (Rd2RdLrank   * FreqRatio), Rd2RdLrank,  (Rd2RdLrank/tCK_mc   * FreqRatio), Rd2RdLrank/tCK_mc)
        OutputAcTiming( (Rd2WrLrank   * FreqRatio), Rd2WrLrank,  (Rd2WrLrank/tCK_mc   * FreqRatio), Rd2WrLrank/tCK_mc)

        outFile.close();
    }
}
    }
}