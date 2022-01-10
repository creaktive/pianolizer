#include <chrono>
#include <iostream>
#include <map>
#include <stdlib.h>

#include <gtest/gtest.h>
#include "pianolizer.hpp"

using namespace std;

#define ABS_ERROR 1e-7

TEST(RingBuffer, Tiny) {
  auto rb = RingBuffer(15);

  EXPECT_EQ(rb.read(0), 0) << "initalized to zeroes";

  // write one single element
  rb.write(1);
  EXPECT_EQ(rb.read(0), 1) << "insertion succeeded";
  EXPECT_EQ(rb.read(1), 0) << "boundary shifted";

  // write 9 more elements
  for (unsigned i = 2; i < 10; i++)
    rb.write(i);

  // read in sequence
  for (unsigned i = 0; i < 10; i++)
    EXPECT_EQ(rb.read(9 - i), i) << "sequence value matches";

  // overflow
  for (unsigned i = 10; i < 20; i++)
    rb.write(i);

  // both the head and the tail are expected values
  EXPECT_EQ(rb.read(0), 19) << "head as expected";
  EXPECT_EQ(rb.read(15), 4) << "tail as expected";

  // reading beyond the capacity of ring buffer wraps it around
  EXPECT_EQ(rb.read(16), 19) << "wrap back to 0";
  EXPECT_EQ(rb.read(17), 18) << "wrap back to 1";
}

const unsigned SAMPLE_RATE = 44100;

const unsigned SINE = 0;
const unsigned SAWTOOTH = 1;
const unsigned SQUARE = 2;
const unsigned NOISE = 3;

// 441Hz wave period is 100 samples when the sample rate is 44100Hz
float oscillator(unsigned s, unsigned type);
float oscillator(unsigned s, unsigned type = SINE) {
  switch (type) {
    case SINE:
      return sin(M_PI / 50. * s);
    case SAWTOOTH:
      return ((s % 100) / 50.) - 1.;
    case SQUARE:
      return ((s % 100) < 50) ? 1. : -1.;
    case NOISE:
      return 2. * (rand() / RAND_MAX) - 1.;
    default:
      throw invalid_argument("unknown oscillator type " + to_string(type));
  }
  return 0.;
}

void testDFT(unsigned type, double expNAS, double expRMS, double expLog);
void testDFT(unsigned type, double expNAS, double expRMS, double expLog) {
  const unsigned N = 1700;
  auto bin = DFTBin(17, N);
  auto rb = RingBuffer(N);
  for (unsigned i = 0; i < 2000; i++) {
    const double currentSample = oscillator(i, type);
    rb.write(currentSample);
    const double previousSample = rb.read(N);
    bin.update(previousSample, currentSample);
  }

  const string prefix = "oscillator #" + to_string(type) + "; ";
  EXPECT_NEAR(expNAS, bin.normalizedAmplitudeSpectrum(), ABS_ERROR) << prefix + "normalized amplitude spectrum";
  EXPECT_NEAR(expRMS, bin.rms(), ABS_ERROR) << prefix + "RMS";
  EXPECT_NEAR(expLog, bin.logarithmicUnitDecibels(), ABS_ERROR) << prefix + "log dB";
}

TEST(DFTBin, Oscillators) {
  // reference values from the JS implementation
  testDFT(SINE, .9999999999999972, .7071067809649849, -3.0102999593614452);
  testDFT(SAWTOOTH, .7797470999951004, .5774080013883754, -6.93126867978036);
  testDFT(SQUARE, .9004644293093976, 1., -0.9106687653789797);
}

TEST(MovingAverage, FastAndHeavy) {
  auto fma = make_unique<FastMovingAverage>(2, SAMPLE_RATE);
  fma->averageWindowInSeconds(0.01);

  auto hma = make_unique<HeavyMovingAverage>(2, SAMPLE_RATE, 500);
  hma->averageWindowInSeconds(0.01);

  for (unsigned i = 0; i < 500; i++) {
    vector<float> sample = { oscillator(i, SINE), oscillator(i, SAWTOOTH) };
    fma->update(sample);
    hma->update(sample);
  }

  EXPECT_NEAR(fma->read(0), -.024506002326671227, ABS_ERROR) << "sine fast average";
  EXPECT_NEAR(fma->read(1), .01886483060529713, ABS_ERROR) << "sawtooth fast average";

  EXPECT_NEAR(hma->read(0), -.06714661267338967, ABS_ERROR) << "sine heavy average";
  EXPECT_NEAR(hma->read(1), .04485260926676986, ABS_ERROR) << "sawtooth heavy average";
}

TEST(PianoTuning, DFTValues) {
  auto pt = PianoTuning(SAMPLE_RATE);
  auto m = pt.mapping();

  EXPECT_EQ(m.size(), static_cast<unsigned>(61)) << "mapping size";

  EXPECT_EQ(static_cast<int>(m[0].k), 17) << "C2 k";
  EXPECT_EQ(static_cast<int>(m[0].N), 11462) << "C2 N";

  EXPECT_EQ(static_cast<int>(m[33].k), 17) << "A4 k";
  EXPECT_EQ(static_cast<int>(m[33].N), 1704) << "A4 N";

  EXPECT_EQ(static_cast<int>(m[60].k), 17) << "C7 k";
  EXPECT_EQ(static_cast<int>(m[60].N), 358) << "C7 N";
}

TEST(SlidingDFT, IntegrationBenchmark) {
  auto sdft = SlidingDFT(make_shared<PianoTuning>(SAMPLE_RATE), -1.);
  const unsigned bufferSize = 128;
  float input[bufferSize];
  const float *output = nullptr;

  auto start = chrono::high_resolution_clock::now();
  unsigned i;
  for (i = 0; i < bufferSize * 10000; i++) {
    unsigned j = i % bufferSize;
    input[j] = oscillator(i, SAWTOOTH);
    if (j == bufferSize - 1)
      output = sdft.process(input, bufferSize, .05);
  }
  auto end = chrono::high_resolution_clock::now();
  chrono::duration<double> elapsed = end - start;
  cerr << "# benchmark: " << static_cast<int>(round(i / elapsed.count())) << " samples per second" << endl;

  map<int,double> test = {
    { 21, .0039842012338340 },
    { 33, .7776550054550171 },
    { 45, .3889078497886658 },
    { 52, .2582178413867950 },
    { 57, .1949640512466431 }
  };
  for (auto kv : test) {
    EXPECT_NEAR(output[kv.first], test[kv.first], ABS_ERROR) << "sawtooth, key #" + to_string(kv.first);
    // char buf[20]; snprintf(buf, 20, "%.16f", output[kv.first]); cerr << buf << endl;
  }
}
