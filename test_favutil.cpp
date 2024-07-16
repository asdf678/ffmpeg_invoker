#include "favutil/waveform.h"
#include <iostream>
#include <string>
#include <string_view>
using namespace std;

int main(int argc, char **argv) {
  // char *argvA[4] = {"", "C:\\KwDownload\\song\\44100_32.mp3", "0",
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
    cout << "please input waveform per second" << endl;
    return 1;
  }

  string_view path = argv[1];
  int waveform_per_second = std::stoi(argv[2]);

  avpro::Waveform w;
  int ret = w.execute(path, waveform_per_second, 100);
  if (ret < 0) {
    cout << "error" << endl;
  }

  std::cout << "waveform size:" << w.get_audio_waveform().size() << std::endl;
  std::cout << "audio_duration:" << w.get_audio_duration() << std::endl;
  std::cout << "audio_duration_decoded:" << w.get_audio_duration_decoded()
            << std::endl;
  return 0;
}
