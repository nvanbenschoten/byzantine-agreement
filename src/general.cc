#include "general.h"

namespace generals {

size_t MessagesForRound(size_t process_num, unsigned int round) {
  if (round == 0) return 1;
  return (process_num - 1 - round) * MessagesForRound(process_num, round - 1);
}

std::experimental::optional<msg::Message> ByzantineMsgFromBuf(char* buf,
                                                              size_t n) {
  // Check to make sure the size of the buffer is correct.
  if (n < sizeof(msg::ByzantineMessage)) {
    return {};
  }

  // Copy out the message part.
  msg::Message msg;
  msg::ByzantineMessage* c_msg = reinterpret_cast<msg::ByzantineMessage*>(buf);
  msg.round = ntohl(c_msg->round);
  msg.order = static_cast<msg::Order>(ntohl(c_msg->order));

  msg.ids.resize((n - sizeof(*c_msg)) / sizeof(uint32_t));
  uint32_t* id_buf = reinterpret_cast<uint32_t*>(buf + sizeof(*c_msg));
  for (size_t i = 0; i < msg.ids.size(); ++i) {
    msg.ids[i] = ntohl(id_buf[i]);
  }

  return msg;
}

std::experimental::optional<unsigned int> RoundOfAck(char* buf, size_t n) {
  // Check to make sure the size of the buffer is correct.
  if (n != sizeof(msg::Ack)) {
    return {};
  }

  msg::Ack* ack = reinterpret_cast<msg::Ack*>(buf);
  return ntohl(ack->round);
}

void SendMessage(udp::ClientPtr client, const msg::Message& msg) {
  size_t size =
      sizeof(msg::ByzantineMessage) + sizeof(uint32_t) * msg.ids.size();
  char buf[size];
  bzero(buf, size);

  // Copy the message part.
  msg::ByzantineMessage* c_msg = reinterpret_cast<msg::ByzantineMessage*>(buf);
  c_msg->type = htonl(kByzantineMessageType);
  c_msg->size = htonl(size);
  c_msg->round = htonl(msg.round);
  c_msg->order = htonl(static_cast<int>(msg.order));

  // C++ does not support flexible arrays, so we need to be a little tricky
  // here. We already made sure the buffer was the correct size by adding space
  // for each of the ids at the end of ByzantineMessage. Now we populate the
  // array.
  uint32_t* id_buf = reinterpret_cast<uint32_t*>(buf + sizeof(*c_msg));
  for (size_t i = 0; i < msg.ids.size(); ++i) {
    id_buf[i] = htonl(msg.ids[i]);
  }

  // Passed to SendWithAck to verify that any acknowledgement we hear is valid.
  auto isValidAck = [msg](udp::ClientPtr _, char* buf, size_t n) {
    auto ackRound = RoundOfAck(buf, n);
    bool valid = ackRound && *ackRound == msg.round;
    if (!valid) return udp::ServerAction::Continue;
    return udp::ServerAction::Stop;
  };

  client->SendWithAck(buf, size, kSendAttempts, isValidAck);
}

void SendAckForRound(udp::ClientPtr client, unsigned int round) {
  msg::Ack ack = {};
  ack.type = htonl(kAckType);
  ack.size = htonl(sizeof(ack));
  ack.round = htonl(round);

  char* buf = reinterpret_cast<char*>(&ack);
  client->Send(buf, sizeof(ack));
}

UdpClientMap ClientsForProcessList(const ProcessList& processes) {
  UdpClientMap clients(processes.size());
  for (auto const& addr : processes) {
    clients.emplace(addr, std::make_shared<udp::Client>(addr, kAckTimeout));
  }
  return clients;
}

MaliciousBehavior StringToMaliciousBehavior(std::string str) {
  if (str == "silent") return MaliciousBehavior::SILENT;
  if (str == "delay_send") return MaliciousBehavior::DELAY_SEND;
  if (str == "partial_send") return MaliciousBehavior::PARTIAL_SEND;
  if (str == "wrong_order") return MaliciousBehavior::WRONG_ORDER;
  throw std::invalid_argument(
      "malicious behavior can one of {\"silent\", \"delay_send\", "
      "\"partial_send\", \"wrong_order\"}");
}

std::string MaliciousBehaviorString(MaliciousBehavior m) {
  switch (m) {
    case MaliciousBehavior::SILENT:
      return "silent";
    case MaliciousBehavior::DELAY_SEND:
      return "delay_send";
    case MaliciousBehavior::PARTIAL_SEND:
      return "partial_send";
    case MaliciousBehavior::WRONG_ORDER:
      return "wrong_order";
    default:
      throw std::invalid_argument("unexpected MaliciousBehavior value");
  }
}

bool General::ShouldSendMsg() {
  if (ExhibitsBehavior(MaliciousBehavior::SILENT)) {
    return false;
  }
  if (ExhibitsBehavior(MaliciousBehavior::PARTIAL_SEND)) {
    // Send message 75% of the time.
    static thread_local std::default_random_engine random_engine(
        std::chrono::system_clock::now().time_since_epoch().count());

    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    return distribution(random_engine) < 0.75;
  }
  return true;
}

void General::MaybeDelaySend() {
  if (!ExhibitsBehavior(MaliciousBehavior::DELAY_SEND)) {
    return;
  }

  // Here and above, static thread local to avoid expensive initialization cost
  // on every call, while maintaining thread safety.
  static thread_local std::default_random_engine random_engine(
      std::chrono::system_clock::now().time_since_epoch().count());

  // Delay for a random duration based on a selection from a poisson
  // distribution centered at half the round timeout, at intervals of 1/10th a
  // second.
  typedef std::chrono::duration<int, std::deci> deciseconds;
  auto timeout_deci = std::chrono::duration_cast<deciseconds>(kRoundTimeout);
  std::poisson_distribution<int> poisson(timeout_deci.count() / 2);
  int delay = poisson(random_engine);
  if (delay <= 0) {
    return;
  }
  std::this_thread::sleep_for(deciseconds{delay});
  return;
}

msg::Order Commander::Decide() {
  // Send in parallel so that some Lieutenants don't end up far ahead of
  // others.
  ThreadGroup senders;
  auto ids = std::vector<unsigned int>{0};
  for (unsigned int pid = 1; pid < processes_.size(); ++pid) {
    if (ShouldSendMsg()) {
      msg::Message msg{round_, OrderForMsg(), ids};
      logging::out << "Sending  " << msg << " to p" << pid << "\n";

      udp::ClientPtr client = ClientForId(pid);
      senders.AddThread([this, client, msg] {
        MaybeDelaySend();
        SendMessage(client, msg);
      });
    }
  }
  senders.JoinAll();
  return order_;
}

msg::Order Commander::OrderForMsg() const {
  if (ExhibitsBehavior(MaliciousBehavior::WRONG_ORDER)) {
    // Send wrong order 30% of the time.
    static thread_local std::default_random_engine random_engine(
        std::chrono::system_clock::now().time_since_epoch().count());

    std::uniform_real_distribution<double> distribution(0.0, 1.0);
    if (distribution(random_engine) < 0.30) {
      return order_ == msg::Order::ATTACK ? msg::Order::RETREAT
                                          : msg::Order::ATTACK;
    }
  }
  return order_;
}

msg::Order Lieutenant::Decide() {
  server_.Listen(
      // Called on all incoming Byzantine Messages.
      [this](udp::ClientPtr client, char* buf, size_t n) {
        auto from = client->RemoteAddress();
        auto msg = ByzantineMsgFromBuf(buf, n);
        if (!msg || !ValidMessage(*msg, from)) {
          // If the message was not valid, return without trying to use it.
          return ContinueUnlessTimeout();
        }

        logging::out << "Received " << *msg << " from p" << msg->ids.back()
                     << "\n";
        SendAckForRound(client, round_);

        bool newRound = false;
        if (FirstRound()) {
          // Only handle the first real order.
          if (msg->order != msg::Order::NO_ORDER && orders_seen_.size() == 0) {
            orders_seen_.insert(msg->order);
            msgs_this_round_.insert(*msg);
            newRound = true;
          }
        } else {
          // Handle if not a replay of a previous message (msg with same ids).
          if (ids_this_round_.count(msg->ids) == 0) {
            ids_this_round_.insert(msg->ids);

            // Handle the order in the message based on if we've seen the same
            // order or not.
            if (msg->order != msg::Order::NO_ORDER &&
                orders_seen_.count(msg->order) == 0) {
              // We have not seen this order yet, so we add it to the
              // orders_seen set and forward it in the next round.
              orders_seen_.insert(msg->order);
            } else {
              // We have already seen this order, so we forward a no_order
              // instead next round.
              msg->order = msg::Order::NO_ORDER;
            }

            // Record the message so we can forward it next round.
            msgs_this_round_.insert(*msg);

            // Determine if this is the last message needed for the round.
            newRound = RoundComplete();
          }
        }

        if (newRound) {
          return MoveToNewRoundOrStop();
        }
        return ContinueUnlessTimeout();
      },
      // Called on socket timeout.
      [this]() { return HandleRoundTimeout(); });

  return DecideOrder();
}

inline msg::Order Lieutenant::DecideOrder() const {
  if (orders_seen_.size() == 1 && orders_seen_.count(msg::Order::ATTACK) == 1) {
    return msg::Order::ATTACK;
  }
  return msg::Order::RETREAT;
}

inline bool Lieutenant::RoundComplete() const {
  return ids_this_round_.size() == MessagesForRound(processes_.size(), round_);
}

udp::ServerAction Lieutenant::ContinueUnlessTimeout() {
  // Compute the duration between the start of the round and now.
  const auto now = std::chrono::steady_clock::now();
  const auto round_dur = std::chrono::duration_cast<std::chrono::microseconds>(
      now - round_start_ts_);

  // If this duration is more than the round timeout, handle the timeout.
  if (round_dur > kRoundTimeout) {
    HandleRoundTimeout();
  }
  return udp::ServerAction::Continue;
}

udp::ServerAction Lieutenant::HandleRoundTimeout() {
  if (FirstRound()) {
    // We can't timeout in the first round. Just continue to wait.
    return udp::ServerAction::Continue;
  }

  logging::out << "Timeout in round " << round_ << "\n";
  return MoveToNewRoundOrStop();
}

udp::ServerAction Lieutenant::MoveToNewRoundOrStop() {
  if (LastRound()) {
    ClearSenders();
    return udp::ServerAction::Stop;
  }
  InitNewRound();
  return udp::ServerAction::Continue;
}

void Lieutenant::ClearSenders() {
  sender_threads_this_round_.JoinAll();
  sender_threads_this_round_.Clear();
}

void Lieutenant::InitNewRound() {
  ClearSenders();
  IncrementRound();

  // Determine the set of messages to forward in the next round.
  std::unordered_map<unsigned int, std::vector<msg::Message>> toSend;
  for (msg::Message msg : msgs_this_round_) {
    if (msg.round != round_ - 1) {
      throw std::logic_error(
          "message in msgs_this_round_ not from current round");
    }

    // Update the messages round number to the current round.
    msg.round = round_;

    // Add this process in at the end of the message id list.
    msg.ids.push_back(id_);

    // Determine which processes we need to send this message to.
    for (unsigned int pid = 0; pid < processes_.size(); ++pid) {
      // Only send to processes not already in this message.
      bool inMsg = false;
      for (auto const& id : msg.ids) {
        if (id == pid) {
          inMsg = true;
          break;
        }
      }
      if (!inMsg) {
        if (ShouldSendMsg()) {
          logging::out << "Sending  " << msg << " to p" << pid << "\n";
          toSend[pid].push_back(msg);
        }
      }
    }
  }

  // For each process that we have messages to send to...
  for (auto const& batch : toSend) {
    sender_threads_this_round_.AddThread([this, batch] {
      // Send each message to the process serially in a new thread.
      unsigned int pid = batch.first;
      udp::ClientPtr client = ClientForId(pid);
      for (auto const& msg : batch.second) {
        MaybeDelaySend();
        SendMessage(client, msg);
      }
    });
  }

  // Clear round-specific containers and reset round start timestamp.
  ids_this_round_.clear();
  msgs_this_round_.clear();
  round_start_ts_ = std::chrono::steady_clock::now();
}

bool Lieutenant::ValidMessage(const msg::Message& msg,
                              const net::Address& from) const {
  // Invalid if the message is from a later round.
  if (msg.round > round_) {
    return false;
  }
  // Invalid if the message has an incorrect number of ids.
  if (msg.round + 1 != msg.ids.size()) {
    return false;
  }
  // Invalid if the first message is not from the General (pid 0);
  if (msg.ids.at(0) != 0) {
    return false;
  }
  // Invalid if not all ids are unique.
  std::set<unsigned int> idset;
  for (auto const& id : msg.ids) {
    // Invalid if any id is out of bounds.
    if (id >= processes_.size()) {
      return false;
    }
    // Invalid if any id is our id.
    if (id == id_) {
      return false;
    }
    idset.insert(id);
  }
  if (idset.size() < msg.ids.size()) {
    return false;
  }
  // Invalid if the last id does not match the sender. This check will not
  // be complete for processes on the same host, because we can not know the
  // sending port of the process, only its receiving port.
  if (processes_.at(msg.ids.back()).hostname() != from.hostname()) {
    return false;
  }
  return true;
}

}  // namespace generals
