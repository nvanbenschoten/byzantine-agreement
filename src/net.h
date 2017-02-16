#ifndef NET_H_
#define NET_H_

#include <unistd.h>

#include <experimental/optional>
#include <functional>
#include <iostream>
#include <string>

// HOST_NAME_MAX is recommended by POSIX, but not required.
// FreeBSD and OSX (as of 10.9) are known to not define it.
// 255 is generally the safe value to assume.
#ifndef HOST_NAME_MAX
#define HOST_NAME_MAX 255
#endif

namespace net {

// Retrieves the current computer's hostname.
std::string GetHostname();

class Address {
 public:
  Address(std::string hostname, unsigned short port)
      : hostname_(hostname), port_(port){};

  inline const std::string& hostname() const { return hostname_; };
  inline unsigned short port() const { return port_; };

  bool operator==(const Address& other) const {
    return hostname_ == other.hostname_ && port_ == other.port_;
  }
  bool operator!=(const Address& other) const { return !(*this == other); }

  friend std::ostream& operator<<(std::ostream& os, const Address& addr);

 private:
  std::string hostname_;
  unsigned short port_;
};

Address AddressWithDefaultPort(
    std::string addr, std::experimental::optional<unsigned short> default_port);

}  // namespace net

// custom specialization of std::hash can be injected in namespace std
namespace std {
template <>
struct hash<net::Address> {
  std::size_t operator()(const net::Address& k) const {
    return (std::hash<std::string>()(k.hostname()) ^
            (std::hash<unsigned short>()(k.port()) << 1));
  }
};
}  // namespace std

#endif
