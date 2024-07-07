
#ifndef SPLEETER_FFMPEG_AUDIO_DECODER
#define SPLEETER_FFMPEG_AUDIO_DECODER
extern "C" {
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
}

#include "waveform.h"
#include <memory>
namespace spleeter {
namespace codec {

std::unique_ptr<Waveform> decode(const std::string &path, int dst_rate,
                                 AVSampleFormat dst_sample_fmt,
                                 AVChannelLayout dst_ch_layout,
                                 const std::int64_t start,
                                 const std::int64_t duration);
} // namespace codec
} // namespace spleeter

#endif