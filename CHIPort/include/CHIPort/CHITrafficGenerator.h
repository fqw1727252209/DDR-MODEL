#ifndef ARM_CHI_TRAFFIC_GENERATOR_H
#define ARM_CHI_TRAFFIC_GENERATOR_H

#include <systemc>
#include <tlm>
#include <deque>
#include <unordered_map>
#include <unordered_set>
#include <array>
#include <vector>
#include <random>
#include <memory>
#include <optional>
#include <queue>

#include <ARM/TLM/arm_chi.h>
#include "CHIPort/CHIUtilities.h"

namespace dmu {
namespace Port {

struct TrafficConfig {
  double read_ratio = 0.5;
  uint64_t addr_start = 0x0;
  uint64_t addr_end = 0xFFFFFFFF;
  uint64_t addr_step = 0x40;
  bool sequential = true;
  uint64_t max_generated_transactions = 1000;
  unsigned max_outstanding = 16;
  uint8_t addr_target_select_bit = 6;
  unsigned num_initiators = 1;
  unsigned num_trackers = 1;

  ARM::CHI::MemAttr mem_attr = ARM::CHI::MEM_ATTR_NORMAL_WB_A;
  ARM::CHI::Order order = ARM::CHI::ORDER_REQUEST_ACCEPTED;
  uint8_t qos = 15;
  uint16_t src_id = 1;
  uint16_t tgt_id = 1;
  bool write_ptl = false;

  std::vector<uint16_t> initiator_src_ids;
  std::vector<uint16_t> initiator_tgt_ids;
};

struct RequestDescriptor {
  uint64_t address;
  ARM::CHI::Size size;
  bool is_read;
};

enum class TransactionState {
  REQ_SENT,
  READ_WAIT_DATA,
  WRITE_WAIT_DBID,
  WRITE_WAIT_COMP,
  DONE
};

struct ActiveTransaction {
  uint16_t txn_id;
  uint64_t address;
  bool is_read;
  ARM::CHI::Size size;
  TransactionState state;
  int initiator_index;
  uint16_t src_id;
  uint16_t tgt_id;
  uint8_t qos;
  ARM::CHI::Order order;
  uint8_t data_beats_remaining;
};

struct RetryEntry {
  uint64_t address;
  bool is_read;
  ARM::CHI::Size size;
  uint8_t qos;
  ARM::CHI::Order order;
  uint8_t pcrd_type;
  int initiator_index;
  uint64_t entry_time;
};

class TrafficProducer {
public:
  virtual bool generate_next(RequestDescriptor &desc) = 0;
  virtual ~TrafficProducer() = default;
};

class PatternTrafficProducer : public TrafficProducer {
public:
  explicit PatternTrafficProducer(const TrafficConfig &cfg);
  bool generate_next(RequestDescriptor &desc) override;

private:
  TrafficConfig config_;
  uint64_t generated_count_;
  uint64_t current_address_;
  std::mt19937 rng_;
  std::uniform_real_distribution<double> dist_;
  std::uniform_int_distribution<uint64_t> step_dist_;
};

class RetryQueueManager {
public:
  explicit RetryQueueManager(unsigned num_initiators);

  void enqueue_retry(int initiator_index, uint8_t pcrd_type,const RetryEntry &entry);
  bool has_resendable(int initiator_index) const;
  bool try_resend(int initiator_index, RetryEntry &out_entry);
  void add_pcrd_credit(int initiator_index, uint8_t pcrd_type);
  bool has_pending_for_address(uint64_t address) const;

private:
  std::vector<std::vector<std::deque<RetryEntry>>> retry_queues_;
  std::vector<std::vector<uint8_t>> pcrd_credits_;
  uint64_t consecutive_qos_resend_count_;
  static constexpr uint64_t STARVATION_THRESHOLD = 5;
  static constexpr uint8_t MAX_PCRD_TYPES = 16;
};

class TransactionTracker {
public:
  TransactionTracker(unsigned max_outstanding,const std::vector<int> &initiator_indices);

  bool allocate_slot(uint16_t &out_txn_id);
  void free_slot(uint16_t txn_id);
  bool is_full() const;
  unsigned outstanding_count() const;

