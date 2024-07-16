#include "waveform.h"
#include "common.h"
#include <assert.h>
#include <list>

inline int64_t compute_duration(AVFormatContext *ic) {
  return ic->duration == AV_NOPTS_VALUE
             ? -1
             : av_rescale(ic->duration, 1000, AV_TIME_BASE);
}

int avpro::Waveform::execute(std::string_view url, int waveform_per_second,
                             double max_waveform_height) {
  CommonMedia media;
  int ret;
  ret = media.open_input(url);
  if (ret < 0) {
    return -1;
  }
  ret = media.open_audio_stream();
  if (ret < 0) {
    return -1;
  }
  ret = media.open_audio_codec();
  if (ret < 0) {
    return -1;
  }
  if (waveform_per_second > 0) {
    ret = media.init_audio_filters(
        "aformat=sample_fmts=s16:channel_layouts=mono");
  } else {
    ret = media.init_audio_filters("anull");
  }

  if (ret < 0) {
    return -1;
  }
  // int64_t duration = ;
  int sample_rate = media.get_media_context().dec_ctx->sample_rate;
  std::vector<double> result;
  int64_t pad = 0;
  if (waveform_per_second > 0) {
    const int samples_per_waveform = sample_rate / waveform_per_second;
    assert(samples_per_waveform > 0);
    int next_samples = samples_per_waveform;
    int values = 0;
    std::list<int> data;
    int max_value = 0;
    ret = media.decode_audio(
        [&](const AVFrame *f, const CommonFilterContext *filter_context_ptr) {
          return filter_context_ptr->filter(f, [&](const AVFrame *frame) {
            const int n = frame->nb_samples * frame->ch_layout.nb_channels;
            const int16_t *p = (int16_t *)frame->data[0];
            const int16_t *p_end = p + n;

            while (p < p_end) {
              values += static_cast<int>(std::abs(*p));
              --next_samples;
              if (next_samples == 0) {
                int value = values / samples_per_waveform;
                if (value > max_value) {
                  max_value = value;
                }
                data.push_back(value);
                values = 0;
                next_samples = samples_per_waveform;
              }
              p++;
            }
          });
        });
    if (ret < 0) {
      if (data.empty()) {
        return -1;
      }
      ret = 0;
    } else {
      ret = 1;
    }
    double scale = max_waveform_height / max_value;
    std::for_each(data.cbegin(), data.cend(),
                  [&](auto &&v) { result.push_back(v * scale); });
    assert(next_samples <= samples_per_waveform);
    pad = av_rescale(samples_per_waveform - next_samples, 1000, sample_rate);
  } else {
    ret = media.decode_audio(
        [&](const AVFrame *f, const CommonFilterContext *filter_context_ptr) {
          return 1;
        });

    if (ret < 0) {
      if (media.get_decoded_duration() < 0) {
        return -1;
      }
      ret = 0;
    } else {
      ret = 1;
    }
    pad = 0;
  }

  int64_t duration = compute_duration(&media.get_format_context());
  this->audio_duration = duration;
  this->waveform = std::move(result);
  this->audio_pad = pad;
  this->audio_duration_decoded = media.get_decoded_duration();
  this->sample_rate = media.get_sample_rate();
  this->sample_fmt = media.get_sample_fmt();
  this->channel_layout = media.get_channel_layout();
  return ret;

  // return media.execute(url,
  // "aresample=8000,aformat=sample_fmts=s16:channel_layouts=mono",
  //                      [](const AVFrame *frame) {
  //                          const int n = frame->nb_samples *
  //                          frame->ch_layout.nb_channels; const uint16_t *p =
  //                          (uint16_t *) frame->data[0]; const uint16_t *p_end
  //                          = p + n;

  //                          while (p < p_end) {
  //                              fputc(*p & 0xff, stdout);
  //                              fputc(*p >> 8 & 0xff, stdout);
  //                              p++;
  //                          }
  //                          fflush(stdout);
  //                      });
}