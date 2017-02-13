#include <errno.h>
#include <exception>
#include <sstream>
#include <string>

class AbstractNetworkException : public std::exception {
 public:
  virtual const char* what() const throw() { return stream_.str().c_str(); }

 protected:
  std::ostringstream stream_;
};

class SocketException : public AbstractNetworkException {
 public:
  SocketException() { stream_ << "Could not create UDP Socket"; }
};

class HostNotFoundException : public AbstractNetworkException {
 public:
  HostNotFoundException(std::string host) {
    stream_ << "Could not find host " << host;
  };
};

class BindException : public AbstractNetworkException {
 public:
  BindException() { stream_ << "Could not bind UDP Socket"; }
};

class SendException : public AbstractNetworkException {
 public:
  SendException() { stream_ << "Could not send data on socket: " << errno; }
};

class ReceiveException : public AbstractNetworkException {
 public:
  ReceiveException() {
    stream_ << "Could not receive data on socket: " << errno;
  }
};
