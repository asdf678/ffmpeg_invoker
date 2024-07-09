///
/// @file
/// @brief Contains Data Structure to hold Waveform (i.e. audio samples)
/// @copyright Copyright (c) 2020, MIT License
///
#ifndef SPLEETER_DATATYPES_WAVEFORM_H
#define SPLEETER_DATATYPES_WAVEFORM_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

namespace spleeter {
struct Waveform {
  std::size_t nb_frames;
  std::int32_t nb_channels;
  std::vector<float> data;

  Waveform sub_frames(std::size_t start, std::size_t end) const {
    std::size_t count = end - start;
    return Waveform{.nb_frames = count,
                    .nb_channels = nb_channels,
                    .data =
                        std::vector<float>(data.cbegin() + start * nb_channels,
                                           data.cbegin() + end * nb_channels)};
  }
};

/// @brief List of waveforms
using Waveforms = std::vector<Waveform>;

/// @brief Provide output stream for waveform (list of samples), prints number
/// of samples it holds.
inline std::ostream &operator<<(std::ostream &out, const Waveform &waveform) {
  out << "Waveform{nb_frames: " << waveform.nb_frames
      << ", nb_channels: " << waveform.nb_channels
      << ", nb_size: " << waveform.data.size() << "}";
  return out;
}

/// @brief Provide output stream for waveforms (list of waveform), prints number
/// of sample in each waveform
inline std::ostream &operator<<(std::ostream &out, const Waveforms &waveforms) {
  std::int32_t idx{0};
  out << "Waveforms{\n";
  std::for_each(waveforms.begin(), waveforms.end(), [&](const auto &waveform) {
    out << "  " << idx++ << ". " << waveform << "\n";
  });
  out << "}";
  return out;
}
} // namespace spleeter
#endif /// SPLEETER_DATATYPES_WAVEFORM_H
