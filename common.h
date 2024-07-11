#ifndef SPLEETER_COMMON_H
#define SPLEETER_COMMON_H

#include "waveform.h"
#include <atomic>
#include <chrono>
#include <deque>
#include <exception>
#include <functional>
#include <queue>
#include <string>
#include <vector>

namespace spleeter {

namespace constants {
static constexpr int kSampleRate = 44100;
static constexpr int kChannelNum = 2;
} // namespace constants

using ProgressCallback = std::function<void(int64_t)>;
class CancelToken {
  std::atomic_bool cancel_token_{false};

public:
  void cancel() { cancel_token_.store(true); }
  bool is_cancelled() { return cancel_token_; }
};

class CancelException : public std::exception {
private:
  std::string message;

public:
  CancelException(const std::string msg = "") : message(msg) {}

  const char *what() const noexcept override { return message.c_str(); }

  static void check_cancel_and_throw(CancelToken &cancel_token) {
    if (cancel_token.is_cancelled()) {
      throw CancelException();
    }
  }
};

std::chrono::milliseconds get_current_timestamp();

std::queue<spleeter::Waveform> segment_audio(const Waveform &,
                                             std::size_t segment_nb_samples,
                                             std::size_t boundary_nb_samples);

spleeter::Waveform restore_segment_audio(const Waveform &waveform,
                                         std::size_t boundary_nb_samples,
                                         bool head, bool tail);

} // namespace spleeter

#endif