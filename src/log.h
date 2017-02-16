#ifndef LOG_H_
#define LOG_H_

#include <atomic>
#include <iostream>

namespace logging {

class Logger {
 public:
  Logger() : enabled_(false){};

  inline void enable(bool enable) { enabled_ = enable; };

  template <typename T>
  const Logger& operator<<(const T& v) const {
    if (enabled_) {
      std::cerr << v;
    }
    return *this;
  }

 private:
  std::atomic<bool> enabled_;
};

// Global
extern Logger out;

}  // namespace logging

#endif
