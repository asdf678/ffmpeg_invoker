///
/// @file
/// @brief Contains implementation of FFMPEG based Audio Adapter
/// @copyright Copyright (c) 2020, MIT License
///
#ifndef SPLEETER_FFMPEG_AUDIO_CODEC_H
#define SPLEETER_FFMPEG_AUDIO_CODEC_H

#include "common.h"
#include "waveform.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

namespace spleeter {
namespace codec {
class FFmpegAudioEncoder;

class FFmpegAudioDecoder;
} // namespace codec

class AudioDecoder {
private:
  std::unique_ptr<codec::FFmpegAudioDecoder> decoder_;

public:
  AudioDecoder(const AudioDecoder &) = delete;

  AudioDecoder &operator=(const AudioDecoder &) = delete;

  AudioDecoder(AudioDecoder &&);

  AudioDecoder &operator=(AudioDecoder &&);

  AudioDecoder(std::string path, CancelToken *cancel_token);

  int Decode(std::unique_ptr<Waveform> &result, std::size_t max_frame_size);

  operator bool() { return static_cast<bool>(decoder_); }

  ~AudioDecoder();
};

class AudioEncoder {
private:
  std::unique_ptr<codec::FFmpegAudioEncoder> encoder_;

public:
  AudioEncoder(const AudioEncoder &) = delete;

  AudioEncoder &operator=(const AudioEncoder &) = delete;

  AudioEncoder(AudioEncoder &&);

  AudioEncoder &operator=(AudioEncoder &&);

  AudioEncoder(std::string out_filename, CancelToken *cancel_token);

  int Encode(const Waveform &waveform);

  int FinishEncode();

  std::int64_t LastTimestamp();

  operator bool() { return static_cast<bool>(encoder_); }

  ~AudioEncoder();
};

} // namespace spleeter

#endif /// SPLEETER_AUDIO_FFMPEG_AUDIO_ADAPTER_H
