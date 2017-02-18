#ifndef GENERAL_H_
#define GENERAL_H_

#include <chrono>
#include <exception>
#include <experimental/optional>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "log.h"
#include "message.h"
#include "net.h"
#include "thread.h"
#include "udp_conn.h"

namespace generals {

const auto kAckTimeout = std::chrono::milliseconds{250};
const auto kRoundTimeout = std::chrono::seconds{1};
const unsigned int kSendAttempts = 3;

// Determines the maximum number of valid messages that a Lieutenant process
// should expect in a certain round given a number of initial processes.
size_t MessagesForRound(size_t process_num, unsigned int round);

// Decodes a msg::Message from the provided buffer. If the decoding is
// successful, the optional return value will be present. If not, the return
// value will be absent.
std::experimental::optional<msg::Message> ByzantineMsgFromBuf(char* buf,
                                                              size_t n);

// Decodes a msg::Ack from the provided buffer and returns its round number. If
// the decoding is successful, the optional return value will be present. If
// not, the return value will be absent.
std::experimental::optional<unsigned int> RoundOfAck(char* buf, size_t n);

// Sends the message to the client.
void SendMessage(udp::ClientPtr client, const msg::Message& msg);

// Sends an acknowledgement for the provided round to the client.
void SendAckForRound(udp::ClientPtr client, unsigned int round);

// Holds a list of processes participating in the agreement algorithm.
typedef std::vector<net::Address> ProcessList;

// Creates a mapping from network addresses to UDP clients, populated with each
// process provided.
std::unordered_map<net::Address, udp::ClientPtr> ClientsForProcessList(
    const ProcessList processes);

// A abstract representation of a general process in the Byzantine Agreement
// Algorithm. Extended by the Commander and Lieutenant classes.
class General {
 public:
  General(const ProcessList processes, unsigned int id, unsigned int faulty)
      : processes_(processes),
        clients_(ClientsForProcessList(processes)),
        id_(id),
        faulty_(faulty),
        round_(0) {}

  virtual ~General() = default;

  // Runs the Byzantine Agreement Algorithm and decides on an order by
  // coordinating with peer processes.
  virtual msg::Order Decide() = 0;

 protected:
  const ProcessList processes_;
  const std::unordered_map<net::Address, udp::ClientPtr> clients_;
  const unsigned int id_;
  const unsigned int faulty_;

  unsigned int round_;
  // Determines if this is the first round of the algorithm.
  inline bool FirstRound() const { return round_ == 0; }
  // Determines if this is the last round of the algorithm.
  inline bool LastRound() const { return round_ == faulty_ + 1; };
  // Increments the round number.
  inline void IncrementRound() {
    round_++;
    logging::out << "Moving to round " << round_ << "\n";
  };
};

// A representation of a commander process in the Byzantine Agreement Algorithm.
class Commander : public General {
 public:
  Commander(const ProcessList processes, unsigned int faulty, msg::Order order)
      : General(processes, 0, faulty), order_(order) {}

  msg::Order Decide();

 private:
  msg::Order order_;
};

// A representation of a lieutenant process in the Byzantine Agreement
// Algorithm.
class Lieutenant : public General {
 public:
  Lieutenant(const ProcessList processes, unsigned int id,
             unsigned short server_port, unsigned int faulty)
      : General(processes, id, faulty), server_(server_port, kRoundTimeout) {}

  msg::Order Decide();

 private:
  const udp::Server server_;

  // The set of unique orders seen orders over the course of the agreement
  // algorithm.
  std::set<msg::Order> orders_seen_;

  // Decides what the order should be based on the seen orders over the course
  // of the agreement algorithm. Defined as follows:
  //
  // choice(V) := v        if V = {v}
  //            | RETREAT  if V = {} or |V| >= 2
  //
  inline msg::Order DecideOrder() const;

  // Per-round variables:

  // Timestamp at the begining of the round, used as a backup round timeout
  // because socket timeouts alone are not sufficient (see
  // ContinueUnlessTimeout). steady_clock (monotonic) to measure elapsed time
  // accurately even in the face of clock resets.
  std::chrono::steady_clock::time_point round_start_ts_;
  // Contains the set of all unique messages received so far this round.
  std::set<msg::Message> msgs_this_round_;
  // Same as msgs_this_round_, except with only the ids so that all messages
  // with the same process list collide.
  std::set<std::vector<unsigned int>> ids_this_round_;
  // Holds the sender threads for the given round.
  ThreadGroup sender_threads_this_round_;

  // Decides if the current round is complete based on the number of messages
  // received.
  inline bool RoundComplete() const;

  // Handles moving to the next round, unless this is as already the last round.
  udp::ServerAction MoveToNewRoundOrStop();
  // Checks if the round has timed out and returns an action accordingly. If the
  // round has not yet timed out, the server will be told to continue. We need
  // both a round timeout and a socket timeout so that faulty processes cannot
  // continue to send messages to reset the socket timeout without ever actually
  // making forward progress.
  udp::ServerAction ContinueUnlessTimeout();
  // Handles a round timeout, moving to the next round if necessary.
  udp::ServerAction HandleRoundTimeout();

  // Waits for all sender threads to drain and terminate before clearing the
  // sender_threads_this_round_ vector.
  void ClearSenders();
  // Handles a new round by setting up per-round variables and launching threads
  // (senders) to send round related messages.
  void InitNewRound();

  // Validates that the message makes sense in the current context of the
  // algorithm and verifies that it is properly formatted. This protects against
  // malicious messages.
  bool ValidMessage(const msg::Message& msg, const net::Address& from) const;
};

}  // namespace generals

#endif
