
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

        class FFmpegAudioDecoder {
            int dst_sample_rate_;
            AVSampleFormat dst_sample_fmt_;
            AVChannelLayout dst_ch_layout_;
            std::atomic_bool *cancel_token_;
        public:
            FFmpegAudioDecoder(int dst_sample_rate,
                               AVSampleFormat dst_sample_fmt,
                               const AVChannelLayout &dst_ch_layout,
                               std::atomic_bool *cancel_token);

            int decode(std::string path, const std::int64_t start,
                       const std::int64_t duration, std::unique_ptr<Waveform> &result,
                       ProgressCallback progress_callback);

        };
    } // namespace codec
} // namespace spleeter

#endif