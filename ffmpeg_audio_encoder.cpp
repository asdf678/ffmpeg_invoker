
#include "ffmpeg_audio_encoder.h"

#include "common.h"
#include "waveform.h"
#include <algorithm>
#include <assert.h>
#include <memory>
#include <vector>

namespace spleeter {
namespace codec {
class FramesManager {
  AVAudioFifo *audio_fifo_{nullptr};
  AVSampleFormat sample_fmt_;
  AVChannelLayout *channel_layout_;
  int sample_rate_;

public:
  FramesManager(AVSampleFormat sample_fmt, AVChannelLayout *channel_layout,
                int sample_rate)
      : sample_fmt_(sample_fmt), channel_layout_(channel_layout),
        sample_rate_(sample_rate) {}
  int alloc(Waveform waveform) {
    AVAudioFifo *fifo = av_audio_fifo_alloc(
        sample_fmt_, channel_layout_->nb_channels, waveform.nb_frames);
    if (!fifo) {
      return -1;
    }

    void *dd = reinterpret_cast<void *>(waveform.data.data());
    if (av_audio_fifo_write(fifo, &dd, waveform.nb_frames) < 0) {
      av_audio_fifo_free(fifo);
      return -1;
    }
    audio_fifo_ = fifo;
    return 0;
  }

  int read(AVFrame *frame, int max_samples, int *data_present, int *finished) {
    assert(audio_fifo_);

    *data_present = 0;
    *finished = 0;

    auto fifo_size = av_audio_fifo_size(audio_fifo_);

    if (fifo_size == 0) {
      *finished = 1;
      return 0;
    }

    int nb_samples = std::min(fifo_size, max_samples);

    frame->nb_samples = nb_samples;
    frame->format = sample_fmt_;
    frame->sample_rate = sample_rate_;
    av_channel_layout_copy(&frame->ch_layout, channel_layout_);

    if (av_frame_get_buffer(frame, 0) < 0) {
      return -1;
    }

    int nb = av_audio_fifo_read(audio_fifo_, (void **)frame->data,
                                frame->nb_samples);

    if (nb > 0) {
      *data_present = 1;
      return 0;
    }
    if (nb == 0) {
      *finished = 1;
      return 0;
    }

    return -1;
  }

