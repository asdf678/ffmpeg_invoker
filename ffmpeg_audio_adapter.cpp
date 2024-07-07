#include "ffmpeg_audio_adapter.h"
#include "ffmpeg_audio_decoder.h"
#include "ffmpeg_audio_encoder.h"
#include "waveform.h"
#include <algorithm>
#include <assert.h>
#include <memory>
#include <string>

namespace spleeter {

static constexpr int kSampleRate = 44100;
static constexpr AVSampleFormat kSampleFormat = AV_SAMPLE_FMT_FLT;
static constexpr AVChannelLayout kChannelLayout = AV_CHANNEL_LAYOUT_STEREO;

int FfmpegAudioAdapter::Decode(const std::string &path,
                               const std::int64_t start,
                               const std::int64_t duration,
                               std::unique_ptr<Waveform> &result,
                               ProgressCallback progress_callback) {
  cancel_token_.store(false);
  return spleeter::codec::decode(path, kSampleRate, kSampleFormat,
                                 kChannelLayout, start, duration, cancel_token_,
                                 result, std::move(progress_callback));
}

int FfmpegAudioAdapter::Encode(Waveform waveform, std::string filename,
                               ProgressCallback progress_callback) {
  cancel_token_.store(false);
  return spleeter::codec::encode(filename, kSampleRate, kSampleFormat,
                                 kChannelLayout, std::move(waveform), -1,
                                 cancel_token_, std::move(progress_callback));
}
void FfmpegAudioAdapter::Cancel() { cancel_token_.store(true); }

} // namespace spleeter