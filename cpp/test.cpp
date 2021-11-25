#include <iostream>
#include <stdlib.h>
#include "pianolizer.hpp"

using namespace std;

// kill me
unsigned static TEST_COUNT = 0;
#define TEST_OK(expression, description) { \
  if (expression) {\
    cout << "ok " << ++TEST_COUNT << " - " << description << endl; \
  } else { \
    cout << "not ok " << ++TEST_COUNT << " - " << description << endl; \
    cerr << __FUNCTION__ << " failed on line " << __LINE__ << endl; \
  } \
}
#define FLOAT_EQ(a, b) (abs(a - b) <= 1e-4)

void testRingBuffer() {
  RingBuffer rb = RingBuffer(15);

  TEST_OK(rb.read(0) == 0, "initalized to zeroes");

  // write one single element
  rb.write(1);
  TEST_OK(rb.read(0) == 1, "insertion succeeded");
  TEST_OK(rb.read(1) == 0, "boundary shifted");

  // write 9 more elements
  for (unsigned i = 2; i < 10; i++)
    rb.write(i);

  // read in sequence
  for (unsigned i = 0; i < 10; i++)
    TEST_OK(rb.read(9 - i) == i, "sequence value matches");

  // overflow
  for (unsigned i = 10; i < 20; i++)
    rb.write(i);

  // both the head and the tail are expected values
  TEST_OK(rb.read(0) == 19, "head as expected");
  TEST_OK(rb.read(15) == 4, "tail as expected");

  // reading beyond the capacity of ring buffer wraps it around
  TEST_OK(rb.read(16) == 19, "wrap back to 0");
  TEST_OK(rb.read(17) == 18, "wrap back to 1");
}

const float TWO_PI = 2. * acos(-1.);

const unsigned SINE = 0;
const unsigned SAWTOOTH = 1;
const unsigned SQUARE = 2;
const unsigned NOISE = 3;

// 441Hz wave period is 100 samples when the sample rate is 44100Hz
float oscillator(unsigned s, unsigned type = SINE) {
  switch (type) {
    case SINE:
      return sin(TWO_PI / 100. * s);
    case SAWTOOTH:
      return ((s % 100) / 50.) - 1.;
    case SQUARE:
      return ((s % 100) < 50) ? 1. : -1.;
    case NOISE:
      return 2. * (rand() / RAND_MAX) - 1.;
  }
  return 0.;
}

void testDFT(unsigned type, float expNAS, float expRMS, float expLog) {
  const unsigned N = 1700;
  DFTBin bin = DFTBin(17, N);
  RingBuffer rb = RingBuffer(N);
  for (unsigned i = 0; i < 2000; i++) {
    const float currentSample = oscillator(i, type);
    rb.write(currentSample);
    const float previousSample = rb.read(N);
    bin.update(previousSample, currentSample);
  }

  const string prefix = "oscillator #" + to_string(type) + "; ";
  TEST_OK(FLOAT_EQ(expNAS, bin.normalizedAmplitudeSpectrum()), prefix + "normalized amplitude spectrum");
  TEST_OK(FLOAT_EQ(expRMS, bin.rms()), prefix + "RMS");
  TEST_OK(FLOAT_EQ(expLog, bin.logarithmicUnitDecibels()), prefix + "log dB");
}

int main(int argc, char *argv[]) {
  testRingBuffer();

  // reference values from the JS implementation
  testDFT(SINE, .9999999999999972, .7071067809649849, -3.0102999593614452);
  testDFT(SAWTOOTH, .7797470999951004, .5774080013883754, -6.93126867978036);
  testDFT(SQUARE, .9004644293093976, 1., -0.9106687653789797);

  cout << "1.." << TEST_COUNT << endl;
  return 0;
}