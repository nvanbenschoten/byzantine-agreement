#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <string>

#define BYZANTINE_MESSAGE_TYPE 1
#define ACK_TYPE 2

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
  UNKNOWN,
};

std::string order_string(Order o);

#endif
