#include "ffmpeg_audio_adapter.h"
#include "waveform.h"
#include <iostream>
#include <memory>
#include <string>
using namespace std;
int main(int argc, char **argv) {
  char *argvA[3] = {"", "/Users/tzy/Downloads/2.m4a", "./test2.mp3"};
  if (argc != 3) {
    argc = 3;
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

  string path = argv[1];
  string output_flename = argv[2];
  cout << "load path:" << path << endl;
  unique_ptr<spleeter::FfmpegAudioAdapter> adapter =
      make_unique<spleeter::FfmpegAudioAdapter>();
  auto waveform = adapter->Load(path, -1, -1);
  if (!waveform) {
    cout << "load failed:" << path << endl;
    return 1;
  }
  cout << "result nb_channels:" << waveform->nb_channels << endl;
  cout << "result nb_frames:" << waveform->nb_frames << endl;

  if (adapter->Save(std::move(*waveform.release()), output_flename) >= 0) {
    cout << "save " << output_flename << " success" << endl;
  } else {
    cout << "save " << output_flename << " failed" << endl;
  }

  return 0;
}
