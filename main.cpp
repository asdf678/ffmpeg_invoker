#include "common.h"
#include "ffmpeg_audio_adapter.h"
#include "waveform.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>

#define ENABLE_SEGMENT 0

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
    auto decode_ret = spleeter::FfmpegAudioAdapter::Decode(
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
    auto adapter = std::make_unique<spleeter::FfmpegAudioAdapter>(
        output_flename, &cancel_token);

    /// 按10s进行分割
    constexpr std::size_t segment_nb_samples =
        spleeter::constants::kSampleRate * 2;
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
    while (!segments.empty()) {
      spleeter::Waveform curr = segments.front();

      encode_ret = adapter->Encode(curr, [](auto &&t) {
        std::cout << "encode pos:" << t << "ms" << endl;
      });
      segments.pop();

      if (encode_ret <= 0) {
        break;
      }
    }

    if (encode_ret > 0) {
      encode_ret = adapter->FinishEncode();
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
