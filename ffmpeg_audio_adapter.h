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
    namespace codec {
        class FFmpegAudioEncoder;

        class FFmpegAudioDecoder;
    }
    class FfmpegAudioAdapter final {
    private:
        std::unique_ptr<codec::FFmpegAudioEncoder> encoder_;
    public:

        FfmpegAudioAdapter(const FfmpegAudioAdapter &) = delete;

        FfmpegAudioAdapter &operator=(const FfmpegAudioAdapter &) = delete;

        FfmpegAudioAdapter(FfmpegAudioAdapter &&);

        FfmpegAudioAdapter &operator=(FfmpegAudioAdapter &&);

        FfmpegAudioAdapter(std::string out_filename, std::atomic_bool *cancel_token);

        static int Decode(const std::string path, const std::int64_t start,
                          const std::int64_t duration, std::unique_ptr<Waveform> &result,
                          ProgressCallback progress_callback, std::atomic_bool *cancel_token);

        int Encode(Waveform waveform, ProgressCallback progress_callback);

        int FinishEncode();

        ~FfmpegAudioAdapter();

    };
} // namespace spleeter

#endif /// SPLEETER_AUDIO_FFMPEG_AUDIO_ADAPTER_H
