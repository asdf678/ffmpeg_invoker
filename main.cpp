#include "common.h"
#include "ffmpeg_audio_codec.h"
#include "waveform.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>

#define ENABLE_SEGMENT 1

#if ENABLE_SEGMENT
#define ENABLE_SEGMENT_RESTORE 1
#endif

using namespace std;
int main(int argc, char **argv) {
  // char *argvA[4] = {"", "/Users/tzy/Downloads/2.m4a", "./test2.mp3", "-1"};
  // if (argc != 4) {
  //   argc = 4;
  //   argv = argvA;
  // }

  if (argc <= 1) {
    cout << "please input path" << endl;
    return 1;
  }
  if (argc <= 2) {
    cout << "please input output filename" << endl;
    return 1;
  }

  if (argc <= 3) {
    cout << "please input task alive time" << endl;
    return 1;
  }

  string path = argv[1];
  string output_flename = argv[2];
  int alive_time = std::stoi(argv[3]);
  cout << "load path:" << path << endl;
  std::atomic_bool cancel_token{false};

  thread t([=, &cancel_token]() {
    unique_ptr<spleeter::Waveform> waveform;
    auto decode_ret = spleeter::FfmpegAudioCodec::Decode(
        path, -1, -1, waveform,
        [](auto &&t) { std::cout << "decode pos:" << t << "ms" << endl; },
        &cancel_token);
    if (decode_ret == 0) {
      cout << "load failed(canceled):" << path << endl;
      return 1;
    } else if (decode_ret < 0) {
      cout << "load failed(error):" << path << endl;
      return 1;
    } else {
      cout << "load success:" << path << endl;
    }
    assert(waveform);
    cout << "result nb_channels:" << waveform->nb_channels << endl;
    cout << "result nb_frames:" << waveform->nb_frames << endl;
    auto codec = std::make_unique<spleeter::FfmpegAudioCodec>(
        output_flename, &cancel_token);

    /// 按10s进行分割
    constexpr std::size_t segment_nb_samples =
        spleeter::constants::kSampleRate * 10;
/// 需要多0.5s进行临界点处理
#if ENABLE_SEGMENT
    constexpr std::size_t boundary_nb_samples =
        spleeter::constants::kSampleRate * 1;
#else
    constexpr std::size_t boundary_nb_samples =
        spleeter::constants::kSampleRate * 0;
#endif

    std::queue<spleeter::Waveform> segments = spleeter::segment_audio(
        *waveform, segment_nb_samples, boundary_nb_samples);
    /// 释放内存
    waveform.reset();

    int encode_ret = -1;
    std::size_t segment_index = 0, segment_size = segments.size();
    while (!segments.empty()) {
      bool head = true, tail = true;
      if (segment_index == 0) {
        head = false;
      }
      if (segment_index == segment_size - 1) {
        tail = false;
      }
#if ENABLE_SEGMENT_RESTORE
      spleeter::Waveform curr = spleeter::restore_segment_audio(
          segments.front(), boundary_nb_samples, head, tail);
#else
      spleeter::Waveform curr = segments.front();
#endif

      encode_ret = codec->Encode(curr, [](auto &&t) {
        std::cout << "encode pos:" << t << "ms" << endl;
      });
      segments.pop();
      ++segment_index;

      if (encode_ret <= 0) {
        break;
      }
    }

    if (encode_ret > 0) {
      encode_ret = codec->FinishEncode();
    }
    if (encode_ret == 0) {
      cout << "save failed(canceled):" << output_flename << endl;
      return 1;
    } else if (encode_ret < 0) {
      cout << "save failed(error):" << output_flename << endl;
      return 1;
    } else {
      cout << "save success:" << output_flename << endl;
    }

    return 0;
  });

  if (alive_time > 0) {
    this_thread::sleep_for(std::chrono::milliseconds(alive_time));
    // adapter->Cancel();
    cancel_token.store(true);
  }

  t.join();

  return 0;
}
