#include <exception>
#include <experimental/optional>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "args.h"
#include "general.h"
#include "log.h"
#include "net.h"

typedef args::ValueFlag<int> IntFlag;
typedef args::ValueFlag<std::string> StringFlag;

const std::string program_desc =
    "An implementation of the Byzantine Agreement Algorithm.";
const std::string help_desc = "Display this help menu.";
const std::string port_desc =
    "The port identifies on which port the process will be listening on for "
    "incoming messages. It can take any integer from 1024 to 65535.";
const std::string hostfile_desc =
    "The hostfile is the path to a file that contains the list of hostnames "
    "that the processes are running on. It assumes that each host is running "
    "only one instance of the process. It should be in the following format.\n"
    "`\n"
    "xinu01.cs.purdue.edu\n"
    "xinu02.cs.purdue.edu\n"
    "...\n"
    "`\n"
    "All the processes will listen on the same port. "
    "The line number indicates the identifier of the process.";
const std::string faulty_desc =
    "The \"faulty\" specifies the total number of Byzantine processes in the "
    "system. The value of faulty is non-negative. It also indicates after "
    "which round a process should terminate. Whenever a process finishes the "
    "(faulty + 1)th round or reaches a round greater than the (faulty + 1)th "
    "round, the process can safely decide and terminate. Note that the total "
    "number of processes must be no less than (faulty + 2).";
const std::string cmdr_id_desc = "The identifier of the commander. 0-indexed.";
const std::string order_desc =
    "The order can be either \"attack\" or \"retreat\". If specified, the "
    "process will be the Commander and will send the specified order. "
    "Otherwise, the process will be a lieutenant.";
const std::string id_desc =
    "The optional id specifier of this process. Only needed if multiple "
    "processes in the hostfile are running on the same host. 0-indexed.";
const std::string verbose_desc = "Sets the logging level to verbose.";

// gets the processed.
ProcessList GetProcesses(
    const std::string hostfile,
    std::experimental::optional<unsigned short> default_port) {
  ProcessList processes;
  std::ifstream file(hostfile);
  if (!file) {
    throw std::runtime_error("could not open hostfile");
  }

  std::string host;
  while (file >> host) {
    try {
      auto addr = net::AddressWithDefaultPort(host, default_port);
      processes.push_back(addr);
    } catch (std::invalid_argument e) {
      throw args::UsageError(e.what());
    }
  }
  return processes;
}

// checks if the --id flag is within the process list and pointing to
// our hostname.
void CheckProcessId(const ProcessList& processes, int my_id) {
  // check if the id is within bounds.
  if (my_id < 0 || (uint)my_id >= processes.size()) {
    throw args::ValidationError("--id value not found in hostfile");
  }

  // check if the process is on this host.
  if (processes[my_id].hostname() != net::GetHostname()) {
    throw args::ValidationError("--id value is not the hostname of this host");
  }
}

// gets the current process ID.
int GetProcessId(const ProcessList& processes) {
  int found = -1;
  auto hostname = net::GetHostname();
  for (std::size_t i = 0; i < processes.size(); ++i) {
    if (processes[i].hostname() == hostname) {
      if (found >= 0) {
        // multiple processes are set to use our host.
        throw args::UsageError(
            "when running multiple processes on the same host, use the --id "
            "flag");
      }
      found = i;
    }
  }
  if (found == -1) {
    // our process is not in the file, throw an error
    throw args::ValidationError("current hostname not found in hostfile");
  }
  return found;
}

// validate the commander_id flag.
void ValidateCommanderId(ProcessList& processes, int commander_id) {
  // make sure the commander_id is valid
  if (commander_id < 0 || (size_t)commander_id >= processes.size()) {
    throw args::ValidationError("commander_id does not reference a process");
  }
  // move the commander to the first element in the vector
  std::iter_swap(processes.begin(), processes.begin() + commander_id);
}

// validate the fault flag.
void ValidateFaultyCount(const ProcessList& processes, int faulty) {
  if (faulty < 0) {
    throw args::ValidationError("faulty count must be non-negative");
  }
  if ((size_t)faulty + 2 > processes.size()) {
    throw args::ValidationError(
        "process count must be no less than (faulty + 2)");
  }
}

// validate the order flag.
std::experimental::optional<msg::Order> ValidateOrder(StringFlag& order,
                                                      bool is_commander) {
  if (is_commander) {
    if (!order) {
      throw args::UsageError("the commander must specify an order");
    }

    try {
      auto order_val = args::get(order);
      return msg::StringToOrder(order_val);
    } catch (std::invalid_argument e) {
      throw args::ValidationError(e.what());
    }
  } else {
    if (order) {
      throw args::ValidationError(
          "only the commander process can specify an order");
    }
    return {};
  }
}

int main(int argc, const char** argv) {
  args::ArgumentParser parser(program_desc);
  args::HelpFlag help(parser, "help", help_desc, {"help"});
  IntFlag port(parser, "port", port_desc, {'p', "port"});
  StringFlag hostfile(parser, "hostfile", hostfile_desc, {'h', "hostfile"});
  IntFlag faulty(parser, "faulty", faulty_desc, {'f', "faulty"});
  IntFlag cmdr_id(parser, "commander_id", cmdr_id_desc, {'C', "commander_id"});
  StringFlag order(parser, "order", order_desc, {'o', "order"});
  IntFlag id(parser, "id", id_desc, {'i', "id"});
  args::Flag verbose(parser, "verbose", verbose_desc, {'v', "verbose"});

  try {
    parser.ParseCLI(argc, argv);

    // Set up logging.
    logging::out.enable(verbose);

    // Check required fields.
    if (!hostfile) throw args::UsageError("--hostfile is a required flag");
    if (!faulty) throw args::UsageError("--faulty is a required flag");
    if (!cmdr_id) throw args::UsageError("--commander_id is a required flag");
    auto hostfile_val = args::get(hostfile);
    auto faulty_val = args::get(faulty);
    auto commander_id_val = args::get(cmdr_id);

    // Get the default process port, if one is supplied.
    std::experimental::optional<unsigned short> default_port;
    if (port) {
      default_port = args::get(port);
    }

    // create the process list from the hostfile.
    auto processes = GetProcesses(hostfile_val, default_port);

    int my_id;
    if (id) {
      my_id = args::get(id);
      CheckProcessId(processes, my_id);
    } else {
      my_id = GetProcessId(processes);
    }
    auto server_port = processes[my_id].port();

    ValidateCommanderId(processes, commander_id_val);
    ValidateFaultyCount(processes, faulty_val);

    bool is_commander = my_id == commander_id_val;
    auto order_val = ValidateOrder(order, is_commander);

    std::unique_ptr<General> general;
    if (is_commander) {
      general = std::make_unique<Commander>(processes, faulty_val, *order_val);
    } else {
      general = std::make_unique<Lieutenant>(processes, my_id, server_port,
                                             faulty_val);
    }
    msg::Order decision = general->Decide();

    std::cout << my_id << ": Agreed on " << msg::OrderString(decision)
              << std::endl;
  } catch (args::Help) {
    std::cout << parser;
    return 0;
  } catch (args::UsageError e) {
    std::cerr << "\n  \033[1;31m" << e.what() << "\033[0m\n\n";
    std::cerr << parser;
    return 1;
  } catch (std::runtime_error e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
