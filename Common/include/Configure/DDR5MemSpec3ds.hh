#ifndef __DDR5_MEM_SPEC_3DS_HH__
#define __DDR5_MEM_SPEC_3DS_HH__

#include <systemc>

#include "Configure/DDR5MemSpec.hh"

namespace dmu {
namespace Controller {

class DDR5MemSpec3ds final : public DDR5MemSpec {
public:
  // RankSwitchDelay using tck_mc
  const sc_core::sc_time Wr2WrPrank;
  const sc_core::sc_time Wr2RdPrank;
  const sc_core::sc_time Rd2RdPrank;
  const sc_core::sc_time Rd2WrPrank;
  const sc_core::sc_time Wr2WrLrank;
  const sc_core::sc_time Wr2RdLrank;
  const sc_core::sc_time Rd2RdLrank;
  const sc_core::sc_time Rd2WrLrank;

  // Ac Timing
  const sc_core::sc_time tCCD_L_slr; // Read to Read command delay for same bank
                                     // group in same logical rank
  const sc_core::sc_time tCCD_L_WR_slr; // Write to Write command delay for same
                                        // bank group in same logical rank
  const sc_core::sc_time
      tCCD_L_WR2_slr; // Write to Write command delay for same bank group in
                      // same logical rank, second write not RMW
  const sc_core::sc_time tCCD_L_RTW_slr; // Read to Write command delay for same
                                         // bank group in same logical rank
  const sc_core::sc_time
      tCCD_L_WTR_slr; // not part// Write to Read command delay for same bank
                      // group in same logical rank
  const sc_core::sc_time tCCD_M_slr; // Read to Read command delay for different
                                     // bank in same bank group
  const sc_core::sc_time tCCD_M_WR_slr; // Write to Write command delay for
                                        // different bank in same bank group
  const sc_core::sc_time
      tCCD_M_WTR_slr; // not part// Write to Read command delay for different
                      // bank in same bank group
  const sc_core::sc_time tCCD_S_slr; // Read to Read command delay for different
                                     // bank group in same logical rank
  const sc_core::sc_time
      tCCD_S_WR_slr; // Write to Write command delay for different bank group in
                     // same logical rank
  const sc_core::sc_time
      tCCD_S_RTW_slr; // Read to Write command delay for different bank group in
                      // same logical rank
  const sc_core::sc_time
      tCCD_S_WTR_slr; // not part  // Write to Read command delay for different
                      // bank group in same logical rank
  const sc_core::sc_time
      tCCD_WTRA_slr; // Write to Read with Auto Precharge command for same bank
                     // in same logic rank
  const sc_core::sc_time tRRD_L_slr; // Activate to Activate command delay to
                                     // same bank group in the same logical rank
  const sc_core::sc_time
      tRRD_S_slr; // Activate to Activate command delay to different bank group
                  // in the same logical rank
  const sc_core::sc_time
      tFAW_slr; // Four activate window to the same logical rank  // only for x4
  const sc_core::sc_time
      tRTP_slr; // Read command to Precharge command delay in same logical rank
  const sc_core::sc_time
      tPPD_slr; // Precharge to Precharge delay in same logical rank
  const sc_core::sc_time tWR_slr; // Write recovery time in same logical rank
  const sc_core::sc_time
      tCCD_dlr; // Read to Read command delay in different logical ranks
  const sc_core::sc_time
      tCCD_WR_dlr; // Write to Write command delay in different logical ranks
  const sc_core::sc_time
      tCCD_RTW_dlr; // Read to Write command delay in different logical ranks
  const sc_core::sc_time tCCD_WTR_dlr; // not part // Write to Read command
                                       // delay in different logical ranks
  const sc_core::sc_time
      tRRD_dlr; // Activate to Activate command delay to different logical ranks
  const sc_core::sc_time
      tFAW_dlr; // Four activate window to different logical ranks
  const sc_core::sc_time
      tPPD_dlr; // Precharge to Precharge delay in different logical rank
  const sc_core::sc_time tCCD_WR_dpr; // Minimum Write to Write command delay in
                                      // different 3DS or DDP physical ranks
  const sc_core::sc_time tDCAW;       // Activate window by DIMM channel
  const unsigned nDCAC; // DIMM Channel Activate Command Count in tDCAW

