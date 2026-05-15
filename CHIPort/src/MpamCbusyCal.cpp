#include "CHIPort/MpamCbusyCal.hh"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
namespace dmu {
namespace Port {

MpamCbusyCal::MpamCbusyCal(const Configure &configure, const P2cFifo &p2c_fifo)
    : m_configure(configure), m_p2c_fifo(p2c_fifo),
      CBUSY_ENABLE(configure.controller_config->CBUSY_ENABLE),
      MPAM_ENABLE(configure.controller_config->MPAM_ENABLE),
      QOS_MAPPING(configure.controller_config->QOS_MAPPING),
      SRC_ID_MAPPING(configure.controller_config->SRC_ID_MAPPING),
      QOS_THRESHOD_GROUP0(configure.controller_config->QOS_THRESHOD_GROUP0),
      QOS_THRESHOD_GROUP1(configure.controller_config->QOS_THRESHOD_GROUP1),
      QOS_THRESHOD_GROUP2(configure.controller_config->QOS_THRESHOD_GROUP2),
      SRC_ID_GROUP0(configure.controller_config->SRC_ID_GROUP0),
      SRC_ID_GROUP1(configure.controller_config->SRC_ID_GROUP1),
      SRC_ID_GROUP2(configure.controller_config->SRC_ID_GROUP2),
      PROTQ_FREE_THRESHOLD_LGPR(
          configure.controller_config->PROTQ_FREE_THRESHOLD_LGPR),
      PROTQ_MIDDLE_THRESHOLD_LGPR(
          configure.controller_config->PROTQ_MIDDLE_THRESHOLD_LGPR),
      PROTQ_BUSY_THRESHOLD_LGPR(
          configure.controller_config->PROTQ_BUSY_THRESHOLD_LGPR),
      PROTQ_FREE_THRESHOLD_HPR(
          configure.controller_config->PROTQ_FREE_THRESHOLD_HPR),
      PROTQ_MIDDLE_THRESHOLD_HPR(
          configure.controller_config->PROTQ_MIDDLE_THRESHOLD_HPR),
      PROTQ_BUSY_THRESHOLD_HPR(
          configure.controller_config->PROTQ_BUSY_THRESHOLD_HPR),
      PROTQ_FREE_THRESHOLD_WR(
          configure.controller_config->PROTQ_FREE_THRESHOLD_WR),
      PROTQ_MIDDLE_THRESHOLD_WR(
          configure.controller_config->PROTQ_MIDDLE_THRESHOLD_WR),
      PROTQ_BUSY_THRESHOLD_WR(
          configure.controller_config->PROTQ_BUSY_THRESHOLD_WR),
      PROTQ_FREE_THRESHOLD_RD(
          configure.controller_config->PROTQ_FREE_THRESHOLD_RD),
      PROTQ_MIDDLE_THRESHOLD_RD(
          configure.controller_config->PROTQ_MIDDLE_THRESHOLD_RD),
      PROTQ_BUSY_THRESHOLD_RD(
          configure.controller_config->PROTQ_BUSY_THRESHOLD_RD),
      CAM_FREE_THRESHOLD_LGPR(
          configure.controller_config->CAM_FREE_THRESHOLD_LGPR),
      CAM_MIDDLE_THRESHOLD_LGPR(
          configure.controller_config->CAM_MIDDLE_THRESHOLD_LGPR),
      CAM_BUSY_THRESHOLD_LGPR(
          configure.controller_config->CAM_BUSY_THRESHOLD_LGPR),
      CAM_FREE_THRESHOLD_HPR(
          configure.controller_config->CAM_FREE_THRESHOLD_HPR),
      CAM_MIDDLE_THRESHOLD_HPR(
          configure.controller_config->CAM_MIDDLE_THRESHOLD_HPR),
      CAM_BUSY_THRESHOLD_HPR(
          configure.controller_config->CAM_BUSY_THRESHOLD_HPR),
      CAM_FREE_THRESHOLD_WR(configure.controller_config->CAM_FREE_THRESHOLD_WR),
      CAM_MIDDLE_THRESHOLD_WR(
          configure.controller_config->CAM_MIDDLE_THRESHOLD_WR),
      CAM_BUSY_THRESHOLD_WR(configure.controller_config->CAM_BUSY_THRESHOLD_WR),
      CAM_BASED(configure.controller_config->CAM_BASED),
      PROTQ_BASED(configure.controller_config->PROTQ_BASED) {}

uint8_t MpamCbusyCal::GetCbusyResult(ARM::CHI::Phase req_phase,
                                     PortCmdType cmd_type) {
  uint8_t protq_cbusy = GetProtqCbusyValue();
  uint8_t cam_cbusy = GetCamCbusyValue(cmd_type);
  // Implement with Codex
  uint8_t d2d_cbusy = 0; // TODO
  return std::max(std::max(protq_cbusy, cam_cbusy), d2d_cbusy);
}

uint8_t MpamCbusyCal::GetCalId(ARM::CHI::Phase req_phase) {
  if (QOS_MAPPING)
    return GetQosCalId(req_phase.qos);
  else if (SRC_ID_MAPPING)
    return GetSrcCalId(req_phase.src_id);
  else {
    std::cerr << "Call the GtCalId function, but not open CalId Mapping Switch";
    std::abort();
  }
}

uint8_t MpamCbusyCal::GetSrcCalId(unsigned src_id) {
  if (SRC_ID_GROUP0.count(src_id) > 0) {
    return 0;
  } else if (SRC_ID_GROUP1.count(src_id) > 0) {
    return 1;
  } else if (SRC_ID_GROUP2.count(src_id) > 0) {
    return 2;
  } else {
    return 3;
  }
}

uint8_t MpamCbusyCal::GetQosCalId(unsigned qos) {
  if (qos >= 0 && qos < QOS_THRESHOD_GROUP0) {
    return 0;
  } else if (qos >= QOS_THRESHOD_GROUP0 && qos < QOS_THRESHOD_GROUP1) {
    return 1;
  } else if (qos >= QOS_THRESHOD_GROUP1 && qos < QOS_THRESHOD_GROUP2) {
    return 2;
  } else if (qos >= QOS_THRESHOD_GROUP2 && qos < 15) {
    return 3;
  } else {
    std::cerr << "Invalid Qos Value " << __FUNCTION__;
    std::abort();
  }
}

// namespace Port
} // namespace dmu