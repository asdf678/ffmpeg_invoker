
#ifndef SPLEETER_FFMPEG_AUDIO_DECODER_H
#define SPLEETER_FFMPEG_AUDIO_DECODER_H
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/channel_layout.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
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
  std::string path_;

  AVFormatContext *input_format_context_{nullptr};
  int audio_stream_idx_ = {-1};
  SwrContext *resample_context_{nullptr};
  AVCodecContext *input_codec_context_{nullptr};
  AVAudioFifo *fifo_{nullptr};
  int finished_{0};

public:
  FFmpegAudioDecoder(std::string path, int dst_sample_rate,
                     AVSampleFormat dst_sample_fmt,
                     const AVChannelLayout &dst_ch_layout,
                     std::atomic_bool *cancel_token);

  static std::unique_ptr<FFmpegAudioDecoder>
  create(std::string path, int dst_sample_rate, AVSampleFormat dst_sample_fmt,
         const AVChannelLayout &dst_ch_layout, std::atomic_bool *cancel_token);

  // int decode(std::string path, const std::int64_t start,
  //            const std::int64_t duration, std::unique_ptr<Waveform> &result,
  //            ProgressCallback progress_callback);

  int decode(std::unique_ptr<Waveform> &result, std::size_t max_frame_size);

  bool finished() { return finished_; }

  ~FFmpegAudioDecoder();
};
} // namespace codec
} // namespace spleeter

#endif