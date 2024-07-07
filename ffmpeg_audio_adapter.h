///
/// @file
/// @brief Contains implementation of FFMPEG based Audio Adapter
/// @copyright Copyright (c) 2020, MIT License
///
#ifndef SPLEETER_AUDIO_FFMPEG_AUDIO_ADAPTER_H
#define SPLEETER_AUDIO_FFMPEG_AUDIO_ADAPTER_H

#include "common.h"
#include "waveform.h"
#include <atomic>
#include <functional>
#include <memory>

namespace spleeter {
class FfmpegAudioAdapter final {
private:
  std::atomic_bool cancel_token_{false};

public:
  int Decode(const std::string &path, const std::int64_t start,
             const std::int64_t duration, std::unique_ptr<Waveform> &result,
             ProgressCallback progress_callback);

  int Encode(Waveform waveform, std::string filename,
             ProgressCallback progress_callback);

  void Cancel();
};
} // namespace spleeter

#endif /// SPLEETER_AUDIO_FFMPEG_AUDIO_ADAPTER_H
