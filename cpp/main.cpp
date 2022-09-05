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
  cout << "\t-b\tbuffer size; default: 256 (samples)" << endl;
  cout << "\t-c\tnumber of channels; default: 1" << endl;
  cout << "\t-s\tsample rate; default: 44100 (Hz)" << endl;
  cout << "\t-p\tA4 reference frequency; default: 440 (Hz)" << endl;
  cout << "\t-k\tnumber of keys on the piano keyboard; default: 61" << endl;
  cout << "\t-r\treference key index (A4); default: 33" << endl;
  cout << "\t-a\taverage window (effectively a low-pass filter for the output); default: 0.04 (seconds; 0 to disable)" << endl;
  cout << "\t-t\tnoise gate threshold, from 0 to 1; default: 0" << endl;
  cout << endl;
  cout << "Description:" << endl;
  cout << "Consumes an audio stream (1 channel, 32-bit float PCM)" << endl;
  cout << "and emits the volume levels of 61 notes (from C2 to C7) as a hex string." << endl;
  exit(EXIT_SUCCESS);
}

// C++17's <algorithm> header has this already
double clamp(double d, double min, double max);
double clamp(double d, double min, double max) {
  const double t = d < min ? min : d;
  return t > max ? max : t;
}

int main(int argc, char *argv[]) {
  size_t samples = 256; // known to work on RPi3b
  size_t channels = 1;
  int sampleRate = 44100;
  float pitchFork = 440.;
  float averageWindow = 0.04;
  int keys = 61;
  int refKey = 33;
  float threshold = 0.;

  for (;;) {
    switch (getopt(argc, argv, "b:c:s:p:k:r:a:t:h")) {
      case -1:
        break;
      case 'b':
        if (optarg) samples = static_cast<size_t>(atoi(optarg));
        continue;
      case 'c':
        if (optarg) channels = static_cast<size_t>(atoi(optarg));
        continue;
      case 's':
        if (optarg) sampleRate = atoi(optarg);
        continue;
      case 'p':
        if (optarg) pitchFork = atof(optarg);
        continue;
      case 'k':
        if (optarg) keys = atoi(optarg);
        continue;
      case 'r':
        if (optarg) refKey = atoi(optarg);
        continue;
      case 'a':
        if (optarg) averageWindow = atof(optarg);
        continue;
      case 't':
        if (optarg) threshold = atof(optarg);
        continue;
      case 'h':
      default:
        help();
    }
    break;
  }

  if (sampleRate < 8000 || sampleRate > 200000) {
    cerr << "sampleRate must be between 8000 and 200000 Hz" << endl;
    return EXIT_FAILURE;
  }

  auto tuning = make_shared<PianoTuning>(sampleRate, pitchFork, keys, refKey);
  auto sdft = SlidingDFT(tuning, -1.);

  try {
    freopen(nullptr, "rb", stdin);
    if (ferror(stdin))
      throw runtime_error(strerror(errno));

    size_t len;
    size_t bufferSize = samples * channels;
    auto buffer = make_unique<float[]>(bufferSize);
    auto input = make_unique<float[]>(samples);
    const float *output = nullptr;

    while ((len = fread(buffer.get(), sizeof(buffer[0]), bufferSize, stdin)) > 0) {
      if (ferror(stdin) && !feof(stdin))
        throw runtime_error(strerror(errno));

      memset(input.get(), 0, sizeof(input[0]) * samples);
      for (unsigned i = 0; i < len; i++)
        input[i / channels] += buffer[i];

      if ((output = sdft.process(input.get(), samples, averageWindow)) == nullptr)
        throw runtime_error("sdft.process() returned nothing");

      stringstream stream;
      for (unsigned i = 0; i < sdft.bands; i++) {
        const float value = output[i] > threshold ? output[i] : 0.;
        unsigned valueInt = static_cast<unsigned>(std::round(255. * clamp(value, 0., 1.)));
        stream << setfill('0') << setw(2) << hex << valueInt;
      }
      cout << stream.str() << endl;
    }
  } catch (exception const& e) {
    cerr << e.what() << endl;
  }

  return EXIT_SUCCESS;
}