  ~FramesManager() {
    if (audio_fifo_) {
      av_audio_fifo_free(audio_fifo_);
      audio_fifo_ = nullptr;
    }
  }
};
static int open_output_file(const char *filename, int sample_rate,
                            AVSampleFormat sample_fmt, int nb_channels,
                            int bitrate,
                            AVFormatContext **output_format_context,
                            AVCodecContext **output_codec_context) {
  AVCodecContext *avctx = NULL;
  AVIOContext *output_io_context = NULL;
  AVStream *stream = NULL;
  const AVCodec *output_codec = NULL;
  int error;

  /* Open the output file to write to it. */
  if ((error = avio_open(&output_io_context, filename, AVIO_FLAG_WRITE)) < 0) {
    fprintf(stderr, "Could not open output file '%s' (error '%s')\n", filename,
            av_err2str(error));
    return error;
  }

  /* Create a new format context for the output container format. */
  if (!(*output_format_context = avformat_alloc_context())) {
    fprintf(stderr, "Could not allocate output format context\n");
    return AVERROR(ENOMEM);
  }

  /* Associate the output file (pointer) with the container format context. */
  (*output_format_context)->pb = output_io_context;

  /* Guess the desired container format based on the file extension. */
  if (!((*output_format_context)->oformat =
            av_guess_format(NULL, filename, NULL))) {
    fprintf(stderr, "Could not find output file format\n");
    goto cleanup;
  }

  if (!((*output_format_context)->url = av_strdup(filename))) {
    fprintf(stderr, "Could not allocate url.\n");
    error = AVERROR(ENOMEM);
    goto cleanup;
  }

  /* Find the encoder to be used by its name. */
  if (!(output_codec = avcodec_find_encoder(
            (*output_format_context)->oformat->audio_codec))) {
    fprintf(stderr, "Could not find an AAC encoder.\n");
    goto cleanup;
  }

  /* Create a new audio stream in the output file container. */
  if (!(stream = avformat_new_stream(*output_format_context, NULL))) {
    fprintf(stderr, "Could not create new stream\n");
    error = AVERROR(ENOMEM);
    goto cleanup;
  }

  avctx = avcodec_alloc_context3(output_codec);
  if (!avctx) {
    fprintf(stderr, "Could not allocate an encoding context\n");
    error = AVERROR(ENOMEM);
    goto cleanup;
  }

  /* Set the basic encoder parameters.
   * The input file's sample rate is used to avoid a sample rate conversion. */
  av_channel_layout_default(&avctx->ch_layout, nb_channels);
  avctx->sample_rate = sample_rate;
  avctx->sample_fmt = output_codec->sample_fmts[0];
  if (bitrate > 0) {
    avctx->bit_rate = bitrate;
  }

  /* Set the sample rate for the container. */
  stream->time_base.den = sample_rate;
  stream->time_base.num = 1;

  /* Some container formats (like MP4) require global headers to be present.
   * Mark the encoder so that it behaves accordingly. */
  if ((*output_format_context)->oformat->flags & AVFMT_GLOBALHEADER)
    avctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  /* Open the encoder for the audio stream to use it later. */
  if ((error = avcodec_open2(avctx, output_codec, NULL)) < 0) {
    fprintf(stderr, "Could not open output codec (error '%s')\n",
            av_err2str(error));
    goto cleanup;
  }

  error = avcodec_parameters_from_context(stream->codecpar, avctx);
  if (error < 0) {
    fprintf(stderr, "Could not initialize stream parameters\n");
    goto cleanup;
  }

  /* Save the encoder context for easier access later. */
  *output_codec_context = avctx;

  return 0;

cleanup:
  avcodec_free_context(&avctx);
  avio_closep(&(*output_format_context)->pb);
  avformat_free_context(*output_format_context);
  *output_format_context = NULL;
  return error < 0 ? error : AVERROR_EXIT;
}

static int init_packet(AVPacket **packet) {
  if (!(*packet = av_packet_alloc())) {
    fprintf(stderr, "Could not allocate packet\n");
    return AVERROR(ENOMEM);
  }
  return 0;
}
static int init_input_frame(AVFrame **frame) {
  if (!(*frame = av_frame_alloc())) {
    fprintf(stderr, "Could not allocate input frame\n");
    return AVERROR(ENOMEM);
  }
  return 0;
}

static int init_resampler(const AVChannelLayout *in_ch_layout,
                          enum AVSampleFormat in_sample_fmt, int in_sample_rate,
                          AVCodecContext *output_codec_context,
                          SwrContext **resample_context) {
  int error;

  /*
   * Create a resampler context for the conversion.
   * Set the conversion parameters.
   */
  error = swr_alloc_set_opts2(
      resample_context, &output_codec_context->ch_layout,
      output_codec_context->sample_fmt, output_codec_context->sample_rate,
      in_ch_layout, in_sample_fmt, in_sample_rate, 0, NULL);
  if (error < 0) {
    fprintf(stderr, "Could not allocate resample context\n");
    return error;
  }
  /*
   * Perform a sanity check so that the number of converted samples is
   * not greater than the number of samples to be converted.
   * If the sample rates differ, this case has to be handled differently
   */
  av_assert0(output_codec_context->sample_rate == in_sample_rate);

  /* Open the resampler with the specified parameters. */
  if ((error = swr_init(*resample_context)) < 0) {
    fprintf(stderr, "Could not open resample context\n");
    swr_free(resample_context);
    return error;
  }
  return 0;
}

static int init_fifo(AVAudioFifo **fifo, AVCodecContext *output_codec_context) {
  /* Create the FIFO buffer based on the specified output sample format. */
  if (!(*fifo = av_audio_fifo_alloc(output_codec_context->sample_fmt,
                                    output_codec_context->ch_layout.nb_channels,
                                    1))) {
    fprintf(stderr, "Could not allocate FIFO\n");
    return AVERROR(ENOMEM);
  }
  return 0;
}

/**
 * Write the header of the output file container.
 * @param output_format_context Format context of the output file
 * @return Error code (0 if successful)
 */
static int write_output_file_header(AVFormatContext *output_format_context) {
  int error;
  if ((error = avformat_write_header(output_format_context, NULL)) < 0) {
    fprintf(stderr, "Could not write output file header (error '%s')\n",
            av_err2str(error));
    return error;
  }
  return 0;
}

static int init_converted_samples(uint8_t ***converted_input_samples,
                                  AVCodecContext *output_codec_context,
                                  int frame_size) {
  int error;

  /* Allocate as many pointers as there are audio channels.
   * Each pointer will later point to the audio samples of the corresponding
   * channels (although it may be NULL for interleaved formats).
   */

  if (!(*converted_input_samples =
            new uint8_t *[output_codec_context->ch_layout.nb_channels])) {
    fprintf(stderr, "Could not allocate converted input sample pointers\n");
    return AVERROR(ENOMEM);
  }

  // if (!(*converted_input_samples =
  //           calloc(output_codec_context->ch_layout.nb_channels,
  //                  sizeof(**converted_input_samples)))) {
  //   fprintf(stderr, "Could not allocate converted input sample pointers\n");
  //   return AVERROR(ENOMEM);
  // }

  /* Allocate memory for the samples of all channels in one consecutive
   * block for convenience. */
  if ((error = av_samples_alloc(*converted_input_samples, NULL,
                                output_codec_context->ch_layout.nb_channels,
                                frame_size, output_codec_context->sample_fmt,
                                0)) < 0) {
    fprintf(stderr, "Could not allocate converted input samples (error '%s')\n",
            av_err2str(error));
    av_freep(&(*converted_input_samples)[0]);
    // free(*converted_input_samples);
    delete[] *converted_input_samples;
    return error;
  }
  return 0;
}
static int convert_samples(const uint8_t **input_data, uint8_t **converted_data,
                           const int frame_size, SwrContext *resample_context) {
  int error;

  /* Convert the samples using the resampler. */
  if ((error = swr_convert(resample_context, converted_data, frame_size,
                           input_data, frame_size)) < 0) {
    fprintf(stderr, "Could not convert input samples (error '%s')\n",
            av_err2str(error));
    return error;
  }

  return 0;
}

static int add_samples_to_fifo(AVAudioFifo *fifo,
                               uint8_t **converted_input_samples,
                               const int frame_size) {
  int error;

  /* Make the FIFO as large as it needs to be to hold both,
   * the old and the new samples. */
  if ((error = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) +
                                               frame_size)) < 0) {
    fprintf(stderr, "Could not reallocate FIFO\n");
    return error;
  }

