/*
 * g++ -Ofast -std=c++14 -DDISABLE_MOVING_AVERAGE -o benchmark benchmark.cpp
 */
#include <chrono>
#include <iostream>
#include <stdlib.h>

#include "pianolizer.hpp"

using namespace std;

int main(int argc, char *argv[]) {
  unsigned sampleRate = 44100;
  if (argc == 2) {
    sampleRate = atoi(argv[1]);
    if (sampleRate < 8000 || sampleRate > 200000) {
      cerr << "sampleRate must be between 8000 and 200000 Hz" << endl;
      return EXIT_FAILURE;
    }
  }
  cerr << "sampleRate: " << sampleRate << endl;

  auto sdft = SlidingDFT(make_shared<PianoTuning>(sampleRate));
  const unsigned bufferSize = 128;
  number input[bufferSize];
  const number *output = nullptr;

  auto start = chrono::high_resolution_clock::now();
  unsigned i;
  for (i = 0; i < bufferSize * 10000; i++) {
    unsigned j = i % bufferSize;
    input[j] = ((i % 100) / 50.) - 1.; // sawtooth wave, 441Hz
    if (j == bufferSize - 1)
      output = sdft.process(input, bufferSize);
  }
  auto end = chrono::high_resolution_clock::now();
  chrono::duration<number> elapsed = end - start;
  cerr << "benchmark: " << static_cast<int>(round(i / elapsed.count())) << " samples per second" << endl;

  return EXIT_SUCCESS;
}
