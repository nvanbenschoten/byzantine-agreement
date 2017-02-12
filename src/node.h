#ifndef NODE_H_
#define NODE_H_

#include "udp_conn.h"

class Commander {
public:
    Commander(std::vector<std::string> hosts, int port, int faulty, std::string order) {}
};

class Lieutenant {
public:
    Lieutenant(std::vector<std::string> hosts, int port, int faulty) {}
};

#endif