  /* Store the new samples in the FIFO buffer. */
  if (av_audio_fifo_write(fifo, (void **)converted_input_samples, frame_size) <
      frame_size) {
    fprintf(stderr, "Could not write data to FIFO\n");
    return AVERROR_EXIT;
  }
  return 0;
}

static int read_decode_convert_and_store(AVAudioFifo *fifo,
                                         AVCodecContext *output_codec_context,
                                         SwrContext *resampler_context,
                                         int *finished,
                                         FramesManager &frame_manager) {

  /* Temporary storage of the input samples of the frame read from the file. */
  AVFrame *input_frame = NULL;

  /* Temporary storage for the converted input samples. */
  uint8_t **converted_input_samples = NULL;
  int data_present;
  int ret = AVERROR_EXIT;

  /* Initialize temporary storage for one input frame. */
  if (init_input_frame(&input_frame))
    goto cleanup;

  if (frame_manager.read(input_frame, 10240, &data_present, finished)) {
    goto cleanup;
  }
  if (*finished) {
    ret = 0;
    goto cleanup;
  }

  /* If there is decoded data, convert and store it. */
  if (data_present) {
    /* Initialize the temporary storage for the converted input samples. */
    if (init_converted_samples(&converted_input_samples, output_codec_context,
                               input_frame->nb_samples))
      goto cleanup;

    if (convert_samples((const uint8_t **)input_frame->extended_data,
                        converted_input_samples, input_frame->nb_samples,
                        resampler_context))
      goto cleanup;

    /* Add the converted input samples to the FIFO buffer for later processing.
     */
    if (add_samples_to_fifo(fifo, converted_input_samples,
                            input_frame->nb_samples))
      goto cleanup;
    ret = 0;
  }
  ret = 0;

cleanup:
  if (converted_input_samples) {
    av_freep(&converted_input_samples[0]);
    // free(converted_input_samples);
    delete[] converted_input_samples;
  }
  av_frame_free(&input_frame);
  return ret;
}

