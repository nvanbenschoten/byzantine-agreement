#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <exception>
#include <iostream>
#include <string>
#include <vector>

const uint32_t kByzantineMessageType = 1;
const uint32_t kAckType = 2;

namespace msg {

typedef struct {
  uint32_t type;   // Must be equal to 1
  uint32_t size;   // size of message in bytes
  uint32_t round;  // round number
  uint32_t order;  // the order (retreat = 0 and attack = 1)
  uint32_t ids[];  // id’s of the senders of this message
} ByzantineMessage;

typedef struct {
  uint32_t type;   // Must be equal to 2
  uint32_t size;   // size of message in bytes
  uint32_t round;  // round number
} Ack;

enum class Order {
  RETREAT,
  ATTACK,
};

// Maps a string to an Order, throwing an exception if the string is invalid.
Order StringToOrder(std::string str);
// Returns the string representation of the provided Order.
std::string OrderString(Order o);

struct Message {
  unsigned int round;
  Order order;
  std::vector<unsigned int> ids;
};

// Needed so that Message can be added to std::set.
bool operator<(const Message& lhs, const Message& rhs);

// Allow streaming of Message on ostreams.
std::ostream& operator<<(std::ostream& o, const Message& m);

}  // namespace msg

#endif
