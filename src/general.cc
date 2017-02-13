#include "general.h"

void General::send_message(UdpClient client, Order order) {
  ByzantineMessage msg = {};
  msg.type  = htonl(BYZANTINE_MESSAGE_TYPE);
  msg.size  = htonl(sizeof(msg));
  msg.round = htonl(round_);
  msg.order = htonl(static_cast<int>(order));

  char* buf = reinterpret_cast<char*>(&msg);
  client.send(buf, sizeof(msg));
}

void General::send_ack(UdpClient client) {
  Ack ack   = {};
  ack.type  = htonl(ACK_TYPE);
  ack.size  = htonl(sizeof(ack));
  ack.round = htonl(round_);

  char* buf = reinterpret_cast<char*>(&ack);
  client.send(buf, sizeof(ack));
}
