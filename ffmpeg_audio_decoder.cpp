
#include "ffmpeg_audio_decoder.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavutil/channel_layout.h"
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

  int64_t delay = swr_get_delay(swr_ctx, audio_dec_ctx->sample_rate);
  int64_t dst_nb_samples =
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

std::unique_ptr<Waveform> decode(const std::string &path, int dst_rate,
                                 AVSampleFormat dst_sample_fmt,
                                 AVChannelLayout dst_ch_layout,
                                 const std::int64_t start,
                                 const std::int64_t duration) {
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
  std::unique_ptr<Waveform> waveform = std::make_unique<Waveform>();
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

  /* retrieve stream information */
  if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    fprintf(stderr, "Could not find stream information\n");
    goto end;
  }

  if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx,
                         AVMEDIA_TYPE_AUDIO) >= 0) {
    audio_stream = fmt_ctx->streams[audio_stream_idx];
  }

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
  av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", audio_dec_ctx->sample_fmt, 0);

  av_opt_set_chlayout(swr_ctx, "out_chlayout", &dst_ch_layout, 0);
  av_opt_set_int(swr_ctx, "out_sample_rate", dst_rate, 0);
  av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", dst_sample_fmt, 0);

  /* initialize the resampling context */
  if ((ret = swr_init(swr_ctx)) < 0) {
    fprintf(stderr, "Failed to initialize the resampling context\n");
    goto end;
  }

  if (audio_stream)
    printf("Demuxing audio from file '%s'\n", src_filename);

  /* read frames from the file */
  while (av_read_frame(fmt_ctx, pkt) >= 0) {
    // check if the packet belongs to a stream we are interested in, otherwise
    // skip it
    if (pkt->stream_index == audio_stream_idx)
      ret =
          decode_packet(audio_dec_ctx, pkt, frame, swr_ctx, dst_data, dst_rate,
                        dst_ch_layout, dst_sample_fmt, max_dst_nb_samples,
                        dst_linesize, nb_samples, waveform->data);
    av_packet_unref(pkt);
    if (ret < 0)
      break;
  }

  /* flush the decoders */
  if (audio_dec_ctx)
    decode_packet(audio_dec_ctx, NULL, frame, swr_ctx, dst_data, dst_rate,
                  dst_ch_layout, dst_sample_fmt, max_dst_nb_samples,
                  dst_linesize, nb_samples, waveform->data);
  //        av_init_packet(&packet);
  //        std::uint8_t *buffer = (std::uint8_t *)
  //        av_malloc(MAX_AUDIO_FRAME_SIZE * 2);
  //        ///TODO
  //        Waveform waveform{};
  //        std::int32_t nb_samples{0};
  //        while (av_read_frame(format_context, packet) >= 0) {
  //            AVFrame *frame = av_frame_alloc();
  ////        ASSERT_CHECK(frame) << "Failed to allocate frame";
  //
  //            ret = avcodec_send_packet(audio_codec_context, packet);
  ////        ASSERT_CHECK_LE(0, ret) << "Failed to send packet for decoding.
  ///(Returned: " << ret << ")";
  //            while (ret >= 0) {
  //                ret = avcodec_receive_frame(audio_codec_context, frame);
  //                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
  //                    break;
  //                }
  ////            ASSERT_CHECK_EQ(0, ret) << "Failed to decode received packet.
  ///(Returned: " << ret << ")";
  //
  //                auto buffer_size = frame->nb_samples *
  //                                   av_get_bytes_per_sample(audio_codec_context->sample_fmt);
  //                ret = swr_convert(swr_context, &buffer, buffer_size,
  //                                  (const std::uint8_t **) frame->data,
  //                                  frame->nb_samples);
  ////            ASSERT_CHECK_LE(0, ret) << "Failed to resample. (Returned: "
  ///<< ret << ")";
  //
  //                for (auto idx = 0; idx < buffer_size; ++idx) {
  //                    waveform.data.push_back(buffer[idx]);
  //                }
  //
  //                nb_samples += frame->nb_samples;
  //            }
  //
  //            av_frame_free(&frame);
  //            av_packet_unref(packet);
  //        }
  /// Update Audio properties before releasing resources
  /// TODO
  waveform->nb_frames = nb_samples;
  waveform->nb_channels = dst_ch_layout.nb_channels;
  //        av_packet_unref(packet);
  //        av_free(buffer);
  //        swr_free(&swr_context);
  //        avcodec_close(audio_codec_context);
  //        avformat_close_input(&format_context);
  //        avcodec_alloc_context3()
  //    SPLEETER_LOG(DEBUG) << "Decoded waveform with " << audio_properties_;
  //    SPLEETER_LOG(INFO) << "Loaded waveform from " << path << " using
  //    FFMPEG.";

end:
  avcodec_free_context(&audio_dec_ctx);
  avformat_close_input(&fmt_ctx);
  av_packet_free(&pkt);
  av_frame_free(&frame);

  if (dst_data)
    av_freep(&dst_data[0]);
  av_freep(&dst_data);
  swr_free(&swr_ctx);
  if (ret < 0) {
    return nullptr;
  }

  return waveform;
}
} // namespace codec

} // namespace spleeter
