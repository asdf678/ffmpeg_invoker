#include "ffmpeg_audio_codec.h"
#include "ffmpeg_audio_decoder.h"
#include "ffmpeg_audio_encoder.h"
#include "waveform.h"
#include <algorithm>
#include <assert.h>
#include <cstddef>
#include <memory>
#include <string>

namespace spleeter {

static constexpr AVSampleFormat kSampleFormat = AV_SAMPLE_FMT_FLT;
static constexpr AVChannelLayout kChannelLayout = AV_CHANNEL_LAYOUT_STEREO;

AudioDecoder::AudioDecoder(std::string path, CancelToken *cancel_token)
    : decoder_(codec::FFmpegAudioDecoder::create(
          path, spleeter::constants::kSampleRate, kSampleFormat, kChannelLayout,
          cancel_token)) {}

int AudioDecoder::Decode(std::unique_ptr<Waveform> &result,
                         std::size_t max_frame_size) {

  return decoder_->decode(result, max_frame_size);
}

AudioDecoder &AudioDecoder::operator=(AudioDecoder &&) = default;

AudioDecoder::AudioDecoder(AudioDecoder &&) = default;

AudioDecoder::~AudioDecoder() = default;

AudioEncoder::AudioEncoder(std::string out_filename,
                           CancelToken *cancel_token)
    : encoder_(codec::FFmpegAudioEncoder::create(
          out_filename, spleeter::constants::kSampleRate, kSampleFormat,
          kChannelLayout, -1, cancel_token)) {}

int AudioEncoder::FinishEncode() {
  assert(encoder_);

  return encoder_->finish();
}

int AudioEncoder::Encode(const Waveform &waveform) {
  assert(encoder_);

  int ret = encoder_->encode(waveform);

  return ret;
}

std::int64_t AudioEncoder::LastTimestamp() {
  return encoder_->last_timestamp();
}

AudioEncoder &AudioEncoder::operator=(AudioEncoder &&) = default;

AudioEncoder::AudioEncoder(AudioEncoder &&) = default;

AudioEncoder::~AudioEncoder() = default;

} // namespace spleeter