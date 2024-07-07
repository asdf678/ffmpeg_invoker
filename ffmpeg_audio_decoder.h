
#ifndef SPLEETER_FFMPEG_AUDIO_DECODER
#define SPLEETER_FFMPEG_AUDIO_DECODER
extern "C" {
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
}
#include "common.h"
#include "waveform.h"
#include <memory>
namespace spleeter {
namespace codec {
int decode(const std::string &path, int dst_rate, AVSampleFormat dst_sample_fmt,
           AVChannelLayout dst_ch_layout, const std::int64_t start,
           const std::int64_t duration, std::atomic_bool &cancel_token,
           std::unique_ptr<Waveform> &result,
           ProgressCallback progress_callback);
} // namespace codec
} // namespace spleeter

#endif