#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "argparse.h"
#include "general.h"

// gets the process hosts.
std::vector<std::string> get_hosts(const std::string hostfile) {
  std::vector<std::string> hosts;
  std::ifstream file(hostfile);
  if (!file) {
    throw std::runtime_error("could not open hostfile");
  }

  std::string host;
  while (file >> host) {
    hosts.push_back(host);
  }
  return hosts;
}

// gets the current process ID.
int get_process_id(const std::vector<std::string>& hosts) {
  auto hostname = get_hostname();
  for (std::size_t i = 0; i < hosts.size(); ++i) {
    if (hosts[i] == hostname) {
       // our process is in the file, return its index.
      return i;
    }
  }
  // our process is not in the file, throw an error
  std::cout << "hostname " << hostname << " not found in hostfile" << std::endl;
  exit(-5);
}

// validate the commander_id flag.
void validate_commander_id(std::vector<std::string>& hosts, int commander_id) {
  // make sure the commander_id is valid
  if (commander_id < 0 || (size_t) commander_id >= hosts.size()) {
    std::cout << "commander_id " << commander_id
              << " does not reference a process" << std::endl;
    exit(-5);
  }
  // move the commander to the first element in the vector
  std::iter_swap(hosts.begin(), hosts.begin() + commander_id);
}

// validate the fault flag.
void validate_faulty_count(const std::vector<std::string>& hosts, int faulty) {
  if (faulty < 0) {
    std::cout << "faulty count must be non-negative" << std::endl;
    exit(-5);
  }
  if ((size_t) faulty + 2 > hosts.size()) {
    std::cout << "process count must be no less than (faulty + 2)" << std::endl;
    exit(-5);
  }
}

// validate the order flag.
Order validate_order(const std::string& order, bool is_commander) {
  // if (is_commander) {
  //   if (order == "attack")  return Order::ATTACK;
  //   if (order == "retreat") return Order::ATTACK;

  //   if (order == "") {
  //     std::cout << "the commander must specify an order" << std::endl;
  //   } else {
  //     std::cout << "the order can either be \"attack\" or \"retreat\"" << std::endl;
  //   }
  //   exit(-5);
  // } else {
  //   if (order != "") {
  //     std::cout << "only the commander process can specify an order" << std::endl;
  //     exit(-5);
  //   }
    return Order::UNKNOWN;
  // }
}

// determine if the current process is the commander.
bool check_commander(const std::vector<std::string>& hosts) {
  return hosts[0] == get_hostname();
}

int main(int argc, const char** argv) {
  ArgumentParser parser;
  parser.addArgument("-p", "--port", 1, false);
  parser.addArgument("-h", "--hostfile", 1, false);
  parser.addArgument("-f", "--faulty", 1, false);
  parser.addArgument("-C", "--commander_id", 1, false);
  parser.addArgument("-o", "--order", 1);
  parser.parse(argc, argv);

  auto port = std::stoi(parser.retrieve<std::string>("port"));
  auto hostfile = parser.retrieve<std::string>("hostfile");
  auto faulty = std::stoi(parser.retrieve<std::string>("faulty"));
  auto commander_id = std::stoi(parser.retrieve<std::string>("commander_id"));
  auto order_str = parser.retrieve<std::string>("order");

  auto hosts = get_hosts(hostfile);
  auto id = get_process_id(hosts);

  validate_commander_id(hosts, commander_id);
  validate_faulty_count(hosts, faulty);

  // bool is_commander = check_commander(hosts);
  bool is_commander = order_str == "attack";
  auto order = validate_order(order_str, is_commander);

  std::unique_ptr<General> general;
  if (is_commander) {
    general = std::make_unique<Commander>(hosts, port, faulty, order);
  } else {
    general = std::make_unique<Lieutenant>(hosts, port, faulty);
  }
  Order decision = general->decide();

  std::cout << id << ": Agreed on " << order_string(decision) << std::endl;
}
