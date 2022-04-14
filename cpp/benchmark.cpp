/*
 * g++ -Ofast -std=c++14 -DDISABLE_MOVING_AVERAGE -o benchmark benchmark.cpp
 */
#include <chrono>
#include <iostream>
#include <map>
#include <stdlib.h>

#include "pianolizer.hpp"

using namespace std;

// the floating number arithmetics produce different results on different architectures; account for that
#define ABS_ERROR 1e-4

int main(int argc, char *argv[]) {
  // changing sampleRate affects the memory allocation
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
  cout << "benchmark: " << static_cast<int>(round(i / elapsed.count()))
    << " samples per second" << endl;

  // making a *good* oscillator is hard...
  if (sampleRate == 44100) {
    unsigned errors = 0;
    map<int,float> test = {
      { 21, .000041 },
      { 33, .605242 },
      { 45, .152685 },
      { 52, .069327 },
      { 57, .036673 }
    };
    for (auto kv : test) {
      if (std::fabs(output[kv.first] - test[kv.first]) > ABS_ERROR) {
        errors++;
        cerr << "output for key #" << to_string(kv.first)
          << " is " << to_string(output[kv.first])
          << "; expected " << to_string(kv.second)
          << endl;
      }
    }
    if (errors)
      return EXIT_FAILURE;      
  }

  return EXIT_SUCCESS;
}
