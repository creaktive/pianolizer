/*
 * g++ -Ofast -std=c++14 -DDISABLE_MOVING_AVERAGE -o benchmark benchmark.cpp
 */
#include <chrono>
#include <iostream>
#include <stdlib.h>

#include "pianolizer.hpp"

using namespace std;

int main() {
  const unsigned SAMPLE_RATE = 44100;
  auto sdft = SlidingDFT(make_shared<PianoTuning>(SAMPLE_RATE));
  const unsigned bufferSize = 128;
  float input[bufferSize];
  const float *output = nullptr;

  auto start = chrono::high_resolution_clock::now();
  unsigned i;
  for (i = 0; i < bufferSize * 10000; i++) {
    unsigned j = i % bufferSize;
    input[j] = ((i % 100) / 50.) - 1.; // sawtooth wave, 441Hz
    if (j == bufferSize - 1)
      output = sdft.process(input, bufferSize);
  }
  auto end = chrono::high_resolution_clock::now();
  chrono::duration<double> elapsed = end - start;
  cerr << "# benchmark: " << static_cast<int>(round(i / elapsed.count())) << " samples per second" << endl;
}
