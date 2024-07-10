
#include "ffmpeg_audio_decoder.h"
#include "common.h"
#include "ffmpeg_audio_common.h"
#include "libavutil/audio_fifo.h"
#include "waveform.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"
#include "libavutil/mathematics.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
}
namespace spleeter {
namespace codec {

static int output_audio_frame(
    AVFrame *frame, SwrContext *swr_ctx, AVCodecContext *audio_dec_ctx,
    uint8_t **&dst_data, int dst_rate, const AVChannelLayout &dst_ch_layout,
    enum AVSampleFormat dst_sample_fmt, int &max_dst_nb_samples,
    int &dst_linesize, std::uint64_t &nb_samples, std::vector<float> &container

) {
  //        size_t unpadded_linesize = frame->nb_samples *
  //        av_get_bytes_per_sample(
  //                static_cast<AVSampleFormat>(frame->format));
  //        printf("audio_frame n:%d nb_samples:%d pts:%s\n",
  //               audio_frame_count++, frame->nb_samples,
  //               av_ts2timestr(frame->pts, &audio_dec_ctx->time_base));

  // /* Write the raw audio data samples of the first plane. This works
  //  * fine for packed formats (e.g. AV_SAMPLE_FMT_S16). However,
  //  * most audio decoders output planar audio, which uses a separate
  //  * plane of audio samples for each channel (e.g. AV_SAMPLE_FMT_S16P).
  //  * In other words, this code will write only the first audio channel
  //  * in these cases.
  //  * You should use libswresample or libavfilter to convert the frame
  //  * to packed data. */
  // fwrite(frame->extended_data[0], 1, unpadded_linesize, audio_dst_file);
  int ret = 0;

  std::int64_t delay = swr_get_delay(swr_ctx, audio_dec_ctx->sample_rate);
  std::int64_t dst_nb_samples =
      av_rescale_rnd(delay + frame->nb_samples, dst_rate,
                     audio_dec_ctx->sample_rate, AV_ROUND_UP);
  if (dst_nb_samples > max_dst_nb_samples) {
    av_freep(&dst_data[0]);
    ret = av_samples_alloc(dst_data, &dst_linesize, dst_ch_layout.nb_channels,
                           dst_nb_samples, dst_sample_fmt, 1);
    if (ret < 0)
      return ret;
    max_dst_nb_samples = dst_nb_samples;
  }

  ret = swr_convert(swr_ctx, dst_data, dst_nb_samples,
                    (const uint8_t **)frame->extended_data, frame->nb_samples);
  if (ret < 0) {
    fprintf(stderr, "Error while converting\n");
  } else {

    //            auto bytes_per_sample =
    //            av_get_bytes_per_sample(dst_sample_fmt);
    int dst_bufsize = av_samples_get_buffer_size(
        &dst_linesize, dst_ch_layout.nb_channels, ret, dst_sample_fmt, 1);
    uint8_t *&c_data = dst_data[0];

    //            float *p = reinterpret_cast<float *>(c_data);
    //            int bytes_per_sample =
    //            av_get_bytes_per_sample(dst_sample_fmt);
    float *d = reinterpret_cast<float *>(dst_data[0]);
    for (int i = 0; i < ret; ++i) {
      for (int ch = 0; ch < dst_ch_layout.nb_channels; ++ch) {
        // fwrite(d + i, 1, bytes_per_sample, dst_file);
        //                    fwrite(d + ch, 1, bytes_per_sample, dst_file);
        container.push_back(*(d + ch));
      }
      d += dst_ch_layout.nb_channels;
    }

    nb_samples += ret;
    //            for (int i = 0; i < ret; i++) {
    //                for (int ch = 0; ch <dst_ch_layout.nb_channels; ch++) {
    //                    fwrite(frame->data[ch] + data_size * i, 1, data_size,
    //                    outfile);
    //                }
    //            }

    //            for (auto idx = 0; idx < dst_bufsize; ++idx) {
    //                container.push_back(static_cast<float>(c_data[idx]));
    //            }

    //            nb_samples += ret;
  }
  return ret;
}

static int decode_packet(AVCodecContext *dec, const AVPacket *pkt,
                         AVFrame *frame, SwrContext *swr_ctx,
                         uint8_t **&dst_data, int dst_rate,
                         const AVChannelLayout &dst_ch_layout,
                         enum AVSampleFormat dst_sample_fmt,
                         int &max_dst_nb_samples, int &dst_linesize,
                         std::uint64_t &nb_samples,
                         std::vector<float> &container) {
  int ret = 0;

  // submit the packet to the decoder
  ret = avcodec_send_packet(dec, pkt);
  if (ret < 0) {
    fprintf(stderr, "Error submitting a packet for decoding (%s)\n",
            av_err2str(ret));
    return ret;
  }

  // get all the available frames from the decoder
  while (ret >= 0) {
    ret = avcodec_receive_frame(dec, frame);
    if (ret < 0) {
      // those two return values are special and mean there is no output
      // frame available, but there were no errors during decoding
      if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
        return 0;

      fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
      return ret;
    }

    // write the frame data to output file
    ret = output_audio_frame(frame, swr_ctx, dec, dst_data, dst_rate,
                             dst_ch_layout, dst_sample_fmt, max_dst_nb_samples,
                             dst_linesize, nb_samples, container);

    av_frame_unref(frame);
    if (ret < 0)
      return ret;
  }

  return ret;
}

static int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx,
                              AVFormatContext *fmt_ctx, enum AVMediaType type) {
  int ret, stream_index;
  AVStream *st;
  const AVCodec *dec = NULL;

  ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
  if (ret < 0) {
    fprintf(stderr, "Could not find %s stream in input file\n",
            av_get_media_type_string(type));
    return ret;
  } else {
    stream_index = ret;
    st = fmt_ctx->streams[stream_index];

    /* find decoder for the stream */
    dec = avcodec_find_decoder(st->codecpar->codec_id);
    if (!dec) {
      fprintf(stderr, "Failed to find %s codec\n",
              av_get_media_type_string(type));
      return AVERROR(EINVAL);
    }

    /* Allocate a codec context for the decoder */
    *dec_ctx = avcodec_alloc_context3(dec);
    if (!*dec_ctx) {
      fprintf(stderr, "Failed to allocate the %s codec context\n",
              av_get_media_type_string(type));
      return AVERROR(ENOMEM);
    }

    /* Copy codec parameters from input stream to output codec context */
    if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
      fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
              av_get_media_type_string(type));
      return ret;
    }

