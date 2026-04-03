#ifndef __CHI_LINK_HH__
#define __CHI_LINK_HH__

#include <cstdint>
#include <deque>

#include <systemc>

#include "CHIPort/CHIUtilities.h"
#include "sysc/kernel/sc_simcontext.h"

namespace dmu {
namespace Port {

struct CHILink {
  std::deque<CHIFlit> tx_queue;
  std::deque<CHIFlit> rx_queue;

  /* Link credits issued to us by our peer that we can use. */
  uint8_t tx_credits_available = 0;

  /* If >= 0, link credits we can issue to our peer for them to use.  If < 0, channel disabled. */
  int8_t rx_credits_available = -1;
  bool rx_credit_increment = false;
  uint8_t rx_credit_sent_upstream = 0;

  // void rx_credits_update()
  // {
  //     if(rx_credit_increment)
  //         rx_credits_available++;
  //     rx_credit_increment = false;
  // }
  bool receive_flit(ARM::CHI::Payload &payload, ARM::CHI::Phase &phase) 
  {
    if (phase.lcrd) 
    {
      tx_credits_available++;
    } else 
    {
      if (rx_credits_available < 0)
        return false;

      // rx_credit_increment = true;
      rx_credit_sent_upstream--;
      CHIFlit flit(payload, phase);
      flit.RecordEnteringPortTime(sc_core::sc_time_stamp());
      rx_queue.emplace_back(flit);
    }

    return true;
  }

  inline bool has_credits() {
    return rx_credit_sent_upstream + rx_queue.size() < rx_credits_available;
  }

  template <typename F>
  void send_flits(const ARM::CHI::Channel channel, F nb_transporter) 
  {
    if (static_cast<int>(rx_credit_sent_upstream + rx_queue.size()) < rx_credits_available) 
    {
      ARM::CHI::Payload *const payload = ARM::CHI::Payload::get_dummy();
      ARM::CHI::Phase phase;

      phase.channel = channel;
      phase.lcrd = true;

      rx_credit_sent_upstream++;
      nb_transporter(*payload, phase);
    }

    if (!tx_queue.empty() && tx_credits_available > 0) 
    {
      CHIFlit tx_flit = tx_queue.front();
      tx_queue.pop_front();

      tx_credits_available--;
      nb_transporter(tx_flit.payload, tx_flit.phase);
    }
  }
};


  } 
}

#endif