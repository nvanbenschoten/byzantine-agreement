#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "argparse.h"
#include "node.h"

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
void validate_order(const std::string& order, bool is_commander) {
    if (is_commander) {
        if (order == "") {
            std::cout << "the commander must specify an order" << std::endl;
            exit(-5);
        }
        if (order != "attack" && order != "retreat") {
            std::cout << "the order can either be \"attack\" or \"retreat\"" << std::endl;
            exit(-5);
        }
    } else {
        if (order != "") {
            std::cout << "only the commander process can specify an order" << std::endl;
            exit(-5);
        }
    }
}

// determine if the current process is the commander.
bool check_commander(const std::vector<std::string>& hosts) {
    auto hostname = get_hostname();
    for (auto const& host : hosts) {
        if (host == hostname) {
            // our process is in the file, return if it is the commander
            return hosts[0] == hostname;
        }
    }
    // our process is not in the file, throw an error
    std::cout << "hostname " << hostname << " not found in hostfile" << std::endl;
    exit(-5);
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
    auto order = parser.retrieve<std::string>("order");

    auto hosts = get_hosts(hostfile);
    validate_commander_id(hosts, commander_id);
    validate_faulty_count(hosts, faulty);

    bool is_commander = check_commander(hosts);
    validate_order(order, is_commander);

    if (is_commander) {
        Commander commander(hosts, port, faulty, order);
    } else {
        Lieutenant lieutenant(hosts, port, faulty);
    }
}
