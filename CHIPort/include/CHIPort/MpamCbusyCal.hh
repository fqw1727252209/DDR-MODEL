#ifndef __MPAM_CBUSY_CAL_HH__
#define __MPAM_CBUSY_CAL_HH__

#include "ARM/TLM/arm_chi_phase.h"
#include "CHIPort/P2cFifo.hh"
#include "CHIPort/PortCommon.hh"
#include "Configure/Configure.hh"
#include <cstdint>

namespace dmu {
namespace Port {

class MpamCbusyCal {

public:
  explicit MpamCbusyCal(const Configure &configure, const P2cFifo & p2c_fifo);

  uint8_t GetCbusyResult(ARM::CHI::Phase req_phase, PortCmdType cmd_type);
  uint8_t GetCalId(ARM::CHI::Phase req_phase);
  uint8_t GetSrcCalId(unsigned src_id);
  uint8_t GetQosCalId(unsigned qos);
  uint8_t GetProtqCbusyValue();
  uint8_t GetCamCbusyValue(PortCmdType cmd_type);
  uint8_t GetD2DCbusyValue(ARM::CHI::Phase req_phase);

private:
  const Configure &m_configure;
  const P2cFifo &m_p2c_fifo;

  // config
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
};

} // namespace Port
} // namespace dmu

#endif