static int init_output_frame(AVFrame **frame,
                             AVCodecContext *output_codec_context,
                             int frame_size) {
  int error;

  /* Create a new frame to store the audio samples. */
  if (!(*frame = av_frame_alloc())) {
    fprintf(stderr, "Could not allocate output frame\n");
    return AVERROR_EXIT;
  }

  /* Set the frame's parameters, especially its size and format.
   * av_frame_get_buffer needs this to allocate memory for the
   * audio samples of the frame.
   * Default channel layouts based on the number of channels
   * are assumed for simplicity. */
  (*frame)->nb_samples = frame_size;
  av_channel_layout_copy(&(*frame)->ch_layout,
                         &output_codec_context->ch_layout);
  (*frame)->format = output_codec_context->sample_fmt;
  (*frame)->sample_rate = output_codec_context->sample_rate;

  /* Allocate the samples of the created frame. This call will make
   * sure that the audio frame can hold as many samples as specified. */
  if ((error = av_frame_get_buffer(*frame, 0)) < 0) {
    fprintf(stderr, "Could not allocate output frame samples (error '%s')\n",
            av_err2str(error));
    av_frame_free(frame);
    return error;
  }

  return 0;
}

static int encode_audio_frame(AVFrame *frame,
                              AVFormatContext *output_format_context,
                              AVCodecContext *output_codec_context,
                              int *data_present, int64_t &pts) {
  /* Packet used for temporary storage. */
  AVPacket *output_packet;
  int error;

  error = init_packet(&output_packet);
  if (error < 0)
    return error;

  /* Set a timestamp based on the sample rate for the container. */
  if (frame) {
    frame->pts = pts;
    pts += frame->nb_samples;
  }

  *data_present = 0;
  /* Send the audio frame stored in the temporary packet to the encoder.
   * The output audio stream encoder is used to do this. */
  error = avcodec_send_frame(output_codec_context, frame);
  /* Check for errors, but proceed with fetching encoded samples if the
   *  encoder signals that it has nothing more to encode. */
  if (error < 0 && error != AVERROR_EOF) {
    fprintf(stderr, "Could not send packet for encoding (error '%s')\n",
            av_err2str(error));
    goto cleanup;
  }

  /* Receive one encoded frame from the encoder. */
  error = avcodec_receive_packet(output_codec_context, output_packet);
  /* If the encoder asks for more data to be able to provide an
   * encoded frame, return indicating that no data is present. */
  if (error == AVERROR(EAGAIN)) {
    error = 0;
    goto cleanup;
    /* If the last frame has been encoded, stop encoding. */
  } else if (error == AVERROR_EOF) {
    error = 0;
    goto cleanup;
  } else if (error < 0) {
    fprintf(stderr, "Could not encode frame (error '%s')\n", av_err2str(error));
    goto cleanup;
    /* Default case: Return encoded data. */
  } else {
    *data_present = 1;
  }

  /* Write one audio frame from the temporary packet to the output file. */
  if (*data_present &&
      (error = av_write_frame(output_format_context, output_packet)) < 0) {
    fprintf(stderr, "Could not write frame (error '%s')\n", av_err2str(error));
    goto cleanup;
  }

cleanup:
  av_packet_free(&output_packet);
  return error;
}

