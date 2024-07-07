#include "ffmpeg_audio_adapter.h"
#include "waveform.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
using namespace std;
int main(int argc, char **argv) {
  char *argvA[4] = {"", "/Users/tzy/Downloads/2.m4a", "./test2.mp3", "-1"};
  if (argc != 4) {
    argc = 4;
    argv = argvA;
  }

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
  unique_ptr<spleeter::FfmpegAudioAdapter> adapter =
      make_unique<spleeter::FfmpegAudioAdapter>();
  thread t([=, &adapter]() {
    unique_ptr<spleeter::Waveform> waveform;
    auto decode_ret = adapter->Decode(path, -1, -1, waveform, [](auto &&t) {
      std::cout << "decode pos:" << t << "ms" << endl;
    });
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
    auto encode_ret =
        adapter->Encode(std::move(*waveform), output_flename, [](auto &&t) {
          std::cout << "encode pos:" << t << "ms" << endl;
        });
    /// 由于被move了，对象不可用，所以在这里reset null
    waveform.reset();
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
    adapter->Cancel();
  }

  t.join();

  return 0;
}
