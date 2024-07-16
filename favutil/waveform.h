#ifndef AVPRO_WAVEFORM_H
#define AVPRO_WAVEFORM_H

#include <string>
#include <string_view>
#include <vector>

namespace avpro {
class Waveform {
  int64_t audio_duration{-1};
  int64_t audio_pad{0};
  std::vector<double> waveform{};
  int64_t audio_duration_decoded{-1};
  int sample_rate{-1};
  std::string sample_fmt{};
  std::string channel_layout{};
  int decoded_duration{-1};

public:
  int64_t get_audio_duration() { return audio_duration; }

  int64_t get_audio_pad() { return audio_pad; }

  std::vector<double> &get_audio_waveform() { return waveform; }

  int64_t get_audio_duration_decoded() { return audio_duration_decoded; }

  int get_sample_rate() { return sample_rate; }

  std::string get_sample_fmt() { return sample_fmt; }

  std::string get_channel_layout() { return channel_layout; }

  int execute(std::string_view url, int waveform_per_second,
              double max_waveform_height);
};
} // namespace avpro
#endif