  ActiveTransaction *get_transaction(uint16_t txn_id);
  void put_transaction(uint16_t txn_id, const ActiveTransaction &txn);
  void erase_transaction(uint16_t txn_id);

  bool is_address_locked(uint64_t address) const;
  void lock_address(uint64_t address);
  void unlock_address(uint64_t address);

  RetryQueueManager &retry_queue();
  const std::vector<int> &initiator_indices() const;

private:
  unsigned max_outstanding_;
  std::vector<int> initiator_indices_;
  std::vector<std::optional<ActiveTransaction>> slots_;
  std::queue<uint16_t> free_slots_;
  unsigned outstanding_count_;
  std::unordered_set<uint64_t> address_locks_;
  RetryQueueManager retry_queue_;
};

class CHITrafficGenerator : public sc_core::sc_module {
protected:
  SC_HAS_PROCESS(CHITrafficGenerator);

  struct InitiatorChannels {
    CHIChannelState channels[CHI_NUM_CHANNELS];
  };

  unsigned data_width_bytes_;
  TrafficConfig config_;
  uint64_t cycle_count_;

  std::vector<std::unique_ptr<TrafficProducer>> producers_;
  std::vector<std::deque<RequestDescriptor>> pending_queues_;
  std::vector<std::unique_ptr<TransactionTracker>> trackers_;
  std::vector<InitiatorChannels> initiator_channels_;
  std::unordered_map<uint16_t, int> src_id_to_initiator_index_;

  void clock_posedge();
  void clock_negedge();

  void handle_read_receipt(const CHIFlit &flit, int initiator_index);
  void handle_retry_ack(const CHIFlit &flit, int initiator_index);
  void handle_pcrd_grant(const CHIFlit &flit, int initiator_index);
  void handle_comp(const CHIFlit &flit, int initiator_index);
  void handle_dbid_resp(const CHIFlit &flit, int initiator_index);
  void handle_read_data(const CHIFlit &flit, int initiator_index);

  void dispatch_cycle();
  void dispatch_for_tracker(TransactionTracker &tracker,unsigned tracker_index);
  int select_initiator_for_address(uint64_t address) const;
  bool has_pending_or_retry_same_address(uint64_t address,unsigned tracker_index) const;
  TransactionTracker *find_tracker_for_initiator(int initiator_index);

  void assemble_and_send_request(uint64_t address, 
                                ARM::CHI::Size size,
                                bool is_read, 
                                int initiator_index,
                                uint16_t txn_id, 
                                bool allow_retry, 
                                uint8_t qos,
                                ARM::CHI::Order order, 
                                uint8_t pcrd_type);
  void assemble_and_send_write_data(const CHIFlit &dbid_flit,int initiator_index);

  ActiveTransaction make_active_transaction(uint16_t txn_id, 
                                            uint64_t address,
                                            ARM::CHI::Size size, 
                                            bool is_read,
                                            int initiator_index, 
                                            uint8_t qos,
                                            ARM::CHI::Order order);

  void complete_transaction(uint16_t txn_id, TransactionTracker &tracker);

  uint8_t determine_pcrd_type(bool is_read) const;

  tlm::tlm_sync_enum nb_transport_bw_0(ARM::CHI::Payload &payload,
                                       ARM::CHI::Phase &phase);
  tlm::tlm_sync_enum nb_transport_bw_1(ARM::CHI::Payload &payload,
                                       ARM::CHI::Phase &phase);
  tlm::tlm_sync_enum handle_nb_transport_bw(ARM::CHI::Payload &payload,
                                            ARM::CHI::Phase &phase,
                                            int initiator_index);

public:
  explicit CHITrafficGenerator(const sc_core::sc_module_name &name,
                               const TrafficConfig &config,
                               unsigned data_width_bits = 128);

  std::vector<std::unique_ptr<ARM::CHI::SimpleInitiatorSocket<CHITrafficGenerator>>> initiator;
  sc_core::sc_in<bool> clock;
};

} // namespace Port
} // namespace dmu

#endif // ARM_CHI_TRAFFIC_GENERATOR_H