static int load_encode_and_write(AVAudioFifo *fifo,
                                 AVFormatContext *output_format_context,
                                 AVCodecContext *output_codec_context,
                                 int64_t &pts) {
  /* Temporary storage of the output samples of the frame written to the file.
   */
  AVFrame *output_frame;
  /* Use the maximum number of possible samples per frame.
   * If there is less than the maximum possible frame size in the FIFO
   * buffer use this number. Otherwise, use the maximum possible frame size. */
  const int frame_size =
      FFMIN(av_audio_fifo_size(fifo), output_codec_context->frame_size);
  int data_written;

  /* Initialize temporary storage for one output frame. */
  if (init_output_frame(&output_frame, output_codec_context, frame_size))
    return AVERROR_EXIT;

  /* Read as many samples from the FIFO buffer as required to fill the frame.
   * The samples are stored in the frame temporarily. */
  if (av_audio_fifo_read(fifo, (void **)output_frame->data, frame_size) <
      frame_size) {
    fprintf(stderr, "Could not read data from FIFO\n");
    av_frame_free(&output_frame);
    return AVERROR_EXIT;
  }

  /* Encode one frame worth of audio samples. */
  if (encode_audio_frame(output_frame, output_format_context,
                         output_codec_context, &data_written, pts)) {
    av_frame_free(&output_frame);
    return AVERROR_EXIT;
  }
  av_frame_free(&output_frame);
  return 0;
}
static int write_output_file_trailer(AVFormatContext *output_format_context) {
  int error;
  if ((error = av_write_trailer(output_format_context)) < 0) {
    fprintf(stderr, "Could not write output file trailer (error '%s')\n",
            av_err2str(error));
    return error;
  }
  return 0;
}
inline void check_cancel_and_throw(std::atomic_bool &cancel_token) {
  CancelException::check_cancel_and_throw(cancel_token);
}
// int encode(const std::string &path, int src_sample_rate,
//            AVSampleFormat src_sample_fmt, AVChannelLayout src_ch_layout,
//            Waveform waveform, int bitrate, std::atomic_bool &cancel_token,
//            ProgressCallback progress_callback) {
//   AVFormatContext *output_format_context = NULL;
//   AVCodecContext *output_codec_context = NULL;
//   SwrContext *resample_context = NULL;
//   AVAudioFifo *fifo = NULL;

//   FramesManager frame_manager(src_sample_fmt, &src_ch_layout,
//   src_sample_rate);

//   int data_written = 0;

//   int ret = AVERROR_EXIT;
//   bool canceled = false;
//   int64_t pts = 0;
//   auto last_progress_timestamp = get_current_timestamp();

//   try {
//     if (frame_manager.alloc(std::move(waveform))) {
//       return -1;
//     }
//     check_cancel_and_throw(cancel_token);
//     /* Open the output file for writing. */
//     if ((open_output_file(path.c_str(), src_sample_rate, src_sample_fmt,
//                           src_ch_layout.nb_channels, bitrate,
//                           &output_format_context, &output_codec_context)))
//       goto cleanup;
//     check_cancel_and_throw(cancel_token);

//     if (init_resampler(&src_ch_layout, src_sample_fmt, src_sample_rate,
//                        output_codec_context, &resample_context))
//       goto cleanup;
//     if (init_fifo(&fifo, output_codec_context))
//       goto cleanup;
//     check_cancel_and_throw(cancel_token);

//     /* Write the header of the output file container. */
//     if ((write_output_file_header(output_format_context)))
//       goto cleanup;
//     check_cancel_and_throw(cancel_token);
// #if SPLEETER_ENABLE_PROGRESS_CALLBACK
//     auto progress_fun = [&]() {
//       auto now_progress_timestamp = get_current_timestamp();
//       auto x = now_progress_timestamp - last_progress_timestamp;
//       if (x.count() > 500) {
//         progress_callback(
//             av_rescale(pts, 1000, output_codec_context->sample_rate));
//         last_progress_timestamp = now_progress_timestamp;
//       }
//     };
// #endif
//     while (1) {
//       const int output_frame_size = output_codec_context->frame_size;
//       int finished = 0;

//       while (av_audio_fifo_size(fifo) < output_frame_size) {
//         if (read_decode_convert_and_store(fifo, output_codec_context,
//                                           resample_context, &finished,
//                                           frame_manager))
//           goto cleanup;
//         check_cancel_and_throw(cancel_token);

//         if (finished)
//           break;
//       }

//       while (av_audio_fifo_size(fifo) >= output_frame_size ||
//              (finished && av_audio_fifo_size(fifo) > 0)) {
//         /* Take one frame worth of audio samples from the FIFO buffer,
//          * encode it and write it to the output file. */
//         if (load_encode_and_write(fifo, output_format_context,
//                                   output_codec_context, pts))
//           goto cleanup;
//         check_cancel_and_throw(cancel_token);
// #if SPLEETER_ENABLE_PROGRESS_CALLBACK
//         progress_fun();
// #endif
//       }

