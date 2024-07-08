
#ifndef SPLEETER_FFMPEG_AUDIO_ENCODER
#define SPLEETER_FFMPEG_AUDIO_ENCODER
#include "common.h"
#include "waveform.h"
#include <cassert>
#include <memory>
#include <string>
extern "C" {
#include "libavutil/avassert.h"
#include "libavutil/channel_layout.h"
#include "libavutil/frame.h"
#include "libavutil/samplefmt.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libswresample/swresample.h>
}

namespace spleeter {
namespace codec {
// int encode(const std::string &path, int src_sample_rate,
//            AVSampleFormat src_sample_fmt, AVChannelLayout src_ch_layout,
//            Waveform waveform, int bitrate, std::atomic_bool &cancel_token,
//            ProgressCallback progress_callback);

class FFmpegAudioEncoder {
  int src_sample_rate_;
  AVSampleFormat src_sample_fmt_;
  AVChannelLayout src_ch_layout_;
  int bitrate_;
  std::atomic_bool *cancel_token_;
  std::string path_;

  AVFormatContext *output_format_context_ = NULL;
  AVCodecContext *output_codec_context_ = NULL;
  SwrContext *resample_context_ = NULL;
  AVAudioFifo *fifo_ = NULL;

  //   std::unique_ptr<FramesManager> frame_manager_;

  int64_t pts = {0};

public:
  FFmpegAudioEncoder(std::string path, int src_sample_rate,
                     AVSampleFormat src_sample_fmt,
                     const AVChannelLayout &src_ch_layout, int bitrate,
                     std::atomic_bool *cancel_token);

public:
  static std::unique_ptr<FFmpegAudioEncoder>
  create(std::string path, int src_sample_rate, AVSampleFormat src_sample_fmt,
         const AVChannelLayout &src_ch_layout, int bitrate,
         std::atomic_bool *cancel_token);

  int encode(Waveform waveform);

  int finish();

  virtual ~FFmpegAudioEncoder();
};
} // namespace codec

} // namespace spleeter
#endif