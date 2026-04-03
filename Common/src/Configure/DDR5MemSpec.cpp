#include "Configure/DDR5MemSpec.hh"
#include "Configure/LoadMemSpec.hh"


namespace dmu{
    namespace Controller{
    using namespace sc_core;
    DDR5MemSpec::DDR5MemSpec(const DDR5MemConfig& mem_spec_base)
    :memory_id(mem_spec_base.MemoryId)
    ,memory_type(mem_spec_base.MemoryType)
    ,Is_3DS(mem_spec_base.Is_3ds)

    ,NumOfChannels(mem_spec_base.NumOfChannels)
    ,NumOfSubChannels(mem_spec_base.NumOfSubChannels)

    ,TotalNumOfBanksPerDevice(mem_spec_base.Device.TotalNumOfBanksPerDevice)
    ,NumOfBankGroupsPerDevice(mem_spec_base.Device.NumOfBankGroupsPerDevice)
    ,NumOfRows(mem_spec_base.Device.NumOfRows)
    ,NumOfColumns(mem_spec_base.Device.NumOfColumns)
    ,NumOfBanksPerBg(mem_spec_base.Device.NumOfBanksPerBg)
    ,RAAIMT(mem_spec_base.Device.RAAIMT)
    ,RAAMMT(mem_spec_base.Device.RAAMMT)
    ,RAADEC(mem_spec_base.Device.RAADEC)
    ,width(mem_spec_base.Device.width)
    ,DataRate(mem_spec_base.Device.DataRate)

    ,FreqRatio(mem_spec_base.FreqRatio)
    ,BurstLenth(mem_spec_base.BurstLenth)
    ,maxBurstLenth(mem_spec_base.MaxBurstLenth)

    ,fCKMHz(mem_spec_base.SpeedBins.Freq)
    ,tCK(sc_time(mem_spec_base.SpeedBins.CLK,SC_NS))
    ,tCL(mem_spec_base.SpeedBins.CL * tCK)
    ,tCWL(mem_spec_base.SpeedBins.CWL *tCK)
    ,tRCD(sc_time(mem_spec_base.SpeedBins.tRCD,SC_NS))
    ,tRP(sc_time(mem_spec_base.SpeedBins.tRP,SC_NS))
    ,tRASmin(sc_time(mem_spec_base.SpeedBins.tRAS,SC_NS))
    ,nRCD(mem_spec_base.SpeedBins.nRCD)
    ,nRP(mem_spec_base.SpeedBins.nRP)
    ,CL(mem_spec_base.SpeedBins.CL)
    ,CWL(mem_spec_base.SpeedBins.CWL)


    ,tRPRE(mem_spec_base.DataAcTiming.tRPRE * tCK)
    ,tRPST(mem_spec_base.DataAcTiming.tRPST * tCK)
    ,tWPRE(mem_spec_base.DataAcTiming.tWPRE * tCK)
    ,tWPST(mem_spec_base.DataAcTiming.tWPST * tCK)
    ,Read_DQS_Offset(mem_spec_base.DataAcTiming.Read_DQS_Offset * tCK)

    ,nRPRE(mem_spec_base.DataAcTiming.tRPRE)
    ,nRPST(mem_spec_base.DataAcTiming.tRPST)
    ,nWPRE(mem_spec_base.DataAcTiming.tWPRE)
    ,nWPST(mem_spec_base.DataAcTiming.tWPST)
    ,nRead_DQS_Offset(mem_spec_base.DataAcTiming.Read_DQS_Offset)

    ,tBurst((static_cast<double>(BurstLenth)/DataRate) * tCK)


    ,tCK_mc(sc_time(static_cast<double>(1000.0/mem_spec_base.SpeedBins.Freq * mem_spec_base.FreqRatio),SC_NS))
    ,tRCD_mc(Tranform2McClk(mem_spec_base.SpeedBins.nRCD,mem_spec_base.FreqRatio) * tCK_mc)
    ,tRASmin_mc(Tranform2McClk(AcTimingRounding(mem_spec_base.SpeedBins.tRAS, mem_spec_base.SpeedBins.CLK, true),
                               mem_spec_base.FreqRatio) * tCK_mc)
    ,tRP_mc(Tranform2McClk(mem_spec_base.SpeedBins.nRP,mem_spec_base.FreqRatio) * tCK_mc)
    ,tCL_mc(Tranform2McClk(mem_spec_base.SpeedBins.CL,mem_spec_base.FreqRatio) * tCK_mc)
    ,tCWL_mc(Tranform2McClk(mem_spec_base.SpeedBins.CWL,mem_spec_base.FreqRatio) * tCK_mc)


    {
        if(mem_spec_base.CmdMode == 0)
        {
            cmd_mode = CmdMode::cmd_1_N;
        }
        else if(mem_spec_base.CmdMode == 1){
            cmd_mode = CmdMode::cmd_2_N;
        }
        else
        {
            std::cerr << " Invalid cmd Mode in mem spec: "<< mem_spec_base.CmdMode << std::endl;
            std::abort();
        }

        if(mem_spec_base.RefreshMode == 0)
        {
            RefMode = RefModeTypeDDR5::Normal;
        }
        else if(mem_spec_base.RefreshMode == 1)
        {
            RefMode = RefModeTypeDDR5::FGR;
        }
        else
        {
            std::cerr << " Invalid refresh Mode in mem spec: "<< mem_spec_base.RefreshMode << std::endl;
            std::abort();
        }

    }



    }
}