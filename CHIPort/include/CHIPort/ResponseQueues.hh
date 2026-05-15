#ifndef __RESPONSE_QUEUES_HH__
#define __RESPONSE_QUEUES_HH__

#include "ARM/TLM/arm_chi_phase.h"
#include "CHIPort/CHIUtilities.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <systemc>
#include <unordered_map>

// #include "Controller/common/Configure.hh"
#include "Common/CommonDefine.hh"
#include "Configure/Configure.hh"
#include "PortCommon.hh"
namespace dmu {
namespace Port {

static ARM::CHI::Phase make_response_phase(const ARM::CHI::Phase &fw_phase,
                                           const ARM::CHI::RspOpcode rsp_opcode,
                                           const unsigned dbid = 0) {
  ARM::CHI::Phase rsp_phase;

  rsp_phase.channel = ARM::CHI::CHANNEL_RSP;

  rsp_phase.qos = fw_phase.qos;
  rsp_phase.tgt_id = fw_phase.src_id;
  rsp_phase.src_id = fw_phase.tgt_id;
  rsp_phase.txn_id = fw_phase.txn_id;
  rsp_phase.home_nid = fw_phase.tgt_id;
  rsp_phase.rsp_opcode = rsp_opcode;
  rsp_phase.dbid = dbid;

  return rsp_phase;
}

// class RetryResourceManager;
class ResponseQueues {
public:
  // ResponseQueues(const Configure& configure)
  // {
  //  response_queues.reserve(Queue_Type_Num);
  // }
  explicit ResponseQueues(const Configure &configure);
  ~ResponseQueues() = default;

  int winner_queue_index{-1};

  void InsertDbidResp(const CHIFlit &req_flit, unsigned dbid) {
    auto &queue =
        response_queues.at(static_cast<size_t>(ResponseQueueType::Dbid));
    queue.emplace_back(req_flit.payload,
                       make_response_phase(req_flit.phase,
                                           ARM::CHI::RSP_OPCODE_DBID_RESP,
                                           dbid));
  }

  void
  InsertCompResp(const CHIFlit &req_flit) // TODO: CMO Comp in
                                          // ResponseQueueType::ReadReceipt_Comp
                                          // and Other in Comp Queue
  {
    if (req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL ||
        req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL) {
      auto &queue =
          response_queues.at(static_cast<size_t>(ResponseQueueType::Comp));
      queue.emplace_back(
          req_flit.payload,
          make_response_phase(req_flit.phase, ARM::CHI::RSP_OPCODE_COMP));
    } else {
      DPRINT_INFO(
          false, "ResponseQueues",
          "Insert Comp, the req opcode is not writenosnpfull or writenosnpptl");
    }
  }

  void InsertReadReceiptResp(
      const CHIFlit &req_flit) // TODO: Order == 1  && Prefetch Hit situation
  {
    if (req_flit.phase.req_opcode == ARM::CHI::REQ_OPCODE_READ_NO_SNP) {
      auto &queue = response_queues.at(
          static_cast<size_t>(ResponseQueueType::ReadReceipt));
      queue.emplace_back(
          req_flit.payload,
          make_response_phase(req_flit.phase,
                              ARM::CHI::RSP_OPCODE_READ_RECEIPT));
    }
  }

  void InsertRetryAckResp(const CHIFlit &req_flit) {

    auto &queue =
        response_queues.at(static_cast<size_t>(ResponseQueueType::RetryAck));
    auto retryack_phase =
        make_response_phase(req_flit.phase, ARM::CHI::RSP_OPCODE_RETRY_ACK);
    retryack_phase.c_busy = 0x7 & 3; // RetryAck Cbusy field set to 3'b011;
    queue.emplace_back(req_flit.payload, retryack_phase);
  }

  void InsertPcrdGrantResp(const CHIFlit &pcrd_flit) {
    auto &queue =
        response_queues.at(static_cast<size_t>(ResponseQueueType::PcrdGrant));
    queue.emplace_back(pcrd_flit);
  }

private:
  const unsigned Queue_Type_Num{6};
  const unsigned Max_Dbid_Queue_size;
  const unsigned Max_RetryAck_Queue_size;
  const unsigned Max_PcrdGrant_Queue_size;
  const unsigned Max_ReadReceipt_Comp_Queue_size;
  const unsigned Max_ReadReceipt_Queue_size;
  const unsigned Max_Comp_Queue_size;

  std::vector<std::deque<CHIFlit>> response_queues{Queue_Type_Num};

public:
  unsigned GetQueueSize(const ResponseQueueType &type) const {
    return response_queues.at(static_cast<size_t>(type)).size();
  }

  bool IsEmpty() const {
    for (auto &q : response_queues) {
      if (q.size() > 0)
        return false;
    }
    return true;
  }

  bool IsResponseQueueFull(const ResponseQueueType &type);

  bool HasRspPending() const {
    for (auto &q : response_queues) {
      if (q.size() > 0)
        return true;
    }
    return false;
  }

  int Arbiter() {
    int selected_queue = -1;
    for (int i = 0; i < Queue_Type_Num; i++) {
      int index = ((winner_queue_index + 1) + i) % Queue_Type_Num;
      if (response_queues[index].size() > 0) {
        selected_queue = index;
        break;
      }
    }

    winner_queue_index = selected_queue;
    return selected_queue;
  }

  inline const CHIFlit &GetQueueFront(const uint8_t &queue_id) {
    return response_queues[queue_id].front();
  }

  inline void QueuePop(const uint8_t &queue_id) {
    response_queues.at(queue_id).pop_front();
  }
};

} // namespace Port

} // namespace dmu

#endif