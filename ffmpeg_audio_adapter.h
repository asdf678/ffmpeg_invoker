///
/// @file
/// @brief Contains implementation of FFMPEG based Audio Adapter
/// @copyright Copyright (c) 2020, MIT License
///
#ifndef SPLEETER_AUDIO_FFMPEG_AUDIO_ADAPTER_H
#define SPLEETER_AUDIO_FFMPEG_AUDIO_ADAPTER_H

#include "waveform.h"
#include <memory>
namespace spleeter {
/// @brief An AudioAdapter implementation that use FFMPEG libraries to perform
/// I/O operation for audio processing.

enum EncodeFormat { mp3, aac, flac, wav };

class FfmpegAudioAdapter final {
public:
  std::unique_ptr<Waveform> Load(const std::string &path,
                                 const std::int64_t start,
                                 const std::int64_t duration);

  int Save(Waveform waveform, std::string filename);
};
} // namespace spleeter

#endif /// SPLEETER_AUDIO_FFMPEG_AUDIO_ADAPTER_H
