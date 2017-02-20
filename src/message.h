#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <exception>
#include <iostream>
#include <string>
#include <vector>

const uint32_t kByzantineMessageType = 1;
const uint32_t kAckType = 2;

namespace msg {

// ByzantineMessage is the wire format of a standard message used in the
// Byzantine Agreement Algorithm. It is used as a convenience for encoding
// and decoding bytes to and from sockets, but is quickly transformed into
// a Message.
typedef struct {
  uint32_t type;   // Must be equal to 1
  uint32_t size;   // size of message in bytes
  uint32_t round;  // round number
  uint32_t order;  // the order (retreat = 0, attack = 1, no order = 2)
  uint32_t ids[];  // id’s of the senders of this message
} ByzantineMessage;

// Ack is the wire format of an acknowledgement message used to provided
// reliable communication.
typedef struct {
  uint32_t type;   // Must be equal to 2
  uint32_t size;   // size of message in bytes
  uint32_t round;  // round number
} Ack;

// Order is the type of order that the Generals are attempting to come to
// a consensus on in the Byzantine Agreement Algorithm. RETREAT and ATTACK
// are the two options, while NO_ORDER is used in empty messages where no Order
// is needed (per the paper's algorithm: "a message reporting that he will not
// send such a message").
enum class Order {
  RETREAT,
  ATTACK,
  NO_ORDER,
};

// Maps a string to an Order, throwing an exception if the string is invalid.
Order StringToOrder(std::string str);
// Returns the string representation of the provided Order.
std::string OrderString(Order o);

// Message is a convenient representation of a Byzantine message. It should be
// favored over ByzantineMessage for all uses except encoding and decoding.
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
