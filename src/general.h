#ifndef GENERAL_H_
#define GENERAL_H_

#include <exception>
#include <experimental/optional>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "log.h"
#include "message.h"
#include "net.h"
#include "udp_conn.h"

namespace generals {

// Holds a list of processes participating in the agreement algorithm.
typedef std::vector<net::Address> ProcessList;

const struct timeval kAckTimeout = {0, 250000};
const struct timeval kRoundTimeout = {1, 000000};
const unsigned int kSendAttempts = 3;

// TODO
size_t MessagesPerRound(size_t process_num, unsigned int round);

// TODO
void SendMessage(udp::ClientPtr client, const msg::Message& msg);
// TODO
void SendAckForRound(udp::ClientPtr client, unsigned int round);

// TODO
std::experimental::optional<msg::Message> ByzantineMsgFromBuf(char* buf,
                                                              size_t n);

// TODO
std::experimental::optional<unsigned int> RoundOfAck(char* buf, size_t n);

// TODO
class General {
 public:
  General(const ProcessList processes, unsigned int id, unsigned int faulty)
      : processes_(processes), id_(id), faulty_(faulty), round_(0) {
    for (auto const& addr : processes_) {
      clients_.emplace(addr, std::make_shared<udp::Client>(addr, kAckTimeout));
    }
  }

  virtual ~General() = default;

  virtual msg::Order Decide() = 0;

 protected:
  const ProcessList processes_;
  const unsigned int id_;
  const unsigned int faulty_;

  std::unordered_map<net::Address, udp::ClientPtr> clients_;

  unsigned int round_;

  inline void IncrementRound() { round_++; };
};

// TODO
class Commander : public General {
 public:
  Commander(const ProcessList processes, unsigned int faulty, msg::Order order)
      : General(processes, 0, faulty), order_(order) {}

  msg::Order Decide();

 private:
  msg::Order order_;
};

// TODO
class Lieutenant : public General {
 public:
  Lieutenant(const ProcessList processes, unsigned int id,
             unsigned short server_port, unsigned int faulty)
      : General(processes, id, faulty), server_(server_port, kRoundTimeout) {}

  msg::Order Decide();

 private:
  const udp::Server server_;

  std::set<msg::Order> orders_seen_;
  std::set<msg::Message> msgs_this_round_;
  // Same as orders_last_round_, except with only the ids so that all messages
  // from the same process colide.
  std::set<std::vector<unsigned int>> ids_this_round_;
  // Holds the sender threads for the given round.
  std::vector<std::thread> sender_threads_this_round_;

  void ClearSenders();
  void BeginNewRound();

  // Validates that the message makes sense in the current context of the
  // algorithm and verifies that it is properly formatted. This protects against
  // malicious messages.
  bool ValidMessage(const msg::Message& msg, const net::Address& from) const;

  // TODO
  msg::Order DecideOrder() const;

  // Decides if the current round is complete based on the number of messages
  // received.
  inline bool RoundComplete() const;
};

}  // namespace generals

#endif
