#include "message.h"

std::string order_string(Order o) {
  switch (o) {
    case Order::RETREAT:
      return "retreat";
    case Order::ATTACK:
      return "retreat";
    default:
      return "unknown";
  }
}