  // refresh ac timing
  const sc_core::sc_time tREFI1;
  const sc_core::sc_time tREFI2;
  const sc_core::sc_time tRFC1_slr; // Normal Refresh with 3DS same logical rank
  const sc_core::sc_time
      tRFC2_slr; // Fine Granularity Refresh with 3DS same logical rank
  const sc_core::sc_time
      tRFCsb_slr; // Same Bank Refresh with 3DS same logical rank
  const sc_core::sc_time
      tRFC1_dlr; // Normal Refresh with 3DS different logical rank
  const sc_core::sc_time
      tRFC1_dpr; // Normal Refresh with 3DS or DDP different physical rank
  const sc_core::sc_time
      tRFC2_dlr; // Fine Granularity Refresh with 3DS different logical rank
  const sc_core::sc_time tRFC2_dpr; // Fine Granularity Refresh with 3DS or DDP
                                    // different physical rank
  const sc_core::sc_time
      tRFCsb_dlr; // Same Bank Refresh with 3DS different logical rank
  const sc_core::sc_time tREFSBRD_slr; // Same Bank Refresh to ACT delay SLR
  const sc_core::sc_time tREFSBRD_dlr; // Same Bank Refresh to ACT delay DLR
  const sc_core::sc_time tREFABRD_dlr; // All Bank Refresh to ACT delay DLR //
                                       // ddr5 version c spec add this feature

  // rank to rank delay
  const sc_core::sc_time tWr2Wr_prank;
  const sc_core::sc_time tWr2Rd_prank;
  const sc_core::sc_time tRd2Wr_prank;
  const sc_core::sc_time tRd2Rd_prank;

  const sc_core::sc_time tWr2Wr_lrank;
  const sc_core::sc_time tWr2Rd_lrank;
  const sc_core::sc_time tRd2Wr_lrank;
  const sc_core::sc_time tRd2Rd_lrank;
  uint64_t DeviceMemorySizeBytes;
  uint64_t MemorySizeBytes;