    /* Init the decoders */
    if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
      fprintf(stderr, "Failed to open %s codec\n",
              av_get_media_type_string(type));
      return ret;
    }
    *stream_idx = stream_index;
  }

  return 0;
}
static int open_input_file(const char *filename,
                           AVFormatContext **input_format_context,
                           int *audio_stream_idx,
                           AVCodecContext **input_codec_context) {
  AVCodecContext *avctx;
  const AVCodec *input_codec;
  const AVStream *stream;
  int error;

  /* Open the input file to read from it. */
  if ((error = avformat_open_input(input_format_context, filename, NULL,
                                   NULL)) < 0) {
    fprintf(stderr, "Could not open input file '%s' (error '%s')\n", filename,
            av_err2str(error));
    *input_format_context = NULL;
    return error;
  }

  /* Get information on the input file (number of streams etc.). */
  if ((error = avformat_find_stream_info(*input_format_context, NULL)) < 0) {
    fprintf(stderr, "Could not open find stream info (error '%s')\n",
            av_err2str(error));
    avformat_close_input(input_format_context);
    return error;
  }

  /* Get information on the input file (number of streams etc.). */
  if ((*audio_stream_idx = av_find_best_stream(*input_format_context,
                                               AVMediaType::AVMEDIA_TYPE_AUDIO,
                                               -1, -1, NULL, 0)) < 0) {
    fprintf(stderr, "Could not open find stream info (error '%s')\n",
            av_err2str(error));
    avformat_close_input(input_format_context);
    return -1;
  }

  stream = (*input_format_context)->streams[*audio_stream_idx];

  /* Find a decoder for the audio stream. */
  if (!(input_codec = avcodec_find_decoder(stream->codecpar->codec_id))) {
    fprintf(stderr, "Could not find input codec\n");
    avformat_close_input(input_format_context);
    return AVERROR_EXIT;
  }

  /* Allocate a new decoding context. */
  avctx = avcodec_alloc_context3(input_codec);
  if (!avctx) {
    fprintf(stderr, "Could not allocate a decoding context\n");
    avformat_close_input(input_format_context);
    return AVERROR(ENOMEM);
  }

  /* Initialize the stream parameters with demuxer information. */
  error = avcodec_parameters_to_context(avctx, stream->codecpar);
  if (error < 0) {
    avformat_close_input(input_format_context);
    avcodec_free_context(&avctx);
    return error;
  }

  /* Open the decoder for the audio stream to use it later. */
  if ((error = avcodec_open2(avctx, input_codec, NULL)) < 0) {
    fprintf(stderr, "Could not open input codec (error '%s')\n",
            av_err2str(error));
    avcodec_free_context(&avctx);
    avformat_close_input(input_format_context);
    return error;
  }

  /* Set the packet timebase for the decoder. */
  avctx->pkt_timebase = stream->time_base;

  /* Save the decoder context for easier access later. */
  *input_codec_context = avctx;

  return 0;
}

