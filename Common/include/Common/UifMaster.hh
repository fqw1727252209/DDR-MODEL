#ifndef __UIF_MASTER_HH__
#define __UIF_MASTER_HH__

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <random>
#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>
#include <vector>

#include "Common/Common.hh"
#include "Common/CommonDefine.hh"
#include "Common/StatisticExtension.hh"
#include "Common/UifExtension.hh"
#include "Common/logger.hh"
#include "Configure/Configure.hh"
#include "Configure/LoadConfigFromJson.hh"
#include "Controller/MemoryController.hh"
#include "Controller/common/MemoryManager.hh"

// #include "Controller/common/ControllerCommon.hh"
#include "sysc/communication/sc_clock.h"
#include "sysc/datatypes/int/sc_nbdefs.h"
#include "sysc/kernel/sc_module.h"
#include "sysc/kernel/sc_simcontext.h"
#include "sysc/utils/sc_report.h"
#include "systemc.h"
#include "tlm_core/tlm_2/tlm_2_interfaces/tlm_fw_bw_ifs.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_gp.h"
#include "tlm_core/tlm_2/tlm_generic_payload/tlm_phase.h"

namespace dmu {
enum TrafficType {
  Stream_Rd,
  Stream_Wr,
  Random_Rd,
  Random_Wr,
  Stream_Copy,
  Stream_Add,
  Random_Copy,
  Random_Add,
};

class UifMaster : public sc_core::sc_module {

public:
  UifMaster(const sc_core::sc_module_name &name, TrafficType traffic_type,
            const dmu::Configure &_config, unsigned _thread_num = 1,
            unsigned _trans_num = 10240)
      : sc_module(name), traffic_type(traffic_type), thread_num(_thread_num),
        trans_num(_trans_num), iSocket("iSocket"), config(_config),
        clk("clock", _config.mem_spec->tCK_mc)
  // , mm("memory_manager")
  {
    // pa_arbiter = PaArbiter(lpr_queue,hpr_queue,tpw_queue);
    iSocket.register_nb_transport_bw(this, &UifMaster::nb_transport_bw);
    SC_HAS_PROCESS(UifMaster);

    SC_THREAD(SendMultiTrans);

    SC_THREAD(TrafficGenerator);

    // TrafficGenerator();
  }

  sc_core::sc_clock clk;

  const TrafficType traffic_type;
  const unsigned thread_num;
  const unsigned trans_num;

  void bind(Controller::MemoryController &mc) { iSocket.bind(mc.tSocket); }

  bool SendUifTrans(uint64_t addr, CmdType cmd_type, uint8_t qos_value,
                    unsigned trans_id) {
    tlm::tlm_generic_payload &trans = mm.allocate();
    trans.set_address(addr);
    dmu::UifInfo _uif_info;
    _uif_info.cmd_type = cmd_type;
    _uif_info.is_rmw = (cmd_type == CmdType::RMW) ? true : false;
    // trans.set_auto_extension<UifExtension>(uif_ext);
    if (cmd_type == CmdType::RD) {
      trans.set_command(tlm::TLM_READ_COMMAND);
      _uif_info.qos = Qos{qos_value, true};
    } else if (cmd_type == CmdType::WR || cmd_type == CmdType::RMW) {
      trans.set_command(tlm::TLM_WRITE_COMMAND);
      _uif_info.qos = Qos{qos_value, false};
    } else {
      std::cerr << "Uif Master: Invalid cmd Type " << std::endl;
      std::abort();
    }

    tlm::tlm_phase phase = tlm::tlm_phase_enum::BEGIN_REQ;
    sc_core::sc_time delay = sc_core::sc_time(0, sc_core::SC_NS);
    dmu::UifExtension *uif_ext = new dmu::UifExtension(_uif_info);
    dmu::StatisticExtension *stat_ext = new dmu::StatisticExtension();
    stat_ext->RecordTransactionId(trans_id);
    // std::cout<<"trans_id: "<<trans_id<<std::endl;
    trans.set_extension<dmu::StatisticExtension>(stat_ext);
    trans.set_extension<dmu::UifExtension>(uif_ext);
    tlm::tlm_sync_enum callback = iSocket->nb_transport_fw(trans, phase, delay);
    if (callback == tlm::tlm_sync_enum::TLM_ACCEPTED) {
      DPRINT_INFO(TOP_DEBUG, name(),
                  " trans is successfully sent to downstream");
      return true;
    } else if (callback == tlm::tlm_sync_enum::TLM_UPDATED) {
      DPRINT_INFO(TOP_DEBUG, name(), " trans is blocked by downstream");
      return false;
    }

    return false;
  }