//       /* If we are at the end of the input file and have encoded
//        * all remaining samples, we can exit this loop and finish. */
//       if (finished) {
//         int data_written;
//         /* Flush the encoder as it may have delayed frames. */
//         do {
//           if (encode_audio_frame(NULL, output_format_context,
//                                  output_codec_context, &data_written, pts)) {
//             goto cleanup;
//             check_cancel_and_throw(cancel_token);
// #if SPLEETER_ENABLE_PROGRESS_CALLBACK
//             progress_fun();
// #endif
//           }
//         } while (data_written);
//         break;
//       }
//     }

//     if (write_output_file_trailer(output_format_context))
//       goto cleanup;
//     check_cancel_and_throw(cancel_token);

//     ret = 0;
//   } catch (const CancelException &) {
//     canceled = true;
//   }

// cleanup:
//   if (fifo)
//     av_audio_fifo_free(fifo);
//   swr_free(&resample_context);
//   if (output_codec_context)
//     avcodec_free_context(&output_codec_context);
//   if (output_format_context) {
//     avio_closep(&output_format_context->pb);
//     avformat_free_context(output_format_context);
//   }
//   // return ret;

//   if (canceled) {
//     return 0;
//   }
//   if (ret < 0) {
//     return ret;
//   }
//   return 1;
// }

FFmpegAudioEncoder::FFmpegAudioEncoder(std::string path, int src_sample_rate,
                                       AVSampleFormat src_sample_fmt,
                                       const AVChannelLayout &src_ch_layout,
                                       int bitrate,
                                       std::atomic_bool *cancel_token)
    : path_(path), src_sample_rate_(src_sample_rate),
      src_sample_fmt_(src_sample_fmt), bitrate_(bitrate),
      cancel_token_(cancel_token) {
  av_channel_layout_copy(&src_ch_layout_, &src_ch_layout);
  // frame_manager_ = std::make_unique<FramesManager>(
  //     src_sample_fmt, &src_ch_layout_, src_sample_rate);
}
std::unique_ptr<FFmpegAudioEncoder>
FFmpegAudioEncoder::create(std::string path, int src_sample_rate,
                           AVSampleFormat src_sample_fmt,
                           const AVChannelLayout &src_ch_layout, int bitrate,
                           std::atomic_bool *cancel_token) {
  auto encoder = std::make_unique<FFmpegAudioEncoder>(
      path, src_sample_rate, src_sample_fmt, src_ch_layout, bitrate,
      cancel_token);
  /* Open the output file for writing. */
  if ((open_output_file(path.c_str(), src_sample_rate, src_sample_fmt,
                        src_ch_layout.nb_channels, bitrate,
                        &encoder->output_format_context_,
                        &encoder->output_codec_context_)))
    return nullptr;

  if (init_resampler(&src_ch_layout, src_sample_fmt, src_sample_rate,
                     encoder->output_codec_context_,
                     &encoder->resample_context_))
    return nullptr;
  if (init_fifo(&encoder->fifo_, encoder->output_codec_context_))
    return nullptr;

  /* Write the header of the output file container. */
  if ((write_output_file_header(encoder->output_format_context_)))
    return nullptr;
  return encoder;
}

