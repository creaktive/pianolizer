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
#define FLOAT_EQ(a, b) (abs(a - b) <= 1e-8)

void testRingBuffer() {
  auto rb = RingBuffer(15);

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

const unsigned SAMPLE_RATE = 44100;

const unsigned SINE = 0;
const unsigned SAWTOOTH = 1;
const unsigned SQUARE = 2;
const unsigned NOISE = 3;

// 441Hz wave period is 100 samples when the sample rate is 44100Hz
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
  }
  return 0.;
}

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
  TEST_OK(FLOAT_EQ(expNAS, bin.normalizedAmplitudeSpectrum()), prefix + "normalized amplitude spectrum");
  TEST_OK(FLOAT_EQ(expRMS, bin.rms()), prefix + "RMS");
  TEST_OK(FLOAT_EQ(expLog, bin.logarithmicUnitDecibels()), prefix + "log dB");
}

void testMovingAverage() {
  auto fma = FastMovingAverage(2, SAMPLE_RATE);
  fma.averageWindowInSeconds(0.01);

  auto hma = HeavyMovingAverage(2, SAMPLE_RATE, 500);
  hma.averageWindowInSeconds(0.01);

  for (unsigned i = 0; i < 500; i++) {
    float sample[2] = { oscillator(i, SINE), oscillator(i, SAWTOOTH) };
    fma.update(sample);
    hma.update(sample);
  }

  TEST_OK(FLOAT_EQ(fma.read(0), -.024506002326671227), "sine fast average");
  TEST_OK(FLOAT_EQ(fma.read(1), .01886483060529713), "sawtooth fast average");

  TEST_OK(FLOAT_EQ(hma.read(0), -.06714661267338967), "sine heavy average");
  TEST_OK(FLOAT_EQ(hma.read(1), .04485260926676986), "sawtooth heavy average");
}

void testTuning() {
  auto pt = PianoTuning(SAMPLE_RATE);
  auto m = pt.mapping();

  TEST_OK(m.size() == 61, "mapping size");

  TEST_OK(m[0].k == 17, "C2 k");
  TEST_OK(m[0].N == 11462, "C2 N");

  TEST_OK(m[33].k == 17, "A4 k");
  TEST_OK(m[33].N == 1704, "A4 N");

  TEST_OK(m[60].k == 17, "C7 k");
  TEST_OK(m[60].N == 358, "C7 N");
}

void testSlidingDFT() {
  auto sdft = SlidingDFT(new PianoTuning(SAMPLE_RATE), -1.);
  float test[4] = {1., 2., 3., 4.};
  sdft.process(test, 4);
}

int main(int argc, char *argv[]) {
  testRingBuffer();

  // reference values from the JS implementation
  testDFT(SINE, .9999999999999972, .7071067809649849, -3.0102999593614452);
  testDFT(SAWTOOTH, .7797470999951004, .5774080013883754, -6.93126867978036);
  testDFT(SQUARE, .9004644293093976, 1., -0.9106687653789797);

  testMovingAverage();
  testTuning();
  testSlidingDFT();

  cout << "1.." << TEST_COUNT << endl;
  return 0;
}