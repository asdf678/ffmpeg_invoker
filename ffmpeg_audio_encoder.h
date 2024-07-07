
#ifndef SPLEETER_FFMPEG_AUDIO_ENCODER
#define SPLEETER_FFMPEG_AUDIO_ENCODER
#include "common.h"
extern "C" {
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
}
#include "waveform.h"
#include <string>
namespace spleeter {
namespace codec {
int encode(const std::string &path, int src_sample_rate,
           AVSampleFormat src_sample_fmt, AVChannelLayout src_ch_layout,
           Waveform waveform, int bitrate, std::atomic_bool &cancel_token,
           ProgressCallback progress_callback);
}
} // namespace spleeter
#endif