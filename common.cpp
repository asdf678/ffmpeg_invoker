
#include "common.h"
#include "waveform.h"
#include <cassert>
namespace spleeter {

std::chrono::milliseconds get_current_timestamp() {
  auto now = std::chrono::system_clock::now();
  auto timestamp = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
  return timestamp.time_since_epoch();
}

/// 如果是集合中最后一个元素，它是不需要处理边界的
std::queue<spleeter::Waveform> segment_audio(const spleeter::Waveform &waveform,
                                             std::size_t segment_nb_samples,
                                             std::size_t boundary_nb_samples) {
  assert(segment_nb_samples > boundary_nb_samples);
  /// 切记：boundary_nb_samples必须小于segment_nb_samples
  // if (waveform.nb_frames <= segment_nb_samples + boundary_nb_samples) {
  //   /// 不需要处理临界点
  //   return std::queue<spleeter::Waveform>({waveform});
  // }
  std::queue<spleeter::Waveform> result;
  /// cursor为segment+boundary_nb_samples
  std::size_t cursor = 0;
  do {
    std::size_t start, end;
    /// 因为使用了size_t，所以无法判断负数，第一个元素需要判断cursor ==
    /// 0，而不是cursor-boundary_nb_samples<0
    if (cursor == 0) {
      start = 0;
      end = segment_nb_samples + boundary_nb_samples;
    } else {
      /// 由于cursor为end，所以start需要减去2倍的boundary_nb_samples
      start = cursor - 2 * boundary_nb_samples;
      end = cursor + segment_nb_samples;
    }
    if (end > waveform.nb_frames) {
      end = waveform.nb_frames;
    }
    cursor = end;
    result.push(waveform.sub_frames(start, end));
  } while (cursor < waveform.nb_frames);
  return result;
}

spleeter::Waveform restore_segment_audio(const Waveform &waveform,
                                         std::size_t boundary_nb_samples,

                                         bool head, bool tail) {
  std::size_t start = 0;
  std::size_t end = waveform.nb_frames;

  if (head) {
    start += boundary_nb_samples;
  }
  if (end) {
    end -= boundary_nb_samples;
  }
  return waveform.sub_frames(start, end);
}

} // namespace spleeter