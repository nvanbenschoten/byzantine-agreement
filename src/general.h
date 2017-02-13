#ifndef GENERAL_H_
#define GENERAL_H_

#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "message.h"
#include "udp_conn.h"

class General {
 public:
  General(std::vector<std::string> hosts, int port, int faulty)
      : hosts_(hosts),
        port_(port),
        faulty_(faulty)  //, server_(port)
  {
    for (auto const& host : hosts_) {
      clients_.emplace(host, std::make_unique<UdpClient>(host, port));
    }
  }

  virtual ~General() = default;

  virtual Order decide() = 0;

  void send_message(UdpClient client, Order order);
  void send_ack(UdpClient client);

 protected:
  std::vector<std::string> hosts_;
  int port_;
  int faulty_;

  std::unordered_map<std::string, std::unique_ptr<UdpClient>> clients_;
  // UdpServer server_;

  // Todo
  int round_;
};

class Commander : public General {
 public:
  Commander(std::vector<std::string> hosts, int port, int faulty, Order order)
      : General(hosts, port, faulty) /*, order_(order)*/ {}

  Order decide() {
    char* p = (char*)"Hello";
    clients_[hosts_[0]]->send(p, 5);
    return Order::RETREAT;
  };

 private:
  // Order order_;
};

class Lieutenant : public General {
 public:
  Lieutenant(std::vector<std::string> hosts, int port, int faulty)
      : General(hosts, port, faulty), server_(port) {}

  Order decide() {
    int a = 5;
    server_.listen([this, &a](UdpClient client, char* buf) {
      send_ack(client);

      std::cout << client.remote_hostname() << "\n";
      printf("server received bytes: %s\n", buf);
      a--;
    });
    while (a > 0) {
    };
    return Order::RETREAT;
  };

 private:
  UdpServer server_;

  std::set<Order> orders_seen_;
};

#endif
