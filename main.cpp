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
  // char *argvA[4] = {"", "C:\\KwDownload\\song\\44100_32.mp3", "./test2.mp3",
  //                   "-1"};
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

    spleeter::AudioDecoder decoder(path, &cancel_token);
    if (!decoder) {
      cout << "decoder create failed";
      return -1;
    }
    spleeter::AudioEncoder encoder(output_flename, &cancel_token);
    if (!encoder) {
      cout << "encoder create failed";
      return -1;
    }
    while (1) {
      unique_ptr<spleeter::Waveform> waveform;
      int decode_ret = decoder.Decode(waveform, segment_nb_samples);

      if (decode_ret == 0) {
        cout << "decode failed(canceled):" << path << endl;
        return 1;
      } else if (decode_ret < 0) {
        cout << "decode failed(error):" << path << endl;
        return 1;
      }
      if (!waveform) {
        cout << "decode finished:" << path << endl;
      } else {
        cout << "decode success,nb_samples:"
             << (waveform ? waveform->nb_frames : 0) << endl;
      }

      if (waveform) {
        int encode_ret = encoder.Encode(*waveform);
        if (encode_ret == 0) {
          cout << "encode failed(canceled):" << endl;
          return 1;
        } else if (encode_ret < 0) {
          cout << "encode failed(error):" << endl;
          return 1;
        } else {
          cout << "encode success" << endl;
        }

      } else {
        encoder.FinishEncode();
        cout << "encode complete:" << output_flename << endl;
        break;
      }
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