inline void check_cancel_and_throw(std::atomic_bool &cancel_token) {
  CancelException::check_cancel_and_throw(cancel_token);
}
static int decode_audio_frame(AVFrame *frame,
                              AVFormatContext *input_format_context,
                              AVCodecContext *input_codec_context,
                              int *audio_st_index, int *data_present,
                              int *finished) {
  /* Packet used for temporary storage. */
  AVPacket *input_packet;
  int error;

  error = init_packet(&input_packet);
  if (error < 0)
    return error;

  *data_present = 0;
  *finished = 0;
  /* Read one audio frame from the input file into a temporary packet. */
  if ((error = av_read_frame(input_format_context, input_packet)) < 0) {
    /* If we are at the end of the file, flush the decoder below. */
    if (error == AVERROR_EOF)
      *finished = 1;
    else {
      fprintf(stderr, "Could not read frame (error '%s')\n", av_err2str(error));
      goto cleanup;
    }
  }

  if (input_packet->stream_index != *audio_st_index) {
    error = 0;
    goto cleanup;
  }

  /* Send the audio frame stored in the temporary packet to the decoder.
   * The input audio stream decoder is used to do this. */
  if ((error = avcodec_send_packet(input_codec_context, input_packet)) < 0) {
    fprintf(stderr, "Could not send packet for decoding (error '%s')\n",
            av_err2str(error));
    goto cleanup;
  }

  /* Receive one frame from the decoder. */
  error = avcodec_receive_frame(input_codec_context, frame);
  /* If the decoder asks for more data to be able to decode a frame,
   * return indicating that no data is present. */
  if (error == AVERROR(EAGAIN)) {
    error = 0;
    goto cleanup;
    /* If the end of the input file is reached, stop decoding. */
  } else if (error == AVERROR_EOF) {
    *finished = 1;
    error = 0;
    goto cleanup;
  } else if (error < 0) {
    fprintf(stderr, "Could not decode frame (error '%s')\n", av_err2str(error));
    goto cleanup;
    /* Default case: Return decoded data. */
  } else {
    *data_present = 1;
    goto cleanup;
  }

cleanup:
  av_packet_free(&input_packet);
  return error;
}
static int convert_samples(const uint8_t **input_data, uint8_t **converted_data,
                           const int frame_size, SwrContext *resample_context) {
  int ret;

  /* Convert the samples using the resampler. */
  if ((ret = swr_convert(resample_context, converted_data, frame_size,
                         input_data, frame_size)) < 0) {
    fprintf(stderr, "Could not convert input samples (error '%s')\n",
            av_err2str(ret));
  }

  return ret;
}
static int read_decode_convert_and_store(
    AVAudioFifo *fifo, AVFormatContext *input_format_context,
    AVCodecContext *input_codec_context, int out_nb_channels,
    AVSampleFormat out_sample_fmt, int out_sample_rate,
    SwrContext *resampler_context, int *audio_st_index, int *finished) {
  /* Temporary storage of the input samples of the frame read from the file. */
  AVFrame *input_frame = NULL;
  /* Temporary storage for the converted input samples. */
  uint8_t **converted_input_samples = NULL;
  int data_present;
  int ret = AVERROR_EXIT;

  /* Initialize temporary storage for one input frame. */
  if (init_input_frame(&input_frame))
    goto cleanup;
  /* Decode one frame worth of audio samples. */
  if (decode_audio_frame(input_frame, input_format_context, input_codec_context,
                         audio_st_index, &data_present, finished))
    goto cleanup;
  /* If we are at the end of the file and there are no more samples
   * in the decoder which are delayed, we are actually finished.
   * This must not be treated as an error. */
  if (*finished) {
    ret = 0;
    goto cleanup;
  }
  /* If there is decoded data, convert and store it. */
  if (data_present) {
    std::int64_t delay =
        swr_get_delay(resampler_context, input_codec_context->sample_rate);

    std::int64_t dst_nb_samples =
        av_rescale_rnd(delay + input_frame->nb_samples, out_sample_rate,
                       input_codec_context->sample_rate, AV_ROUND_UP);

    /* Initialize the temporary storage for the converted input samples. */
    if (init_converted_samples(&converted_input_samples, out_nb_channels,
                               dst_nb_samples, out_sample_fmt))
      goto cleanup;

    int converted_nb_samples = -1;

    /* Convert the samples using the resampler. */
    if ((converted_nb_samples = swr_convert(
             resampler_context, converted_input_samples, dst_nb_samples,
             (const uint8_t **)input_frame->extended_data,
             input_frame->nb_samples)) < 0) {
      fprintf(stderr, "Could not convert input samples (error '%s')\n",
              av_err2str(ret));
      goto cleanup;
    }

    /* Add the converted input samples to the FIFO buffer for later processing.
     */
    if (add_samples_to_fifo(fifo, converted_input_samples,
                            converted_nb_samples))
      goto cleanup;
    ret = 0;
  }
  ret = 0;

cleanup:
  if (converted_input_samples) {
    av_freep(&converted_input_samples[0]);
    free(converted_input_samples);
  }
  av_frame_free(&input_frame);

  return ret;
}
static int decode(std::string path, int dst_rate, AVSampleFormat dst_sample_fmt,
                  const AVChannelLayout &dst_ch_layout,
                  const std::int64_t start, const std::int64_t duration,
                  std::atomic_bool &cancel_token,
                  std::unique_ptr<Waveform> &result,
                  ProgressCallback progress_callback) {
  ///
  /// Open Input Audio
  ///

  int ret;
  AVFormatContext *fmt_ctx = nullptr;
  int audio_stream_idx = -1;
  const AVCodec *dec;
  SwrContext *swr_ctx = nullptr;
  AVCodecContext *audio_dec_ctx = nullptr;
  AVPacket *pkt = nullptr;
  AVFrame *frame = nullptr;
  AVStream *audio_stream = NULL;
  uint8_t **dst_data = NULL;
  std::uint64_t nb_samples{0};

  int max_dst_nb_samples = 0;
  int dst_linesize;
  const char *src_filename = path.c_str();
  Waveform waveform;
  bool canceled = false;
  auto last_progress_timestamp = get_current_timestamp();

  try {
    check_cancel_and_throw(cancel_token);

    ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize,
                                             dst_ch_layout.nb_channels, 512,
                                             dst_sample_fmt, 0);
    if (ret < 0) {
      goto end;
    }

    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
      fprintf(stderr, "Could not open source file %s\n", src_filename);
      goto end;
    }
    check_cancel_and_throw(cancel_token);

    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
      fprintf(stderr, "Could not find stream information\n");
      goto end;
    }

    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx,
                           AVMEDIA_TYPE_AUDIO) >= 0) {
      audio_stream = fmt_ctx->streams[audio_stream_idx];
    }
    check_cancel_and_throw(cancel_token);

    /* dump input information to stderr */
    av_dump_format(fmt_ctx, 0, src_filename, 0);

    if (!audio_stream) {
      fprintf(stderr,
              "Could not find audio or video stream in the input, aborting\n");
      ret = 1;
      goto end;
    }

    frame = av_frame_alloc();
    if (!frame) {
      fprintf(stderr, "Could not allocate frame\n");
      ret = AVERROR(ENOMEM);
      goto end;
    }

    pkt = av_packet_alloc();
    if (!pkt) {
      fprintf(stderr, "Could not allocate packet\n");
      ret = AVERROR(ENOMEM);
      goto end;
    }

    /* create resampler context */
    swr_ctx = swr_alloc();
    if (!swr_ctx) {
      fprintf(stderr, "Could not allocate resampler context\n");
      ret = AVERROR(ENOMEM);
      goto end;
    }

    /* set options */
    av_opt_set_chlayout(swr_ctx, "in_chlayout", &audio_dec_ctx->ch_layout, 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", audio_dec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", audio_dec_ctx->sample_fmt,
                          0);

    av_opt_set_chlayout(swr_ctx, "out_chlayout", &dst_ch_layout, 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", dst_rate, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

    /* initialize the resampling context */
    if ((ret = swr_init(swr_ctx)) < 0) {
      fprintf(stderr, "Failed to initialize the resampling context\n");
      goto end;
    }
    check_cancel_and_throw(cancel_token);

    if (audio_stream)
      printf("Demuxing audio from file '%s'\n", src_filename);
#if SPLEETER_ENABLE_PROGRESS_CALLBACK
    auto progress_fun = [&]() {
      auto now_progress_timestamp = get_current_timestamp();
      auto x = now_progress_timestamp - last_progress_timestamp;
      if (x.count() > 500) {
        progress_callback(av_rescale(nb_samples, 1000, dst_rate));
        last_progress_timestamp = now_progress_timestamp;
      }
    };
#endif

    /* read frames from the file */
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
      // check if the packet belongs to a stream we are interested in, otherwise
      // skip it
      if (pkt->stream_index == audio_stream_idx)
        ret = decode_packet(audio_dec_ctx, pkt, frame, swr_ctx, dst_data,
                            dst_rate, dst_ch_layout, dst_sample_fmt,
                            max_dst_nb_samples, dst_linesize, nb_samples,
                            waveform.data);
      av_packet_unref(pkt);
      if (ret < 0)
        break;
      check_cancel_and_throw(cancel_token);
#if SPLEETER_ENABLE_PROGRESS_CALLBACK
      progress_fun();
#endif
    }

    /* flush the decoders */
    if (audio_dec_ctx) {
      decode_packet(audio_dec_ctx, NULL, frame, swr_ctx, dst_data, dst_rate,
                    dst_ch_layout, dst_sample_fmt, max_dst_nb_samples,
                    dst_linesize, nb_samples, waveform.data);
#if SPLEETER_ENABLE_PROGRESS_CALLBACK
      progress_fun();
#endif
    }
    check_cancel_and_throw(cancel_token);

    waveform.nb_frames = nb_samples;
    waveform.nb_channels = dst_ch_layout.nb_channels;
    result.reset(new Waveform(std::move(waveform)));
  } catch (const CancelException &e) {
    canceled = true;
  }