int FFmpegAudioEncoder::encode(Waveform waveform) {

  int ret = AVERROR_EXIT;
  bool canceled = false;
  int waveform_data_size = waveform.data.size();
  int waveform_data_nb_frames = waveform.nb_frames;
  try {
    // FramesManager &frame_manager = *frame_manager_;
    auto &cancel_token = *cancel_token_;
    auto &path = path_;
    auto &src_sample_rate = src_sample_rate_;
    auto &src_sample_fmt = src_sample_fmt_;
    auto &src_ch_layout = src_ch_layout_;
    auto &bitrate = bitrate_;

    AVFormatContext *&output_format_context = output_format_context_;
    AVCodecContext *&output_codec_context = output_codec_context_;
    SwrContext *&resample_context = resample_context_;
    AVAudioFifo *fifo = fifo_;
    check_cancel_and_throw(cancel_token);

    FramesManager frame_manager(src_sample_fmt, &src_ch_layout_,
                                src_sample_rate);

    if (frame_manager.alloc(std::move(waveform))) {
      return -1;
    }
    check_cancel_and_throw(cancel_token);

    // #if SPLEETER_ENABLE_PROGRESS_CALLBACK
    //     auto progress_fun = [&]() {
    //       auto now_progress_timestamp = get_current_timestamp();
    //       auto x = now_progress_timestamp - last_progress_timestamp;
    //       if (x.count() > 500) {
    //         progress_callback(
    //             av_rescale(pts, 1000, output_codec_context->sample_rate));
    //         last_progress_timestamp = now_progress_timestamp;
    //       }
    //     };
    // #endif
    while (1) {
      const int output_frame_size = output_codec_context->frame_size;
      int finished = 0;

      while (av_audio_fifo_size(fifo) < output_frame_size) {
        if (read_decode_convert_and_store(fifo, output_codec_context,
                                          resample_context, &finished,
                                          frame_manager))
          goto cleanup;
        check_cancel_and_throw(cancel_token);

        if (finished)
          break;
      }

      while (av_audio_fifo_size(fifo) >= output_frame_size) {
        /* Take one frame worth of audio samples from the FIFO buffer,
         * encode it and write it to the output file. */
        if (load_encode_and_write(fifo, output_format_context,
                                  output_codec_context, pts))
          goto cleanup;
        check_cancel_and_throw(cancel_token);
        // #if SPLEETER_ENABLE_PROGRESS_CALLBACK
        //         progress_fun();
        // #endif
      }

      // while (av_audio_fifo_size(fifo) >= output_frame_size ||
      //        (finished && av_audio_fifo_size(fifo) > 0)) {
      //   /* Take one frame worth of audio samples from the FIFO buffer,
      //    * encode it and write it to the output file. */
      //   if (load_encode_and_write(fifo, output_format_context,
      //                             output_codec_context, pts))
      //     goto cleanup;
      //   check_cancel_and_throw(cancel_token);
      //   // #if SPLEETER_ENABLE_PROGRESS_CALLBACK
      //   //         progress_fun();
      //   // #endif
      // }

      /* If we are at the end of the input file and have encoded
       * all remaining samples, we can exit this loop and finish. */
      if (finished) {

        break;
      }
    }

    check_cancel_and_throw(cancel_token);

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

int FFmpegAudioEncoder::finish() {
  auto &cancel_token = *cancel_token_;

  AVFormatContext *&output_format_context = output_format_context_;
  AVCodecContext *&output_codec_context = output_codec_context_;
  AVAudioFifo *fifo = fifo_;

  int ret = AVERROR_EXIT;
  int data_written;

  while (av_audio_fifo_size(fifo) > 0) {
    /* Take one frame worth of audio samples from the FIFO buffer,
     * encode it and write it to the output file. */
    if (load_encode_and_write(fifo, output_format_context, output_codec_context,
                              pts))
      goto cleanup;
    check_cancel_and_throw(cancel_token);
    // #if SPLEETER_ENABLE_PROGRESS_CALLBACK
    //         progress_fun();
    // #endif
  }

  /* Flush the encoder as it may have delayed frames. */
  do {
    if (encode_audio_frame(NULL, output_format_context, output_codec_context,
                           &data_written, pts)) {
      goto cleanup;
      check_cancel_and_throw(cancel_token);
      // #if SPLEETER_ENABLE_PROGRESS_CALLBACK
      //             progress_fun();
      // #endif
    }
  } while (data_written);

  if (write_output_file_trailer(output_format_context))
    goto cleanup;
  ret = 1;
cleanup:
  return ret;
}

FFmpegAudioEncoder::~FFmpegAudioEncoder() {
  if (fifo_)
    av_audio_fifo_free(fifo_);
  swr_free(&resample_context_);
  if (output_codec_context_)
    avcodec_free_context(&output_codec_context_);
  if (output_format_context_) {
    avio_closep(&output_format_context_->pb);
    avformat_free_context(output_format_context_);
  }
}

} // namespace codec
} // namespace spleeter