  void SendMultiTrans() {
    while (true) {
      pa_arbiter.UpdateState();
      if (pa_arbiter.GetWinningQueue() == QueueType::LPR) {
        DPRINT_INFO(false, name(), " State Is: %d , winning queue is: %d",
                    pa_arbiter.state, QueueType::LPR);
        SendLprTrans();
        DPRINT_INFO(false, name(), "Has Lpr Cmd Pending: %ld",
                    lpr_queue.GetQueueSize());
      } else if (pa_arbiter.GetWinningQueue() == QueueType::HPR) {
        DPRINT_INFO(false, name(), " State Is: %d , winning queue is: %d",
                    pa_arbiter.state, QueueType::HPR);

        SendHprTrans();
        DPRINT_INFO(false, name(), "Has Hpr Cmd Pending: %ld",
                    hpr_queue.GetQueueSize());
      } else if (pa_arbiter.GetWinningQueue() == QueueType::TPW) {
        DPRINT_INFO(false, name(), " State Is: %d , winning queue is: %d",
                    pa_arbiter.state, QueueType::TPW);

        SendTpwTrans();
        DPRINT_INFO(false, name(), "Has Tpw Cmd Pending: %ld",
                    tpw_queue.GetQueueSize());
      } else {
        // DPRINT_INFO(TOP_DEBUG,name()," No Trans Can be Sent in This Cycle");
      }
      if (!stage_queue3.empty()) {
        auto trans = stage_queue3.front();
        if (trans->get_extension<UifExtension>()->GetQosLevel() ==
            PriorityClass::HPR) {
          hpr_queue.InsertRequest(trans);
          DPRINT_INFO(TOP_DEBUG, name(),
                      "hpr trans is successfully Added, Queue Size: %ld",
                      hpr_queue.GetQueueSize());
        } else if (trans->get_extension<UifExtension>()->GetQosLevel() ==
                       PriorityClass::LPR ||
                   trans->get_extension<UifExtension>()->GetQosLevel() ==
                       PriorityClass::GPR) {
          lpr_queue.InsertRequest(trans);
          DPRINT_INFO(TOP_DEBUG, name(),
                      "lpr trans is successfully Added, Queue Size: %ld",
                      lpr_queue.GetQueueSize());
        } else if (trans->get_extension<UifExtension>()->GetQosLevel() ==
                       PriorityClass::TPW ||
                   trans->get_extension<UifExtension>()->GetQosLevel() ==
                       PriorityClass::GPW) {
          tpw_queue.InsertRequest(trans);
          DPRINT_INFO(TOP_DEBUG, name(),
                      "tpw trans is successfully Added, Queue Size: %ld",
                      tpw_queue.GetQueueSize());
        } else {
          std::cerr << "Uif Master: Invalid Qos Level" << std::endl;
          std::abort();
        }
        stage_queue3.pop_front();
      }
      if (!stage_queue2.empty()) {
        stage_queue3.push_back(stage_queue2.front());
        stage_queue2.pop_front();
      }
      if (!stage_queue1.empty()) {
        stage_queue2.push_back(stage_queue1.front());
        stage_queue1.pop_front();
      }
      wait(1 * sc_core::sc_time(config.mem_spec->tCK_mc));
    }
  }