end:
  avcodec_free_context(&audio_dec_ctx);
  avformat_close_input(&fmt_ctx);
  av_packet_free(&pkt);
  av_frame_free(&frame);

  if (dst_data)
    av_freep(&dst_data[0]);
  av_freep(&dst_data);
  swr_free(&swr_ctx);
  //   if (ret < 0) {
  //   return nullptr;
  // }

  // return waveform;
  if (canceled) {
    return 0;
  }
  if (ret < 0) {
    return ret;
  }

  return 1;
} // namespace codec
FFmpegAudioDecoder::FFmpegAudioDecoder(std::string path, int dst_sample_rate,
                                       AVSampleFormat dst_sample_fmt,
                                       const AVChannelLayout &dst_ch_layout,
                                       std::atomic_bool *cancel_token)
    : path_(path), dst_sample_rate_(dst_sample_rate),
      dst_sample_fmt_(dst_sample_fmt), cancel_token_(cancel_token) {
  av_channel_layout_copy(&dst_ch_layout_, &dst_ch_layout);
}

// int FFmpegAudioDecoder::decode(std::string path, const std::int64_t start,
//                                const std::int64_t duration,
//                                std::unique_ptr<Waveform> &result,
//                                ProgressCallback progress_callback) {

//   return spleeter::codec::decode(
//       path, dst_sample_rate_, dst_sample_fmt_, dst_ch_layout_, start,
//       duration, *cancel_token_, result, std::move(progress_callback));
// }

