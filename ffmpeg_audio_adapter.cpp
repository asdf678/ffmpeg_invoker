#include "ffmpeg_audio_adapter.h"
#include "ffmpeg_audio_decoder.h"
#include "ffmpeg_audio_encoder.h"
#include "waveform.h"
#include <algorithm>
#include <assert.h>
#include <memory>
#include <string>

namespace spleeter {

    static constexpr AVSampleFormat kSampleFormat = AV_SAMPLE_FMT_FLT;
    static constexpr AVChannelLayout kChannelLayout = AV_CHANNEL_LAYOUT_STEREO;


    FfmpegAudioAdapter::FfmpegAudioAdapter(std::string out_filename, std::atomic_bool *cancel_token)
            : encoder_(
            codec::FFmpegAudioEncoder::create(out_filename, spleeter::constants::kSampleRate,
                                              kSampleFormat,
                                              kChannelLayout, -1, cancel_token)) {

    }

    int FfmpegAudioAdapter::Decode(const std::string path,
                                   const std::int64_t start,
                                   const std::int64_t duration,
                                   std::unique_ptr<Waveform> &result,
                                   ProgressCallback progress_callback,
                                   std::atomic_bool *cancel_token) {
        std::unique_ptr<codec::FFmpegAudioDecoder> decoder = std::make_unique<codec::FFmpegAudioDecoder>(
                spleeter::constants::kSampleRate, kSampleFormat, kChannelLayout,
                cancel_token);

//        return spleeter::codec::decode(path, kSampleRate, kSampleFormat,
//                                       kChannelLayout, start, duration, cancel_token_,
//                                       result, std::move(progress_callback));
        if (!decoder) {
            return -1;
        }
        return decoder->decode(path, start, duration, result, std::move(progress_callback));
    }

    int FfmpegAudioAdapter::Encode(Waveform waveform, ProgressCallback progress_callback) {
        //   d = new spleeter::codec::FFmpegAudioEncoder()
//        auto encoder = spleeter::codec::FFmpegAudioEncoder::create(
//                filename, kSampleRate, kSampleFormat, kChannelLayout, -1, &cancel_token_);
        if (!encoder_) {
            return -1;
        }
        int ret = encoder_->encode(std::move(waveform));

        return ret;
        //   return spleeter::codec::encode(filename, kSampleRate, kSampleFormat,
        //                                  kChannelLayout, std::move(waveform), -1,
        //                                  cancel_token_,
        //                                  std::move(progress_callback));
    }

    int FfmpegAudioAdapter::FinishEncode() {
//        if (ret > 0) {
//            ret = encoder_->finish();
//        }
        if (!encoder_) {
            return -1;
        }
        int ret = encoder_->finish();
        return ret;
    }

    FfmpegAudioAdapter::~FfmpegAudioAdapter() = default;

    FfmpegAudioAdapter &FfmpegAudioAdapter::operator=(FfmpegAudioAdapter &&) = default;

    FfmpegAudioAdapter::FfmpegAudioAdapter(FfmpegAudioAdapter &&) = default;
} // namespace spleeter