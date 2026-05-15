#include <cstring>
#include <random>

#include "ARM/TLM/arm_chi_phase.h"
#include "CHIPort/CHITrafficGenerator.h"

namespace dmu {
namespace Port {

PatternTrafficProducer::PatternTrafficProducer(const TrafficConfig &cfg)
    : config_(cfg), generated_count_(0), current_address_(cfg.addr_start),
      rng_(std::random_device{}()), dist_(0.0, 1.0), step_dist_(0, 0) {}

bool PatternTrafficProducer::generate_next(RequestDescriptor &desc) {
  if (generated_count_ >= config_.max_generated_transactions) {
    return false;
  }

  desc.is_read = dist_(rng_) < config_.read_ratio;
  desc.size = ARM::CHI::SIZE_64;

  if (config_.sequential) {
    desc.address = current_address_;
    current_address_ += config_.addr_step;
    if (current_address_ > config_.addr_end) {
      current_address_ = config_.addr_start;
    }
  } else {
    uint64_t num_steps =
        (config_.addr_end - config_.addr_start) / config_.addr_step + 1;
    step_dist_ = std::uniform_int_distribution<uint64_t>(0, num_steps - 1);
    desc.address = config_.addr_start + step_dist_(rng_) * config_.addr_step;
  }

  generated_count_++;
  return true;
}

RetryQueueManager::RetryQueueManager(unsigned num_initiators)
    : retry_queues_(num_initiators,
                    std::vector<std::deque<RetryEntry>>(MAX_PCRD_TYPES)),
      pcrd_credits_(num_initiators, std::vector<uint8_t>(MAX_PCRD_TYPES, 0)),
      consecutive_qos_resend_count_(0) {}

void RetryQueueManager::enqueue_retry(int initiator_index, uint8_t pcrd_type,
                                      const RetryEntry &entry) {
  if (initiator_index < 0 ||
      static_cast<unsigned>(initiator_index) >= retry_queues_.size()) {
    return;
  }
  if (pcrd_type >= retry_queues_[initiator_index].size()) {
    return;
  }
  retry_queues_[initiator_index][pcrd_type].push_back(entry);
}

bool RetryQueueManager::has_resendable(int initiator_index) const {
  if (initiator_index < 0 ||
      static_cast<unsigned>(initiator_index) >= retry_queues_.size()) {
    return false;
  }
  for (uint8_t pcrd_type = 0; pcrd_type < retry_queues_[initiator_index].size();
       ++pcrd_type) {
    if (pcrd_credits_[initiator_index][pcrd_type] > 0 &&
        !retry_queues_[initiator_index][pcrd_type].empty()) {
      return true;
    }
  }
  return false;
}

bool RetryQueueManager::try_resend(int initiator_index, RetryEntry &out_entry) {
  if (initiator_index < 0 ||
      static_cast<unsigned>(initiator_index) >= retry_queues_.size()) {
    return false;
  }

  struct Candidate {
    uint8_t pcrd_type;
    size_t index;
    const RetryEntry *entry;
  };
  std::vector<Candidate> candidates;

  for (uint8_t pt = 0; pt < retry_queues_[initiator_index].size(); ++pt) {
    if (pcrd_credits_[initiator_index][pt] == 0)
      continue;
    auto &q = retry_queues_[initiator_index][pt];
    for (size_t i = 0; i < q.size(); ++i) {
      candidates.push_back({pt, i, &q[i]});
    }
  }

  if (candidates.empty())
    return false;

  auto compare_qos = [](const Candidate &a, const Candidate &b) {
    if (a.entry->qos != b.entry->qos)
      return a.entry->qos < b.entry->qos;
    return a.entry->entry_time > b.entry->entry_time;
  };

  auto compare_age = [](const Candidate &a, const Candidate &b) {
    return a.entry->entry_time > b.entry->entry_time;
  };

  size_t selected = 0;
  if (consecutive_qos_resend_count_ >= STARVATION_THRESHOLD) {
    selected =
        std::min_element(candidates.begin(), candidates.end(), compare_age) -
        candidates.begin();
    consecutive_qos_resend_count_ = 0;
  } else {
    selected =
        std::min_element(candidates.begin(), candidates.end(), compare_qos) -
        candidates.begin();
    consecutive_qos_resend_count_++;
  }

  uint8_t pt = candidates[selected].pcrd_type;
  size_t idx = candidates[selected].index;
  out_entry = *candidates[selected].entry;
  retry_queues_[initiator_index][pt].erase(
      retry_queues_[initiator_index][pt].begin() + idx);
  pcrd_credits_[initiator_index][pt]--;

  return true;
}

void RetryQueueManager::add_pcrd_credit(int initiator_index,
                                        uint8_t pcrd_type) {
  if (initiator_index >= 0 &&
      static_cast<unsigned>(initiator_index) < pcrd_credits_.size() &&
      pcrd_type < pcrd_credits_[initiator_index].size()) {
    pcrd_credits_[initiator_index][pcrd_type]++;
  }
}

bool RetryQueueManager::has_pending_for_address(uint64_t address) const {
  for (const auto &per_initiator : retry_queues_) {
    for (const auto &per_pcrd : per_initiator) {
      for (const auto &entry : per_pcrd) {
        if (entry.address == address)
          return true;
      }
    }
  }
  return false;
}

TransactionTracker::TransactionTracker(
    unsigned max_outstanding, const std::vector<int> &initiator_indices)
    : max_outstanding_(max_outstanding), initiator_indices_(initiator_indices),
      slots_(max_outstanding), outstanding_count_(0),
      retry_queue_(initiator_indices.empty()
                       ? 1
                       : static_cast<unsigned>(initiator_indices.size())) {
  for (uint16_t i = 0; i < max_outstanding; ++i) {
    free_slots_.push(i);
  }
}

bool TransactionTracker::allocate_slot(uint16_t &out_txn_id) {
  if (free_slots_.empty())
    return false;
  out_txn_id = free_slots_.front();
  free_slots_.pop();
  outstanding_count_++;
  return true;
}

void TransactionTracker::free_slot(uint16_t txn_id) {
  if (txn_id < slots_.size()) {
    slots_[txn_id].reset();
    free_slots_.push(txn_id);
    outstanding_count_--;
  }
}

bool TransactionTracker::is_full() const {
  return outstanding_count_ >= max_outstanding_;
}

unsigned TransactionTracker::outstanding_count() const {
  return outstanding_count_;
}

ActiveTransaction *TransactionTracker::get_transaction(uint16_t txn_id) {
  if (txn_id < slots_.size() && slots_[txn_id].has_value()) {
    return &slots_[txn_id].value();
  }
  return nullptr;
}

void TransactionTracker::put_transaction(uint16_t txn_id,
                                         const ActiveTransaction &txn) {
  if (txn_id < slots_.size()) {
    slots_[txn_id] = txn;
  }
}

void TransactionTracker::erase_transaction(uint16_t txn_id) {
  if (txn_id < slots_.size()) {
    slots_[txn_id].reset();
  }
}

bool TransactionTracker::is_address_locked(uint64_t address) const {
  return address_locks_.count(address) > 0;
}

void TransactionTracker::lock_address(uint64_t address) {
  address_locks_.insert(address);
}

void TransactionTracker::unlock_address(uint64_t address) {
  address_locks_.erase(address);
}

RetryQueueManager &TransactionTracker::retry_queue() { return retry_queue_; }

const std::vector<int> &TransactionTracker::initiator_indices() const {
  return initiator_indices_;
}

CHITrafficGenerator::CHITrafficGenerator(const sc_core::sc_module_name &name,
                                         const TrafficConfig &config,
                                         unsigned data_width_bits)
    : sc_module(name), data_width_bytes_(data_width_bits / 8), config_(config),
      cycle_count_(0) {
  if (config_.num_initiators != 1 && config_.num_initiators != 2) {
    SC_REPORT_ERROR(name, "num_initiators must be 1 or 2");
  }
  if (config_.num_trackers > config_.num_initiators) {
    SC_REPORT_ERROR(name, "num_trackers must be <= num_initiators");
  }

  if (config_.initiator_src_ids.size() != config_.num_initiators) {
    config_.initiator_src_ids.resize(config_.num_initiators);
    for (unsigned i = 0; i < config_.num_initiators; ++i) {
      config_.initiator_src_ids[i] = config_.src_id + i;
    }
  }
  if (config_.initiator_tgt_ids.size() != config_.num_initiators) {
    config_.initiator_tgt_ids.resize(config_.num_initiators);
    for (unsigned i = 0; i < config_.num_initiators; ++i) {
      config_.initiator_tgt_ids[i] = config_.tgt_id + i;
    }
  }

  for (unsigned i = 0; i < config_.num_initiators; ++i) {
    src_id_to_initiator_index_[config_.initiator_src_ids[i]] =
        static_cast<int>(i);
  }

  initiator.resize(config_.num_initiators);
  initiator_channels_.resize(config_.num_initiators);
  for (unsigned i = 0; i < config_.num_initiators; ++i) {
    std::string socket_name =
        std::string(name) + ".initiator_" + std::to_string(i);
    if (i == 0) {
      initiator[i] = std::make_unique<
          ARM::CHI::SimpleInitiatorSocket<CHITrafficGenerator>>(
          socket_name.c_str(), *this, &CHITrafficGenerator::nb_transport_bw_0,
          ARM::TLM::PROTOCOL_CHI_E, data_width_bits);
    } else if (i == 1) {
      initiator[i] = std::make_unique<
          ARM::CHI::SimpleInitiatorSocket<CHITrafficGenerator>>(
          socket_name.c_str(), *this, &CHITrafficGenerator::nb_transport_bw_1,
          ARM::TLM::PROTOCOL_CHI_E, data_width_bits);
    }

    initiator_channels_[i]
        .channels[ARM::CHI::CHANNEL_RSP]
        .rx_credits_available = CHI_MAX_LINK_CREDITS;
    initiator_channels_[i]
        .channels[ARM::CHI::CHANNEL_DAT]
        .rx_credits_available = CHI_MAX_LINK_CREDITS;
  }

  trackers_.resize(config_.num_trackers);
  producers_.resize(config_.num_trackers);
  pending_queues_.resize(config_.num_trackers);

  for (unsigned t = 0; t < config_.num_trackers; ++t) {
    std::vector<int> tracker_initiators;
    if (config_.num_trackers == config_.num_initiators) {
      tracker_initiators.push_back(static_cast<int>(t));
    } else {
      for (unsigned i = 0; i < config_.num_initiators; ++i) {
        tracker_initiators.push_back(static_cast<int>(i));
      }
    }
    trackers_[t] = std::make_unique<TransactionTracker>(config_.max_outstanding,
                                                        tracker_initiators);
    producers_[t] = std::make_unique<PatternTrafficProducer>(config_);
  }

  SC_METHOD(clock_posedge);
  sensitive << clock.pos();
  dont_initialize();

  SC_METHOD(clock_negedge);
  sensitive << clock.neg();
  dont_initialize();
}

TransactionTracker *
CHITrafficGenerator::find_tracker_for_initiator(int initiator_index) {
  if (config_.num_trackers == config_.num_initiators) {
    if (initiator_index >= 0 &&
        static_cast<unsigned>(initiator_index) < trackers_.size()) {
      return trackers_[initiator_index].get();
    }
  } else {
    if (!trackers_.empty()) {
      return trackers_[0].get();
    }
  }
  return nullptr;
}

uint8_t CHITrafficGenerator::determine_pcrd_type(bool is_read) const {
  if (is_read) {
    return 0;
  } else {
    return 3;
  }
}

ActiveTransaction CHITrafficGenerator::make_active_transaction(
    uint16_t txn_id, uint64_t address, ARM::CHI::Size size, bool is_read,
    int initiator_index, uint8_t qos, ARM::CHI::Order order) {
  ActiveTransaction txn;
  txn.txn_id = txn_id;
  txn.address = address;
  txn.is_read = is_read;
  txn.size = size;
  txn.state = TransactionState::REQ_SENT;
  txn.initiator_index = initiator_index;
  txn.src_id = config_.initiator_src_ids[initiator_index];
  txn.tgt_id = config_.initiator_tgt_ids[initiator_index];
  txn.qos = qos;
  txn.order = order;
  if (is_read) {
    unsigned size_bytes = 1u << static_cast<unsigned>(size);
    txn.data_beats_remaining = (size_bytes <= data_width_bytes_)
                                   ? 1
                                   : (size_bytes / data_width_bytes_);
  } else {
    txn.data_beats_remaining = 0;
  }
  return txn;
}

void CHITrafficGenerator::clock_posedge() {
  cycle_count_++;

  for (unsigned i = 0; i < config_.num_initiators; ++i) {
    auto &rsp_channel = initiator_channels_[i].channels[ARM::CHI::CHANNEL_RSP];
    if (!rsp_channel.rx_queue.empty()) {
      CHIFlit rsp_flit = rsp_channel.rx_queue.front();
      rsp_channel.rx_queue.pop_front();

      switch (rsp_flit.phase.rsp_opcode) {
      case ARM::CHI::RSP_OPCODE_READ_RECEIPT:
        handle_read_receipt(rsp_flit, static_cast<int>(i));
        break;
      case ARM::CHI::RSP_OPCODE_RETRY_ACK:
        handle_retry_ack(rsp_flit, static_cast<int>(i));
        break;
      case ARM::CHI::RSP_OPCODE_PCRD_GRANT:
        handle_pcrd_grant(rsp_flit, static_cast<int>(i));
        break;
      case ARM::CHI::RSP_OPCODE_COMP:
        handle_comp(rsp_flit, static_cast<int>(i));
        break;
      case ARM::CHI::RSP_OPCODE_COMP_DBID_RESP:
      case ARM::CHI::RSP_OPCODE_DBID_RESP:
      case ARM::CHI::RSP_OPCODE_DBID_RESP_ORD:
        handle_dbid_resp(rsp_flit, static_cast<int>(i));
        break;
      default:
        SC_REPORT_ERROR(name(), "unexpected response opcode received");
      }
    }

    auto &dat_channel = initiator_channels_[i].channels[ARM::CHI::CHANNEL_DAT];
    if (!dat_channel.rx_queue.empty()) {
      CHIFlit dat_flit = dat_channel.rx_queue.front();
      dat_channel.rx_queue.pop_front();

      switch (dat_flit.phase.dat_opcode) {
      case ARM::CHI::DAT_OPCODE_COMP_DATA:
      case ARM::CHI::DAT_OPCODE_DATA_SEP_RESP:
        handle_read_data(dat_flit, static_cast<int>(i));
        break;
      default:
        SC_REPORT_ERROR(name(), "unexpected data opcode received");
      }
    }
  }
}

void CHITrafficGenerator::clock_negedge() {
  for (unsigned t = 0; t < config_.num_trackers; ++t) {
    RequestDescriptor desc;
    if (producers_[t]->generate_next(desc)) {
      pending_queues_[t].push_back(desc);
    }
  }

  dispatch_cycle();

  for (unsigned i = 0; i < config_.num_initiators; ++i) {
    for (const auto channel : {ARM::CHI::CHANNEL_REQ, ARM::CHI::CHANNEL_RSP,
                               ARM::CHI::CHANNEL_DAT}) {
      initiator_channels_[i].channels[channel].send_flits(
          channel,
          [this, i](ARM::CHI::Payload &payload, ARM::CHI::Phase &phase) {
            return initiator[i]->nb_transport_fw(payload, phase);
          });
    }
  }
}

void CHITrafficGenerator::handle_read_receipt(const CHIFlit &flit,
                                              int initiator_index) {
  TransactionTracker *tracker = find_tracker_for_initiator(initiator_index);
  if (!tracker)
    return;

  ActiveTransaction *txn = tracker->get_transaction(flit.phase.txn_id);
  if (!txn) {
    SC_REPORT_ERROR(name(), "ReadReceipt for unknown transaction");
    return;
  }

  if (txn->is_read && txn->order == ARM::CHI::ORDER_REQUEST_ACCEPTED) {
    tracker->unlock_address(txn->address);
  }
}

void CHITrafficGenerator::handle_retry_ack(const CHIFlit &flit,
                                           int initiator_index) {
  TransactionTracker *tracker = find_tracker_for_initiator(initiator_index);
  if (!tracker)
    return;

  ActiveTransaction *txn = tracker->get_transaction(flit.phase.txn_id);
  if (!txn) {
    SC_REPORT_ERROR(name(), "RetryAck for unknown transaction");
    return;
  }

  RetryEntry re;
  re.address = txn->address;
  re.is_read = txn->is_read;
  re.size = txn->size;
  re.qos = txn->qos;
  re.order = txn->order;
  re.pcrd_type = determine_pcrd_type(txn->is_read);
  re.initiator_index = initiator_index;
  re.entry_time = cycle_count_;

  tracker->retry_queue().enqueue_retry(initiator_index, re.pcrd_type, re);
  tracker->erase_transaction(txn->txn_id);
  tracker->free_slot(txn->txn_id);
}

void CHITrafficGenerator::handle_pcrd_grant(const CHIFlit &flit,
                                            int initiator_index) {
  TransactionTracker *tracker = find_tracker_for_initiator(initiator_index);
  if (!tracker)
    return;

  tracker->retry_queue().add_pcrd_credit(initiator_index, flit.phase.pcrd_type);
}

void CHITrafficGenerator::handle_comp(const CHIFlit &flit,
                                      int initiator_index) {
  TransactionTracker *tracker = find_tracker_for_initiator(initiator_index);
  if (!tracker)
    return;

  ActiveTransaction *txn = tracker->get_transaction(flit.phase.txn_id);
  if (!txn) {
    SC_REPORT_ERROR(name(), "Comp for unknown transaction");
    return;
  }

  tracker->unlock_address(txn->address);
  complete_transaction(flit.phase.txn_id, *tracker);
}

void CHITrafficGenerator::handle_dbid_resp(const CHIFlit &flit,
                                           int initiator_index) {
  TransactionTracker *tracker = find_tracker_for_initiator(initiator_index);
  if (!tracker)
    return;

  ActiveTransaction *txn = tracker->get_transaction(flit.phase.txn_id);
  if (!txn) {
    SC_REPORT_ERROR(name(), "DBIDResp for unknown transaction");
    return;
  }

  assemble_and_send_write_data(flit, initiator_index);
  txn->state = TransactionState::WRITE_WAIT_COMP;
}

void CHITrafficGenerator::handle_read_data(const CHIFlit &flit,
                                           int initiator_index) {
  TransactionTracker *tracker = find_tracker_for_initiator(initiator_index);
  if (!tracker)
    return;

  ActiveTransaction *txn = tracker->get_transaction(flit.phase.dbid);
  if (!txn) {
    SC_REPORT_ERROR(name(), "ReadData for unknown transaction");
    return;
  }

  txn->data_beats_remaining--;
  if (txn->data_beats_remaining == 0) {
    if (txn->order == ARM::CHI::ORDER_REQUEST_ACCEPTED) {
      tracker->unlock_address(txn->address);
    }
    complete_transaction(flit.phase.txn_id, *tracker);
  } else if (txn->state == TransactionState::REQ_SENT) {
    txn->state = TransactionState::READ_WAIT_DATA;
  }
}

void CHITrafficGenerator::dispatch_cycle() {
  for (unsigned t = 0; t < config_.num_trackers; ++t) {
    dispatch_for_tracker(*trackers_[t], t);
  }
}

void CHITrafficGenerator::dispatch_for_tracker(TransactionTracker &tracker,
                                               unsigned tracker_index) {
  const auto &initiator_indices = tracker.initiator_indices();

  for (int initiator_idx : initiator_indices) {
    if (tracker.retry_queue().has_resendable(initiator_idx)) {
      RetryEntry re;
      if (tracker.retry_queue().try_resend(initiator_idx, re)) {
        uint16_t txn_id;
        if (!tracker.allocate_slot(txn_id)) {
          SC_REPORT_ERROR(name(), "Failed to allocate slot for retry resend");
          return;
        }
        assemble_and_send_request(re.address, re.size, re.is_read,
                                  initiator_idx, txn_id, false, re.qos,
                                  re.order, re.pcrd_type);
        tracker.put_transaction(
            txn_id,
            make_active_transaction(txn_id, re.address, re.size, re.is_read,
                                    initiator_idx, re.qos, re.order));
        if (re.is_read && re.order == ARM::CHI::ORDER_REQUEST_ACCEPTED) {
          tracker.lock_address(re.address);
        } else if (!re.is_read) {
          tracker.lock_address(re.address);
        }
        return;
      }
    }
  }

  if (tracker.is_full()) {
    return;
  }

  auto &queue = pending_queues_[tracker_index];
  if (queue.empty()) {
    return;
  }

  RequestDescriptor desc = queue.front();
  queue.pop_front();

  bool needs_order = false;
  if (desc.is_read &&
      has_pending_or_retry_same_address(desc.address, tracker_index)) {
    needs_order = true;
  }

  if (tracker.is_address_locked(desc.address)) {
    queue.push_front(desc);
    return;
  }

  uint16_t txn_id;
  if (!tracker.allocate_slot(txn_id)) {
    queue.push_front(desc);
    return;
  }

  int initiator_idx = select_initiator_for_address(desc.address);
  if (initiator_idx < 0 ||
      static_cast<unsigned>(initiator_idx) >= config_.num_initiators) {
    queue.push_front(desc);
    tracker.free_slot(txn_id);
    return;
  }

  uint8_t qos = config_.qos;
  ARM::CHI::Order order = config_.order;
  if (desc.is_read && needs_order) {
    order = ARM::CHI::ORDER_REQUEST_ACCEPTED;
  }
  uint8_t pcrd_type = determine_pcrd_type(desc.is_read);

  assemble_and_send_request(desc.address, desc.size, desc.is_read,
                            initiator_idx, txn_id, true, qos, order, pcrd_type);

  tracker.put_transaction(
      txn_id, make_active_transaction(txn_id, desc.address, desc.size,
                                      desc.is_read, initiator_idx, qos, order));

  if (desc.is_read && order == ARM::CHI::ORDER_REQUEST_ACCEPTED) {
    tracker.lock_address(desc.address);
  } else if (!desc.is_read) {
    tracker.lock_address(desc.address);
  }
}

int CHITrafficGenerator::select_initiator_for_address(uint64_t address) const {
  if (config_.num_initiators == 1) {
    return 0;
  }
  return (address >> config_.addr_target_select_bit) & 0x1;
}

bool CHITrafficGenerator::has_pending_or_retry_same_address(
    uint64_t address, unsigned tracker_index) const {
  const auto &queue = pending_queues_[tracker_index];
  for (const auto &desc : queue) {
    if (desc.address == address)
      return true;
  }
  return trackers_[tracker_index]->retry_queue().has_pending_for_address(
      address);
}

void CHITrafficGenerator::assemble_and_send_request(
    uint64_t address, ARM::CHI::Size size, bool is_read, int initiator_index,
    uint16_t txn_id, bool allow_retry, uint8_t qos, ARM::CHI::Order order,
    uint8_t pcrd_type) {
  ARM::CHI::Payload &req_payload = *ARM::CHI::Payload::new_payload();
  ARM::CHI::Phase req_phase;

  req_phase.src_id = config_.initiator_src_ids[initiator_index];
  req_phase.tgt_id = config_.initiator_tgt_ids[initiator_index];
  req_phase.txn_id = txn_id;
  req_phase.return_txn_id = txn_id;
  req_phase.order = order;
  req_phase.qos = qos;
  req_phase.allow_retry = allow_retry;
  req_phase.pcrd_type = pcrd_type;
  req_phase.channel = ARM::CHI::CHANNEL_REQ;

  if (is_read) {
    req_phase.req_opcode = ARM::CHI::REQ_OPCODE_READ_NO_SNP;
  } else {
    req_phase.req_opcode = config_.write_ptl
                               ? ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_PTL
                               : ARM::CHI::REQ_OPCODE_WRITE_NO_SNP_FULL;
  }

  req_payload.address = address;
  req_payload.size = size;
  req_payload.mem_attr = config_.mem_attr;

  initiator_channels_[initiator_index]
      .channels[ARM::CHI::CHANNEL_REQ]
      .tx_queue.emplace_back(req_payload, req_phase);

  req_payload.unref();
}

void CHITrafficGenerator::assemble_and_send_write_data(const CHIFlit &dbid_flit,
                                                       int initiator_index) {
  dbid_flit.payload.byte_enable =
      transaction_valid_bytes_mask(dbid_flit.payload);
  std::memset(dbid_flit.payload.data, dbid_flit.phase.txn_id,
              CHI_CACHE_LINE_SIZE_BYTES);

  ARM::CHI::Phase dat_phase;
  dat_phase.channel = ARM::CHI::CHANNEL_DAT;
  dat_phase.qos = dbid_flit.phase.qos;
  dat_phase.tgt_id = dbid_flit.phase.src_id;
  dat_phase.src_id = dbid_flit.phase.tgt_id;
  dat_phase.txn_id = dbid_flit.phase.dbid;
  dat_phase.dat_opcode = ARM::CHI::DAT_OPCODE_NON_COPY_BACK_WR_DATA;
  dat_phase.resp = ARM::CHI::RESP_I;

  for (const auto data_id :
       transaction_data_ids(dbid_flit.payload, data_width_bytes_)) {
    dat_phase.data_id = data_id;
    initiator_channels_[initiator_index]
        .channels[ARM::CHI::CHANNEL_DAT]
        .tx_queue.emplace_back(dbid_flit.payload, dat_phase);
  }
}

void CHITrafficGenerator::complete_transaction(uint16_t txn_id,
                                               TransactionTracker &tracker) {
  tracker.erase_transaction(txn_id);
  tracker.free_slot(txn_id);
}

tlm::tlm_sync_enum
CHITrafficGenerator::nb_transport_bw_0(ARM::CHI::Payload &payload,
                                       ARM::CHI::Phase &phase) {
  return handle_nb_transport_bw(payload, phase, 0);
}

tlm::tlm_sync_enum
CHITrafficGenerator::nb_transport_bw_1(ARM::CHI::Payload &payload,
                                       ARM::CHI::Phase &phase) {
  return handle_nb_transport_bw(payload, phase, 1);
}

tlm::tlm_sync_enum CHITrafficGenerator::handle_nb_transport_bw(
    ARM::CHI::Payload &payload, ARM::CHI::Phase &phase, int initiator_index) {
  if (!initiator_channels_[initiator_index]
           .channels[phase.channel]
           .receive_flit(payload, phase)) {
    SC_REPORT_ERROR(name(), "flit on inactive channel received");
  }
  return tlm::TLM_ACCEPTED;
}

} // namespace Port
} // namespace dmu