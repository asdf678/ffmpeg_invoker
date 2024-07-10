
#ifndef SPLEETER_FFMPEG_AUDIO_COMMON_H
#define SPLEETER_FFMPEG_AUDIO_COMMON_H

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/packet.h"
#include "libavutil/audio_fifo.h"
#include "libavutil/avassert.h"
#include "libavutil/error.h"
#include "libavutil/frame.h"
#include "libswresample/swresample.h"
}

#include <cerrno>
#include <cstdio>
namespace spleeter {
namespace codec {
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
                          const AVChannelLayout *out_ch_layout,
                          enum AVSampleFormat out_sample_fmt,
                          int out_sample_rate, SwrContext **resample_context) {
  int error;

  /*
   * Create a resampler context for the conversion.
   * Set the conversion parameters.
   */
  error = swr_alloc_set_opts2(resample_context, out_ch_layout, out_sample_fmt,
                              out_sample_rate, in_ch_layout, in_sample_fmt,
                              in_sample_rate, 0, NULL);
  if (error < 0) {
    fprintf(stderr, "Could not allocate resample context\n");
    return error;
  }
  /*
   * Perform a sanity check so that the number of converted samples is
   * not greater than the number of samples to be converted.
   * If the sample rates differ, this case has to be handled differently
   */
  //   av_assert0(output_codec_context->sample_rate == in_sample_rate);

  /* Open the resampler with the specified parameters. */
  if ((error = swr_init(*resample_context)) < 0) {
    fprintf(stderr, "Could not open resample context\n");
    swr_free(resample_context);
    return error;
  }
  return 0;
}

static int init_fifo(AVAudioFifo **fifo, enum AVSampleFormat sample_fmt,
                     int channels, int nb_samples) {
  /* Create the FIFO buffer based on the specified output sample format. */
  if (!(*fifo = av_audio_fifo_alloc(sample_fmt, channels, nb_samples))) {
    fprintf(stderr, "Could not allocate FIFO\n");
    return AVERROR(ENOMEM);
  }
  return 0;
}

static int init_converted_samples(uint8_t ***converted_input_samples,
                                  int nb_channels, int nb_samples,
                                  enum AVSampleFormat sample_fmt) {
  int error;

  /* Allocate as many pointers as there are audio channels.
   * Each pointer will later point to the audio samples of the corresponding
   * channels (although it may be NULL for interleaved formats).
   */

  if (!(*converted_input_samples = new uint8_t *[nb_channels])) {
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
  if ((error = av_samples_alloc(*converted_input_samples, NULL, nb_channels,
                                nb_samples, sample_fmt, 0)) < 0) {
    fprintf(stderr, "Could not allocate converted input samples (error '%s')\n",
            av_err2str(error));
    av_freep(&(*converted_input_samples)[0]);
    // free(*converted_input_samples);
    delete[] *converted_input_samples;
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

} // namespace codec
} // namespace spleeter

#endif