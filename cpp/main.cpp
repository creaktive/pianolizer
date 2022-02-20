#include <climits>
#include <cstring>
#include <iostream>
#include <getopt.h>
#include <stdio.h>

#include "pianolizer.hpp"

using namespace std;

void help();
void help() {
  cout << "Usage:" << endl;
  cout << "\tsox -d -traw -b32 -c1 -efloat - | ./pianolizer [options]" << endl;
  cout << endl;
  cout << "Options:" << endl;
  cout << "\t-h\tthis" << endl;
  cout << "\t-b\tbuffer size; default: 735 (1/60th of a second for 44.1kHz sample rate)" << endl;
  cout << "\t-s\tsample rate; default: 44100 (Hz)" << endl;
  cout << "\t-p\tA4 reference frequency; default: 440 (Hz)" << endl;
  cout << "\t-a\taverage window (effectively a low-pass filter for the output); default: 0.050 (seconds; 0 to disable)" << endl;
  cout << endl;
  cout << "Description:" << endl;
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  size_t bufferSize = 735; // aim for 60 fps
  int sampleRate = 44100;
  float pitchFork = 440.;
  float averageWindow = 0.050;

  for (;;) {
    switch (getopt(argc, argv, "b:s:p:a:h")) {
      case -1:
        break;
      case 'b':
        if (optarg) bufferSize = static_cast<size_t>(atoi(optarg));
        continue;
      case 's':
        if (optarg) sampleRate = atoi(optarg);
        continue;
      case 'p':
        if (optarg) pitchFork = atof(optarg);
        continue;
      case 'a':
        if (optarg) averageWindow = atof(optarg);
        continue;
      case 'h':
      default:
        help();
    }
    break;
  }

  /*
  cerr << "bufferSize=" << bufferSize << endl;
  cerr << "sampleRate=" << sampleRate << endl;
  cerr << "pitchFork=" << pitchFork << endl;
  cerr << "averageWindow=" << averageWindow << endl;
  */

  auto tuning = make_shared<PianoTuning>(sampleRate, pitchFork);
  auto sdft = SlidingDFT(tuning, -1.);

  try {
    freopen(nullptr, "rb", stdin);
    if (ferror(stdin))
      throw runtime_error(strerror(errno));

    size_t len;
    auto input = make_unique<float[]>(bufferSize);
    const float *output = nullptr;

    while ((len = fread(input.get(), sizeof(input[0]), bufferSize, stdin)) > 0) {
      if (ferror(stdin) && !feof(stdin))
        throw runtime_error(strerror(errno));

      output = sdft.process(input.get(), len, averageWindow);
      for (unsigned i = 0; i < tuning->bands; i++)
        printf("%02x", static_cast<unsigned char>(round(output[i] * 255.)));
      cout << endl;
    }
  } catch (exception const& e) {
    cerr << e.what() << endl;
  }

  return EXIT_SUCCESS;
}