  void TrafficGenerator() {
    unsigned IDLE_CYCLE{1000};
    unsigned n = 0;

    while (n < IDLE_CYCLE) {
      n++;
      wait(1 * sc_core::sc_time(config.mem_spec->tCK_mc));
      continue;
    }

    constexpr uint64_t base_addr0{0x0000'0000};
    constexpr uint64_t stride{0x40};
    std::size_t trans_id = 0;

    struct SourceState {
      unsigned remaining_loops;
      unsigned step;
      unsigned idx;
      uint64_t offset;
    };

    std::vector<SourceState> sources;
    sources.reserve(thread_num);

    unsigned base_loops = trans_num / thread_num;
    unsigned remainder = trans_num % thread_num;
    uint64_t cumulative_offset = 0;

    for (unsigned k = 0; k < thread_num; k++) {
      unsigned loops = base_loops + (k < remainder ? 1 : 0);
      uint64_t seg = static_cast<uint64_t>(loops) * stride;
      sources.push_back({loops, 0, 0, cumulative_offset});
      cumulative_offset += seg;
    }

    if (traffic_type == TrafficType::Stream_Copy ||
        traffic_type == TrafficType::Stream_Add) {
      uint64_t total_stream_space = static_cast<uint64_t>(trans_num) * stride;
      if (traffic_type == TrafficType::Stream_Copy) {
        // ensure no overlap between RD and WR regions across all sources
        // WR starts at base_addr0 + 2 * total_stream_space
        (void)total_stream_space;
      } else {
        (void)total_stream_space;
      }
    }

    while (true) {
      std::vector<unsigned> active;
      active.reserve(thread_num);
      for (unsigned k = 0; k < thread_num; k++) {
        if (sources[k].remaining_loops > 0)
          active.push_back(k);
      }
      if (active.empty())
        break;

      unsigned k = active[random_addr_gen.Generate(0, active.size() - 1) %
                          active.size()];
      SourceState &src = sources[k];

      if (traffic_type == TrafficType::Stream_Rd) {
        AddTrans(base_addr0 + src.offset + src.idx * stride, CmdType::RD, 15,
                 trans_id);
        trans_id++;
        src.step++;
      } else if (traffic_type == TrafficType::Stream_Wr) {
        AddTrans(base_addr0 + src.offset + src.idx * stride, CmdType::WR, 15,
                 trans_id);
        trans_id++;
        src.step++;
      } else if (traffic_type == TrafficType::Random_Rd) {
        uint64_t seg = 0x2000'0000 / thread_num;
        AddTrans(random_addr_gen.Generate(base_addr0 + k * seg,
                                          base_addr0 + (k + 1) * seg),
                 CmdType::RD, 15, trans_id);
        trans_id++;
        src.step++;
      } else if (traffic_type == TrafficType::Random_Wr) {
        uint64_t seg = 0x2000'0000 / thread_num;
        AddTrans(random_addr_gen.Generate(base_addr0 + k * seg,
                                          base_addr0 + (k + 1) * seg),
                 CmdType::WR, 15, trans_id);
        trans_id++;
        src.step++;
      } else if (traffic_type == TrafficType::Stream_Copy) {
        uint64_t total_stream_space = static_cast<uint64_t>(trans_num) * stride;
        if (src.step == 0) {
          AddTrans(base_addr0 + src.offset + src.idx * stride, CmdType::RD, 15,
                   trans_id);
          trans_id++;
          src.step = 1;
        } else {
          AddTrans(base_addr0 + 2 * total_stream_space + src.offset +
                       src.idx * stride,
                   CmdType::WR, 15, trans_id);
          trans_id++;
          src.step = 0;
          src.idx++;
          src.remaining_loops--;
        }
      } else if (traffic_type == TrafficType::Stream_Add) {
        uint64_t total_stream_space = static_cast<uint64_t>(trans_num) * stride;
        if (src.step == 0) {
          AddTrans(base_addr0 + src.offset + src.idx * stride, CmdType::RD, 15,
                   trans_id);
          trans_id++;
          src.step = 1;
        } else if (src.step == 1) {
          AddTrans(base_addr0 + total_stream_space + src.offset +
                       src.idx * stride,
                   CmdType::RD, 15, trans_id);
          trans_id++;
          src.step = 2;
        } else {
          AddTrans(base_addr0 + 2 * total_stream_space + src.offset +
                       src.idx * stride,
                   CmdType::WR, 15, trans_id);
          trans_id++;
          src.step = 0;
          src.idx++;
          src.remaining_loops--;
        }
      } else if (traffic_type == TrafficType::Random_Copy) {
        uint64_t seg = 0x2000'0000 / thread_num;
        if (src.step == 0) {
          AddTrans(random_addr_gen.Generate(base_addr0 + k * seg,
                                            base_addr0 + (k + 1) * seg),
                   CmdType::WR, 15, trans_id);
          trans_id++;
          src.step = 1;
        } else {
          AddTrans(random_addr_gen.Generate(base_addr0 + 0x4000'0000 + k * seg,
                                            base_addr0 + 0x4000'0000 +
                                                (k + 1) * seg),
                   CmdType::RD, 15, trans_id);
          trans_id++;
          src.step = 0;
          src.idx++;
          src.remaining_loops--;
        }
      } else if (traffic_type == TrafficType::Random_Add) {
        uint64_t seg = 0x2000'0000 / thread_num;
        if (src.step == 0) {
          AddTrans(random_addr_gen.Generate(base_addr0 + k * seg,
                                            base_addr0 + (k + 1) * seg),
                   CmdType::WR, 15, trans_id);
          trans_id++;
          src.step = 1;
        } else if (src.step == 1) {
          AddTrans(random_addr_gen.Generate(base_addr0 + 0x4000'0000 + k * seg,
                                            base_addr0 + 0x4000'0000 +
                                                (k + 1) * seg),
                   CmdType::RD, 15, trans_id);
          trans_id++;
          src.step = 2;
        } else {
          AddTrans(random_addr_gen.Generate(base_addr0 + 0x8000'0000 + k * seg,
                                            base_addr0 + 0x8000'0000 +
                                                (k + 1) * seg),
                   CmdType::RD, 15, trans_id);
          trans_id++;
          src.step = 0;
          src.idx++;
          src.remaining_loops--;
        }
      } else {
        DPRINT_FATAL(TOP_DEBUG, name(), "Traffic type is not supported");
      }

      // For single-command types, advance loop count after each command
      if (traffic_type == TrafficType::Stream_Rd ||
          traffic_type == TrafficType::Stream_Wr ||
          traffic_type == TrafficType::Random_Rd ||
          traffic_type == TrafficType::Random_Wr) {
        src.idx++;
        src.remaining_loops--;
      }

      wait(1 * sc_core::sc_time(config.mem_spec->tCK_mc));
    }
  }

  void AddTrans(uint64_t addr, CmdType cmd_type, uint8_t qos_value,
                unsigned trans_id) {
    tlm::tlm_generic_payload &trans = mm.allocate();
    trans.set_byte_enable_length(64);
    trans.set_address(addr);
    dmu::StatisticExtension *stat_ext = new dmu::StatisticExtension();
    trans_tracker.track_transaction(&trans, trans_id);
    stat_ext->RecordTransactionId(trans_id);
    dmu::UifInfo _uif_info;
    _uif_info.cmd_type = cmd_type;
    _uif_info.is_rmw = (cmd_type == CmdType::RMW) ? true : false;
    stat_ext->RecordInPortTime(sc_core::sc_time_stamp());
    if (cmd_type == CmdType::RD) {
      trans.set_command(tlm::TLM_READ_COMMAND);
      _uif_info.qos = Qos{qos_value, true};
      if (_uif_info.qos.GetQosLevel() == PriorityClass::HPR)
      // {
      //     hpr_queue.InsertRequest(static_cast<tlm::tlm_generic_payload*>(&trans));
      //     DPRINT_INFO(TOP_DEBUG,name(),"Hpr trans is successfully Added,
      //     Queue Size: %ld",hpr_queue.GetQueueSize());
      // }
      // else if(_uif_info.qos.GetQosLevel() == PriorityClass::LPR ||
      // _uif_info.qos.GetQosLevel() == PriorityClass::GPR)
      // {
      //     lpr_queue.InsertRequest(static_cast<tlm::tlm_generic_payload*>(&trans));
      //     DPRINT_INFO(TOP_DEBUG,name(),"Lpr trans is successfully Added,
      //     Queue Size: %ld",lpr_queue.GetQueueSize());
      // }
    } else if (cmd_type == CmdType::WR || cmd_type == CmdType::RMW) {
      trans.set_command(tlm::TLM_WRITE_COMMAND);
      _uif_info.qos = Qos{qos_value, false};
      // tpw_queue.InsertRequest(static_cast<tlm::tlm_generic_payload*>(&trans));
      // DPRINT_INFO(TOP_DEBUG,name(),"Tpw trans is successfully Added, Queue
      // Size: %ld",tpw_queue.GetQueueSize());
    } else {
      std::cerr << "Uif Master: Invalid cmd Type " << std::endl;
      std::abort();
    }
    stage_queue1.push_back(static_cast<tlm::tlm_generic_payload *>(&trans));
    dmu::UifExtension *uif_ext = new dmu::UifExtension(_uif_info);
    trans.set_extension<dmu::StatisticExtension>(stat_ext);
    trans.set_extension<dmu::UifExtension>(uif_ext);
  }

private:
  tlm::tlm_sync_enum nb_transport_bw(tlm::tlm_generic_payload &trans,
                                     tlm::tlm_phase &phase,
                                     sc_core::sc_time &delay) {
    if (phase == UIF_RDAT_END) {
      DPRINT_INFO(false, (std::string(name()) + " Uif Dat Channel").c_str(),
                  "Transmit Data END");
      trans.get_extension<StatisticExtension>()->PrintStatisticsToLogger(
          Logger::getInstance("Statisic"));
      trans_tracker.release_transaction(
          trans.get_extension<StatisticExtension>()->GetTransactionId());
    } else if (phase == UIF_RDAT_BEGIN) {
      DPRINT_INFO(false, (std::string(name()) + " Uif Dat Channel").c_str(),
                  "Transmit Data Begin");
    } else if (phase == WR_RESPONSE_COMPLETE) {
      trans.get_extension<StatisticExtension>()->PrintStatisticsToLogger(
          Logger::getInstance("Statisic"));
      trans_tracker.release_transaction(
          trans.get_extension<StatisticExtension>()->GetTransactionId());
    } else if (phase == UIF_CREDIT) {
      auto uif_sideband_info =
          trans.get_extension<dmu::UifSideBandExtension>()->uif_side_band_info;
      if (uif_sideband_info.lpr_credit_valid) {
        DPRINT_INFO(true, (std::string(name()) + " Uif Credit Channel").c_str(),
                    "Receive Lpr Credit");
        lpr_queue.ReceiveCredit();
      }
      if (uif_sideband_info.hpr_credit_valid) {
        DPRINT_INFO(true, (std::string(name()) + " Uif Credit Channel").c_str(),
                    "Receive Hpr Credit");
        hpr_queue.ReceiveCredit();
      }
      if (uif_sideband_info.tpw_credit_valid) {
        DPRINT_INFO(true, (std::string(name()) + " Uif Credit Channel").c_str(),
                    "Receive Tpw Credit");
        tpw_queue.ReceiveCredit();
      }
    } else if (phase == UIF_WDAT_REQ) {
      if (trans.get_byte_enable_length() == 64) {
        DPRINT_INFO(true,
                    (std::string(name()) + " Uif Write Dat Channel").c_str(),
                    "Receive Wdat Request");
        tlm::tlm_phase phase1 = UIF_WDAT_BEGIN;
        sc_core::sc_time delay1{config.mem_spec->tCK_mc};
        iSocket->nb_transport_fw(trans, phase1, delay1);

        tlm::tlm_phase phase2 = UIF_WDAT_END;
        sc_core::sc_time delay2{config.mem_spec->tCK_mc +
                                config.mem_spec->tCK_mc};
        iSocket->nb_transport_fw(trans, phase2, delay2);
        DPRINT_INFO(true,
                    (std::string(name()) + " Uif Write Dat Channel").c_str(),
                    "End Wdat Request");
      }
    }
    return tlm::TLM_COMPLETED;
  }

private:
  std::deque<tlm::tlm_generic_payload *> stage_queue1;
  std::deque<tlm::tlm_generic_payload *> stage_queue2;
  std::deque<tlm::tlm_generic_payload *> stage_queue3;

  class PayloadTracker {
  public:
    // 构造时预留空间，减少仿真运行时的 rehash 开销
    PayloadTracker(size_t max_outstanding = 1024) {
      m_pending_transactions.reserve(max_outstanding);
    }

    void track_transaction(tlm::tlm_generic_payload *trans, unsigned trans_id) {
      if (!trans)
        return;
      trans->acquire();
      // unordered_map 的插入效率在平均情况下为 O(1)
      m_pending_transactions[trans_id] = trans;
    }

    void release_transaction(uint64_t id) {
      auto it = m_pending_transactions.find(id);
      if (it != m_pending_transactions.end()) {
        it->second->release();
        m_pending_transactions.erase(it);
        DPRINT_INFO(TOP_DEBUG, "UifMaster", "trans: %ld is complete", id);
      } else {
        std::cerr << "this transaction id is not tracked: %d" << id
                  << std::endl;
        std::abort();
      }
    }

  private:
    // 使用 unordered_map 提升查找和删除速度
    std::unordered_map<uint64_t, tlm::tlm_generic_payload *>
        m_pending_transactions;
  } trans_tracker;

public:
  tlm_utils::simple_initiator_socket<UifMaster> iSocket;

private:
  dmu::Controller::MemoryManager mm;
  const dmu::Configure &config;

  class RandomAddressGenerator {
  private:
    std::mt19937 gen;

  public:
    explicit RandomAddressGenerator(uint64_t seed = std::random_device{}())
        : gen(seed) {}
    uint64_t Generate(uint64_t low, uint64_t high) {
      assert(low <= high && "Invalid address range: low > high");
      std::uniform_int_distribution<uint64> dis(low, high);
      return dis(gen) & ~0x3FULL;
    }
  } random_addr_gen;

  class RequestQueue {
  private:
    sc_core::sc_time aging_expired_time{sc_core::sc_max_time()};
    std::deque<tlm::tlm_generic_payload *> request_queue;
    unsigned credit_counter{0};
    size_t size;

  public:
    bool IsQueueEmpty() const { return request_queue.empty(); }
    bool IsQueueExpired() const {
      return aging_expired_time >= sc_core::sc_time_stamp();
    }
    bool HasCredit() const { return credit_counter > 0; }

    void ReceiveCredit() { credit_counter++; }
    void ConsumeCredit() { credit_counter--; }

    bool IsQueueFull() const { return request_queue.size() >= size; }
    bool HasExpiredCmd() const { return false; }

    bool IsQueueLocked() const { return false; }

    void InsertRequest(tlm::tlm_generic_payload *trans) {
      request_queue.push_back(trans);
    }
    tlm::tlm_generic_payload *GetRequest() { return request_queue.front(); }

    void PopRequest() { request_queue.pop_front(); }

    std::size_t GetQueueSize() const { return request_queue.size(); }

    // if th command type is rmw, and the lpr has no credit, then the command
    // will be masked
    explicit RequestQueue(size_t size) : size(size) {}
  };
  RequestQueue lpr_queue{64};
  RequestQueue hpr_queue{64};
  RequestQueue tpw_queue{64};

  unsigned lpr_deadline_time{0};
  unsigned hpr_deadline_time{0};
  unsigned tpw_deadline_time{0};

  unsigned deadline_time{50000};

  void SendLprTrans() {
    auto trans = lpr_queue.GetRequest();
    trans->get_extension<StatisticExtension>()->RecordOutPortTime(
        sc_core::sc_time_stamp());
    tlm::tlm_phase phase = UIF_REQ;
    sc_core::sc_time delay = sc_core::sc_time(0, sc_core::SC_NS);
    tlm::tlm_sync_enum callback =
        iSocket->nb_transport_fw(*trans, phase, delay);
    if (callback == tlm::tlm_sync_enum::TLM_ACCEPTED) {
      DPRINT_INFO(TOP_DEBUG, name(),
                  "lpr trans is successfully sent to downstream");
      lpr_queue.ConsumeCredit();
      lpr_queue.PopRequest();

      lpr_deadline_time = 0;
    } else if (callback == tlm::tlm_sync_enum::TLM_UPDATED) {
      DPRINT_WARNING(
          TOP_DEBUG, name(),
          " lpr Should send trans, but not accepted for addr collision");
      if (lpr_deadline_time >= deadline_time) {
        ABORT_MESSAGE("Lpr trans is not accepted for too long time");
      }
      lpr_deadline_time++;
    }
  }
  void SendHprTrans() {
    auto trans = hpr_queue.GetRequest();
    trans->get_extension<StatisticExtension>()->RecordOutPortTime(
        sc_core::sc_time_stamp());
    tlm::tlm_phase phase = UIF_REQ;
    sc_core::sc_time delay = sc_core::sc_time(0, sc_core::SC_NS);
    tlm::tlm_sync_enum callback =
        iSocket->nb_transport_fw(*trans, phase, delay);
    if (callback == tlm::tlm_sync_enum::TLM_ACCEPTED) {
      DPRINT_INFO(TOP_DEBUG, name(),
                  "hpr trans is successfully sent to downstream");
      hpr_queue.ConsumeCredit();
      hpr_queue.PopRequest();
      hpr_deadline_time = 0;
    } else if (callback == tlm::tlm_sync_enum::TLM_UPDATED) {
      DPRINT_WARNING(
          TOP_DEBUG, name(),
          " hpr Should send trans, but not accepted for addr collision");
      if (hpr_deadline_time >= deadline_time) {
        ABORT_MESSAGE("hpr trans is not accepted for too long time");
      }
      hpr_deadline_time++;
    }
  }
  void SendTpwTrans() {
    auto trans = tpw_queue.GetRequest();
    trans->get_extension<StatisticExtension>()->RecordOutPortTime(
        sc_core::sc_time_stamp());
    tlm::tlm_phase phase = UIF_REQ;
    sc_core::sc_time delay = sc_core::sc_time(0, sc_core::SC_NS);
    tlm::tlm_sync_enum callback =
        iSocket->nb_transport_fw(*trans, phase, delay);
    if (callback == tlm::tlm_sync_enum::TLM_ACCEPTED) {
      DPRINT_INFO(TOP_DEBUG, name(),
                  "tpw trans is successfully sent to downstream");
      if (trans->get_extension<UifExtension>()->_uif_info.is_rmw) {
        lpr_queue.ConsumeCredit();
      }
      tpw_queue.ConsumeCredit();
      tpw_queue.PopRequest();
      tpw_deadline_time = 0;
    } else if (callback == tlm::tlm_sync_enum::TLM_UPDATED) {
      DPRINT_WARNING(
          TOP_DEBUG, name(),
          " Tpw Should send trans, but not accepted for addr collision");
      if (tpw_deadline_time >= deadline_time) {
        ABORT_MESSAGE("tpw trans is not accepted for too long time");
      }
      tpw_deadline_time++;
    }
  }

  enum QueueType { LPR, HPR, TPW, NONE };
  class PaArbiter {
  public:
    enum State { IDLE, RD, WR } state{IDLE};
    explicit PaArbiter(const RequestQueue &lpr_queue,
                       const RequestQueue &hpr_queue,
                       const RequestQueue &tpw_queue)
        : lpr_queue(lpr_queue), hpr_queue(hpr_queue), tpw_queue(tpw_queue) {}

  private:
    const RequestQueue &lpr_queue;
    const RequestQueue &hpr_queue;
    const RequestQueue &tpw_queue;
    bool switch_fast{false};

    bool IsRdHold() {
      if (switch_fast) {
        return (!lpr_queue.IsQueueEmpty() && lpr_queue.HasCredit()) ||
               (!hpr_queue.IsQueueEmpty() && hpr_queue.HasCredit());
      } else {
        return lpr_queue.HasCredit() || hpr_queue.HasCredit();
      }
    }
    bool IsWrHold() {
      if (switch_fast) {
        return !tpw_queue.IsQueueEmpty() && tpw_queue.HasCredit();
      } else {
        return tpw_queue.HasCredit();
      }
    }

    bool IsHprHighPriority() {
      return hpr_queue.IsQueueExpired() && hpr_queue.HasCredit() &&
             !hpr_queue.IsQueueEmpty();
    }
    bool IsLprHighPriority() {
      return (lpr_queue.IsQueueExpired() || lpr_queue.HasExpiredCmd()) &&
             lpr_queue.HasCredit() && !lpr_queue.IsQueueEmpty();
    }
    bool IsTpwHighPriority() {
      return (tpw_queue.IsQueueExpired() || tpw_queue.HasExpiredCmd()) &&
             tpw_queue.HasCredit() && !tpw_queue.IsQueueEmpty();
    }
    bool IsRdHighPriority() {
      return (IsHprHighPriority() || IsLprHighPriority());
    }
    bool IsWrHighPriority() { return (IsTpwHighPriority()); }
    bool HasRdRequest() {
      return (!lpr_queue.IsQueueEmpty() && lpr_queue.HasCredit()) ||
             (!hpr_queue.IsQueueEmpty() && hpr_queue.HasCredit());
    }
    bool HasWrRequest() {
      return !tpw_queue.IsQueueEmpty() && tpw_queue.HasCredit();
    }

  public:
    void UpdateState() {
      switch (state) {
      case IDLE:
        if (HasRdRequest())
          state = RD;
        else if (HasWrRequest())
          state = WR;
        else
          state = IDLE;
        break;

      case RD:
        if (IsRdHighPriority())
          state = RD;
        else if (IsWrHighPriority())
          state = WR;
        else if (IsRdHold())
          state = RD;
        else if (HasWrRequest())
          state = WR;
        else
          state = IDLE;
        break;

      case WR:
        if (IsWrHighPriority())
          state = WR;
        else if ((IsRdHighPriority()) ||
                 (!hpr_queue.IsQueueEmpty() && hpr_queue.HasCredit()))
          state = RD;
        else if (IsWrHold())
          state = WR;
        else if (!lpr_queue.IsQueueEmpty() && lpr_queue.HasCredit())
          state = RD;
        else
          state = IDLE;
        break;
      }
    }

    QueueType GetWinningQueue() {
      if (state == RD) {
        if (IsHprHighPriority() && hpr_queue.HasCredit())
          return HPR;
        else if (IsLprHighPriority() && lpr_queue.HasCredit())
          return LPR;
        else if (!hpr_queue.IsQueueEmpty() && hpr_queue.HasCredit() &&
                 hpr_queue.IsQueueLocked())
          return HPR;
        else if (!lpr_queue.IsQueueEmpty() && lpr_queue.HasCredit() &&
                 lpr_queue.IsQueueLocked())
          return LPR;
        else if (!hpr_queue.IsQueueEmpty() && hpr_queue.HasCredit())
          return HPR;
        else if (!lpr_queue.IsQueueEmpty() && lpr_queue.HasCredit())
          return LPR;
        else
          return NONE;
      } else if (state == WR) {
        if (!tpw_queue.IsQueueEmpty() && tpw_queue.HasCredit() &&
            tpw_queue.IsQueueLocked())
          return TPW;
        else if (!tpw_queue.IsQueueEmpty() && tpw_queue.HasCredit())
          return TPW;
        else
          return NONE;
      } else
        return NONE;
    }

    bool IsGprCritical() {
      if ((lpr_queue.HasExpiredCmd() && !lpr_queue.HasCredit()) ||
          (tpw_queue.HasExpiredCmd() && !lpr_queue.HasCredit())) {
        return true;
      } else {
        return false;
      }
    }

    bool IsGpwCritical() {
      if (tpw_queue.HasExpiredCmd() && !tpw_queue.HasCredit()) {
        return true;
      } else {
        return false;
      }
    }

  } pa_arbiter{lpr_queue, hpr_queue, tpw_queue};

  // should build 3 request queue, all the queue is a fifo/deque

  // hpr is high level: hpr queue is aging
  // lpr is high level: 1. lpr queue is aging
  //                    or 2. exist timeout gpr cmd in lpr queue
  // tpw is high level: 1. tpw queue is aging
  //                    or 2. exist timeout gpw cmd in tpw queue
  //

  // state info signal: gpr_go2_critical: 1. gpr timeout but no lpr credit
  //                                      or  2. gpw timeout and gpw is rmw
  //                                      request, but no lpr credit
  //                    gpw_go2_critical: 1. gpw timeout but no tpw credit

  // pa switch fast --> this is a configure param
  // if true: rd hold: hpr or lpr queue has credit and request for uif
  //          wr hold: tpw queue has credit and request for uif
  // if false: rd hold: rd queue has credit and request for uif
  //           wr hold: wr queue has credit

  // state machine:
  // IDLE   <------>    RD
  // <-->    WR    <-->
  // IDLE：
  // if(rd queue has req and credit) --> switch 2 RD
  // else if(wr queue has req and credit)  --> switch 2 WR
  // else --> keep IDLE

  // RD:
  // if(rd queue has high priority and credit) --> keep RD
  // else if(wr queue has high priority and credit) --> switch 2 WR
  // else if(rd hold) --> keep RD
  // else if(wr queue has req and credit) --> switch 2 WR
  // else --> switch 2 IDLE

  // WR:
  // if(wr queue has high priority and has credit) --> keep WR
  // else if(rd queue has high priority and credit || hpr has req and credit)
  // --> switch 2 RD else if(wr hold) --> keep WR else if(lpr queue has req and
  // credit) --> switch 2 RD else --> switch 2 IDLE

  // Note:
  // rmw request will not considered be request if lpr has no credit
  // rmw send will call the credit decrease 1
};

} // namespace dmu

#endif