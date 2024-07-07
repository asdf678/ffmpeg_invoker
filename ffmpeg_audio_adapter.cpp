///
/// @file
/// @brief Contains definitions for FFMPEG Audio Adapter class methods
/// @copyright Copyright (c) 2020, MIT License
///
#include "ffmpeg_audio_adapter.h"
#include "ffmpeg_audio_decoder.h"
#include "ffmpeg_audio_encoder.h"
#include <string>

namespace spleeter {

static constexpr int kSampleRate = 44100;
static constexpr AVSampleFormat kSampleFormat = AV_SAMPLE_FMT_FLT;
static constexpr AVChannelLayout kChannelLayout = AV_CHANNEL_LAYOUT_STEREO;

std::unique_ptr<Waveform>
FfmpegAudioAdapter::Load(const std::string &path, const std::int64_t start,
                         const std::int64_t duration) {
  return spleeter::codec::decode(path, kSampleRate, kSampleFormat,
                                 kChannelLayout, start, duration);
}

int FfmpegAudioAdapter::Save(Waveform waveform, std::string filename) {
  std::string path = filename;
  return spleeter::codec::encode(path, kSampleRate, kSampleFormat,
                                 kChannelLayout, std::move(waveform), -1);
}

} // namespace spleeter