#ifndef GENERAL_H_
#define GENERAL_H_

#include <deque>
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

typedef std::deque<net::Address> ProcessList;

const int kAckTimeoutUSec = 250000;
const int kSendAttempts = 3;

int MessagesPerRound(int process_num, int round);

void SendMessage(udp::ClientPtr client, const msg::Message& msg);
void SendAckForRound(udp::ClientPtr client, int round);

std::experimental::optional<msg::Message> ByzantineMsgFromBuf(char* buf,
                                                              size_t n);
std::experimental::optional<int> RoundOfAck(char* buf, size_t n);

// TODO
class General {
 public:
  General(ProcessList processes, int id, int faulty)
      : processes_(processes), id_(id), faulty_(faulty), round_(0) {
    for (auto const& addr : processes_) {
      clients_.emplace(addr,
                       std::make_shared<udp::Client>(addr, kAckTimeoutUSec));
    }
  }

  virtual ~General() = default;

  virtual msg::Order Decide() = 0;

 protected:
  const ProcessList processes_;
  const int id_;
  const int faulty_;

  std::unordered_map<net::Address, udp::ClientPtr> clients_;

  int round_;
};

// TODO
class Commander : public General {
 public:
  Commander(ProcessList processes, int faulty, msg::Order order)
      : General(processes, 0, faulty), order_(order) {}

  msg::Order Decide();

 private:
  msg::Order order_;
};

// TODO
class Lieutenant : public General {
 public:
  Lieutenant(ProcessList processes, int id, unsigned short server_port,
             int faulty)
      : General(processes, id, faulty), server_(server_port) {}

  msg::Order Decide();

 private:
  const udp::Server server_;

  std::set<msg::Order> orders_seen_;
  std::set<msg::Message> msgs_this_round_;
  // Same as orders_last_round_, except with only the ids so that all messages
  // from the same process colide.
  std::set<std::vector<int>> ids_this_round_;
  std::vector<std::thread> sender_threads_this_round_;

  void ClearSenders();
  void BeginNewRound();

  // Validates that the message makes sense in the current context of the
  // algorithm and verifies that it is properly formatted. This protects against
  // malicious messages.
  bool ValidMessage(const msg::Message& msg, const net::Address& from) const;

  // TODO
  msg::Order DecideOrder() const;
  bool RoundOver() const;
};

#endif
