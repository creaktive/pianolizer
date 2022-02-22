#include <climits>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <getopt.h>
#include <stdio.h>

#include "pianolizer.hpp"

using namespace std;

void help();
void help() {
  cout << "Usage:" << endl;
  cout << "\tsox -V -d -traw -r44100 -b32 -c1 -efloat - | ./pianolizer | sudo misc/hex2ws281x.py" << endl;
  cout << endl;
  cout << "Options:" << endl;
  cout << "\t-h\tthis" << endl;
  cout << "\t-b\tbuffer size; default: 256" << endl;
  cout << "\t-s\tsample rate; default: 44100 (Hz)" << endl;
  cout << "\t-p\tA4 reference frequency; default: 440 (Hz)" << endl;
  cout << "\t-a\taverage window (effectively a low-pass filter for the output); default: 0.1 (seconds; 0 to disable)" << endl;
  cout << endl;
  cout << "Description:" << endl;
  exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {
  size_t bufferSize = 256; // known to work on RPi3b
  int sampleRate = 44100;
  float pitchFork = 440.;
  float averageWindow = 0.1;

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

      if ((output = sdft.process(input.get(), len, averageWindow)) == nullptr)
        throw runtime_error("sdft.process() returned nothing");

      stringstream stream;
      for (unsigned i = 0; i < tuning->bands; i++)
        stream << setfill('0') << setw(2) << hex
          << static_cast<unsigned>(round(output[i] * 255.));
      cout << stream.str() << endl;
    }
  } catch (exception const& e) {
    cerr << e.what() << endl;
  }

  return EXIT_SUCCESS;
}