std::unique_ptr<FFmpegAudioDecoder> FFmpegAudioDecoder::create(
    std::string path, int dst_sample_rate, AVSampleFormat dst_sample_fmt,
    const AVChannelLayout &dst_ch_layout, std::atomic_bool *cancel_token) {
  std::unique_ptr<FFmpegAudioDecoder> decoder =
      std::make_unique<FFmpegAudioDecoder>(
          path, dst_sample_rate, dst_sample_fmt, dst_ch_layout, cancel_token);
  if (open_input_file(path.c_str(), &decoder->input_format_context_,
                      &decoder->audio_stream_idx_,
                      &decoder->input_codec_context_)) {
    return nullptr;
  }

  if (init_resampler(&decoder->input_codec_context_->ch_layout,
                     decoder->input_codec_context_->sample_fmt,
                     decoder->input_codec_context_->sample_rate, &dst_ch_layout,
                     dst_sample_fmt, dst_sample_rate,
                     &decoder->resample_context_))
    return nullptr;
  if (init_fifo(&decoder->fifo_, dst_sample_fmt, dst_ch_layout.nb_channels, 1))
    return nullptr;
  return decoder;
}

int FFmpegAudioDecoder::decode(std::unique_ptr<Waveform> &result,
                               std::size_t max_frame_size) {
  int ret = AVERROR_EXIT;
  bool canceled = false;

  /// 不能去goto，因为fifo中可能还有未读完的数据
  // if (finished_) {
  //   ret = 0;
  //   goto cleanup;
  // }

  try {
    while (!finished_ && av_audio_fifo_size(fifo_) < max_frame_size) {
      // av_log(nullptr, AV_LOG_DEBUG, "load\n");
      if (read_decode_convert_and_store(
              fifo_, input_format_context_, input_codec_context_,
              dst_ch_layout_.nb_channels, dst_sample_fmt_, dst_sample_rate_,
              resample_context_, &audio_stream_idx_, &finished_))
        goto cleanup;

      check_cancel_and_throw(*cancel_token_);

      /* If we are at the end of the input file, we continue
       * encoding the remaining audio samples to the output file. */
      if (finished_)
        break;

      // av_audio_fifo_read(AVAudioFifo *af, void **data, int nb_samples)

      // av_samples_get_buffer_size(int *linesize, int nb_channels, int
      // nb_samples, enum AVSampleFormat sample_fmt, int align)
    }

    if (av_audio_fifo_size(fifo_) > 0) {
      int waveform_data_size =
          av_samples_get_buffer_size(nullptr, dst_ch_layout_.nb_channels,
                                     max_frame_size, dst_sample_fmt_, 1) /
          av_get_bytes_per_sample(dst_sample_fmt_);

      std::size_t nb_samples = std::min(
          static_cast<std::size_t>(av_audio_fifo_size(fifo_)), max_frame_size);

      std::vector<float> waveform_data(nb_samples * dst_ch_layout_.nb_channels);

      void *data[1] = {reinterpret_cast<void *>(waveform_data.data())};

      int waveform_nb_samples = av_audio_fifo_read(fifo_, data, nb_samples);

      // printf("read,max%ld,1 %ld,2 %d\n", max_frame_size, nb_samples,
      //        waveform_nb_samples);

      if (waveform_nb_samples > 0) {
        waveform_data.resize(waveform_nb_samples * dst_ch_layout_.nb_channels);
        result.reset(new spleeter::Waveform{
            .nb_frames = static_cast<std ::size_t>(waveform_nb_samples),
            .nb_channels = dst_ch_layout_.nb_channels,
            .data = std::move(waveform_data),
        });
      }
    }
    check_cancel_and_throw(*cancel_token_);

    ret = 0;
  } catch (const CancelException &) {
    canceled = true;
  }

cleanup:
  if (canceled) {
    return 0;
  }
  if (ret < 0) {
    return ret;
  }
  return 1;
}

FFmpegAudioDecoder::~FFmpegAudioDecoder() {
  if (fifo_)
    av_audio_fifo_free(fifo_);
  swr_free(&resample_context_);
  if (input_codec_context_)
    avcodec_free_context(&input_codec_context_);
  if (input_format_context_)
    avformat_close_input(&input_format_context_);
}

} // namespace codec
} // namespace spleeter