  // MC clock domain
  const sc_core::sc_time tCCD_L_slr_mc; // Read to Read command delay for same
                                        // bank group in same logical rank
  const sc_core::sc_time
      tCCD_L_WR_slr_mc; // Write to Write command delay for same bank group in
                        // same logical rank
  const sc_core::sc_time
      tCCD_L_WR2_slr_mc; // Write to Write command delay for same bank group in
                         // same logical rank, second write not RMW
  const sc_core::sc_time
      tCCD_L_RTW_slr_mc; // Read to Write command delay for same bank group in
                         // same logical rank
  const sc_core::sc_time
      tCCD_L_WTR_slr_mc; // not part// Write to Read command delay for same bank
                         // group in same logical rank
  const sc_core::sc_time tCCD_M_slr_mc;    // Read to Read command delay for
                                           // different bank in same bank group
  const sc_core::sc_time tCCD_M_WR_slr_mc; // Write to Write command delay for
                                           // different bank in same bank group
  const sc_core::sc_time
      tCCD_M_WTR_slr_mc; // not part// Write to Read command delay for different
                         // bank in same bank group
  const sc_core::sc_time
      tCCD_S_slr_mc; // Read to Read command delay for different bank group in
                     // same logical rank
  const sc_core::sc_time
      tCCD_S_WR_slr_mc; // Write to Write command delay for different bank group
                        // in same logical rank
  const sc_core::sc_time
      tCCD_S_RTW_slr_mc; // Read to Write command delay for different bank group
                         // in same logical rank
  const sc_core::sc_time
      tCCD_S_WTR_slr_mc; // not part  // Write to Read command delay for
                         // different bank group in same logical rank
  const sc_core::sc_time
      tCCD_WTRA_slr_mc; // Write to Read with Auto Precharge command for same
                        // bank in same logic rank
  const sc_core::sc_time
      tRRD_L_slr_mc; // Activate to Activate command delay to same bank group in
                     // the same logical rank
  const sc_core::sc_time
      tRRD_S_slr_mc; // Activate to Activate command delay to different bank
                     // group in the same logical rank
  const sc_core::sc_time tFAW_slr_mc; // Four activate window to the same
                                      // logical rank  // only for x4
  const sc_core::sc_time tRTP_slr_mc; // Read command to Precharge command delay
                                      // in same logical rank
  const sc_core::sc_time
      tPPD_slr_mc; // Precharge to Precharge delay in same logical rank
  const sc_core::sc_time tWR_slr_mc; // Write recovery time in same logical rank
  const sc_core::sc_time
      tCCD_dlr_mc; // Read to Read command delay in different logical ranks
  const sc_core::sc_time
      tCCD_WR_dlr_mc; // Write to Write command delay in different logical ranks
  const sc_core::sc_time
      tCCD_RTW_dlr_mc; // Read to Write command delay in different logical ranks
  const sc_core::sc_time tCCD_WTR_dlr_mc; // not part // Write to Read command
                                          // delay in different logical ranks
  const sc_core::sc_time tRRD_dlr_mc; // Activate to Activate command delay to
                                      // different logical ranks
  const sc_core::sc_time
      tFAW_dlr_mc; // Four activate window to different logical ranks
  const sc_core::sc_time
      tPPD_dlr_mc; // Precharge to Precharge delay in different logical rank
  const sc_core::sc_time
      tCCD_WR_dpr_mc; // Minimum Write to Write command delay in different 3DS
                      // or DDP physical ranks
  const sc_core::sc_time tDCAW_mc; // Activate window by DIMM channel

  const sc_core::sc_time tBurst_mc;

  const sc_core::sc_time tREFI1_mc;
  const sc_core::sc_time tREFI2_mc;
  const sc_core::sc_time
      tRFC1_slr_mc; // Normal Refresh with 3DS same logical rank
  const sc_core::sc_time
      tRFC2_slr_mc; // Fine Granularity Refresh with 3DS same logical rank
  const sc_core::sc_time
      tRFCsb_slr_mc; // Same Bank Refresh with 3DS same logical rank
  const sc_core::sc_time
      tRFC1_dlr_mc; // Normal Refresh with 3DS different logical rank
  const sc_core::sc_time
      tRFC1_dpr_mc; // Normal Refresh with 3DS or DDP different physical rank
  const sc_core::sc_time
      tRFC2_dlr_mc; // Fine Granularity Refresh with 3DS different logical rank
  const sc_core::sc_time tRFC2_dpr_mc; // Fine Granularity Refresh with 3DS or
                                       // DDP different physical rank
  const sc_core::sc_time
      tRFCsb_dlr_mc; // Same Bank Refresh with 3DS different logical rank
  const sc_core::sc_time tREFSBRD_slr_mc; // Same Bank Refresh to ACT delay SLR
  const sc_core::sc_time tREFSBRD_dlr_mc; // Same Bank Refresh to ACT delay DLR
  const sc_core::sc_time
      tREFABRD_dlr_mc; // All Bank Refresh to ACT delay DLR // ddr5 version c
                       // spec add this feature

  sc_core::sc_time tREFI_mc;
  sc_core::sc_time tRFC_slr_mc;
  sc_core::sc_time tRFC_dlr_mc;
  sc_core::sc_time tRFC_dpr_mc;

  explicit DDR5MemSpec3ds(const DDR5MemConfig &mem_spec,
                          const std::string &output_dir);
};

} // namespace Controller
} // namespace dmu

#endif