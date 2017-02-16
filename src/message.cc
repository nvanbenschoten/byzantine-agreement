#include "message.h"

namespace msg {

Order StringToOrder(std::string str) {
  if (str == "retreat") return Order::RETREAT;
  if (str == "attack") return Order::ATTACK;
  throw std::invalid_argument("order can either be \"attack\" or \"retreat\"");
}

std::string OrderString(Order o) {
  switch (o) {
    case Order::RETREAT:
      return "retreat";
    case Order::ATTACK:
      return "attack";
    default:
      throw std::invalid_argument("unexpected Order value");
  }
}

bool operator<(const Message& lhs, const Message& rhs) {
  if (lhs.round != rhs.round) {
    return lhs.round < rhs.round;
  }
  if (lhs.ids != rhs.ids) {
    return lhs.ids < rhs.ids;
  }
  return lhs.order < rhs.order;
}

std::ostream& operator<<(std::ostream& o, const Message& m) {
  o << "{id: " << m.round << ", order: " << OrderString(m.order) << ", ids: <";
  for (int i = 0; i < m.ids.size(); ++i) {
    if (i > 0) o << ' ';
    o << m.ids[i];
  }
  o << ">}";
  return o;
}

}  // namespace msg
