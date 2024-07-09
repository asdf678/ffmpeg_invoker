#ifndef SPLEETER_COMMON_H
#define SPLEETER_COMMON_H
#include <atomic>
#include <chrono>
#include <exception>
#include <string>
#include <functional>
namespace spleeter {

using ProgressCallback = std::function<void(int64_t)>;

class CancelException : public std::exception {
private:
  std::string message;

public:
  CancelException(const std::string msg = "") : message(msg) {}

  const char *what() const noexcept override { return message.c_str(); }

  static void check_cancel_and_throw(std::atomic_bool &cancel_token) {
    if (cancel_token.load()) {
      throw CancelException();
    }
  }
};

inline std::chrono::milliseconds get_current_timestamp() {
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
  return timestamp.time_since_epoch();
}

} // namespace spleeter
#endif