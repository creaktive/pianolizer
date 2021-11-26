#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <vector>

class RingBuffer {
  private:
    unsigned mask;
    unsigned index = 0;
    float* buffer;

  public:
    RingBuffer(unsigned requestedSize) {
      const unsigned bits = ceil(log2(requestedSize + 1));
      const unsigned size = 1 << bits;
      mask = size - 1;
      buffer = new float[size];
      for (unsigned i = 0; i < size; i++)
        buffer[i] = 0.;
    }

    ~RingBuffer() {
      delete [] buffer;
    }

    void write(double value) {
      index &= mask;
      buffer[index++] = value;
    }

    double read(unsigned position) {
      return buffer[(index + (~position)) & mask];
    }
};

class DFTBin {
  private:
    double totalPower = 0.;
    std::complex<double> coeff;
    std::complex<double> dft = std::complex<double>(0., 0.);

  public:
    double k, N;
    double referenceAmplitude = 1.; // 0 dB level

    DFTBin(unsigned k_, unsigned N_)
      : k(k_), N(N_) {
      if (k == 0)
        throw std::invalid_argument("k=0 (DC) not implemented");
      else if (N == 0)
        throw std::invalid_argument("N=0 is soooo not supported (Y THO?)");

      coeff = exp(std::complex<double>(0., 2. * M_PI * (k / N)));
    }

    void update(double previousSample, double currentSample) {
      totalPower += currentSample * currentSample;
      totalPower -= previousSample * previousSample;

      dft = coeff * ((dft - std::complex<double>(previousSample, 0.)) + std::complex<double>(currentSample, 0.));
    }

    double rms() {
      return sqrt(totalPower / N);
    }

    double amplitudeSpectrum() {
      return M_SQRT2 * (sqrt(norm(dft)) / N);
    }

    double normalizedAmplitudeSpectrum() {
      return totalPower > 0.
        ? amplitudeSpectrum() / rms()
        : 0.;
    }

    double logarithmicUnitDecibels() {
      return 20. * log10(amplitudeSpectrum() / referenceAmplitude);
    }
};

class MovingAverage {
  public:
    unsigned channels, sampleRate;
    int averageWindow = -1;
    unsigned targetAverageWindow;
    float *sum;

    MovingAverage(unsigned channels_, unsigned sampleRate_)
      : channels(channels_), sampleRate(sampleRate_) {
      sum = new float[channels];
    }

    ~MovingAverage() {
      delete [] sum;
    }

    double averageWindowInSeconds() {
      return averageWindow / sampleRate;
    }

    void averageWindowInSeconds(double value) {
      targetAverageWindow = round(value * sampleRate);
      if (averageWindow == -1)
        averageWindow = targetAverageWindow;
    }

    void updateAverageWindow() {
      if (targetAverageWindow > averageWindow)
        averageWindow++;
      else if (targetAverageWindow < averageWindow)
        averageWindow--;
    }

    double read(unsigned n) {
      return sum[n] / averageWindow;
    }

    void update(float levels[]) {}
};

class FastMovingAverage : public MovingAverage {
  public:
    FastMovingAverage(unsigned channels_, unsigned sampleRate_)
      : MovingAverage{ channels_, sampleRate_ }
    {}

    void update(float levels[]) {
      updateAverageWindow();
      for (unsigned n = 0; n < channels; n++) {
        const double currentSum = sum[n];
        sum[n] = averageWindow
          ? currentSum + levels[n] - currentSum / averageWindow
          : levels[n];
      }
    }
};

class HeavyMovingAverage : public MovingAverage {
  private:
    std::vector<RingBuffer*> history;

  public:
    HeavyMovingAverage(unsigned channels_, unsigned sampleRate_, unsigned maxWindow = 0)
      : MovingAverage{ channels_, sampleRate_ }
    {
      history.reserve(channels);
      for (unsigned n = 0; n < channels; n++)
        history.push_back(new RingBuffer(maxWindow ? maxWindow : sampleRate));
    }

    ~HeavyMovingAverage() {
      for (unsigned n = 0; n < channels; n++)
        delete history[n];
    }

    void update(float levels[]) {
      for (unsigned n = 0; n < channels; n++) {
        const double value = levels[n];
        history[n]->write(value);
        sum[n] += value;

        if (targetAverageWindow == averageWindow) {
          sum[n] -= history[n]->read(averageWindow);
        } else if (targetAverageWindow < averageWindow) {
          sum[n] -= history[n]->read(averageWindow);
          sum[n] -= history[n]->read(averageWindow - 1);
        }
      }
      updateAverageWindow();
    }
};

class Tuning {
  public:
    unsigned sampleRate, bands;
    struct tuningValues {
      unsigned k, N;
    };

    Tuning(unsigned sampleRate_, unsigned bands_)
      : sampleRate(sampleRate_), bands(bands_)
    {}

    tuningValues frequencyAndBandwidthToKAndN(double frequency, double bandwidth) {
      double N = floor(sampleRate / bandwidth);
      const double k = floor(frequency / bandwidth);

      // find such N that (sampleRate * (k / N)) is the closest to freq
      // (sacrifices the bandwidth precision; bands will be *wider*, and, therefore, will overlap a bit!)
      double delta = abs(sampleRate * (k / N) - frequency);
      for (unsigned i = N - 1; ; i--) {
        const double tmpDelta = abs(sampleRate * (k / i) - frequency);
        if (tmpDelta < delta) {
          delta = tmpDelta;
          N = i;
        } else {
          tuningValues result = { (unsigned) k, (unsigned) N };
          return result;
        }
      }
    }

    // std::vector<tuningValues> mapping();
};

class PianoTuning : public Tuning {
  private:
    unsigned referenceKey;
    double pitchFork;

  public:
    PianoTuning(unsigned sampleRate_, unsigned keysNum = 61, unsigned referenceKey_ = 33, double pitchFork_ = 440.0)
      : Tuning{ sampleRate_, keysNum }, referenceKey(referenceKey_), pitchFork(pitchFork_)
    {}

    double keyToFreq(double key) {
      return pitchFork * pow(2., (key - referenceKey) / 12.);
    }

    std::vector<tuningValues> mapping() {
      std::vector<tuningValues> output;
      output.reserve(bands);
      for (unsigned key = 0; key < bands; key++) {
        const double frequency = keyToFreq(key);
        const double bandwidth = 2. * (keyToFreq(key + .5) - frequency);
        output.push_back(frequencyAndBandwidthToKAndN(frequency, bandwidth));
      }
      return output;
    }
};

class SlidingDFT {
  private:
    std::vector<DFTBin*> bins;
    float* levels;
    RingBuffer* ringBuffer;
    MovingAverage* movingAverage;

  public:
    SlidingDFT(PianoTuning tuning, double maxAverageWindowInSeconds = 0.) {
      bins.reserve(tuning.bands);
      levels = new float[tuning.bands];

      unsigned maxN = 0;
      for (auto band : tuning.mapping()) {
        bins.push_back(new DFTBin(band.k, band.N));
        maxN = fmax(maxN, band.N);
      }

      ringBuffer = new RingBuffer(maxN);

      if (maxAverageWindowInSeconds > 0.) {
        movingAverage = new HeavyMovingAverage(
          tuning.bands,
          tuning.sampleRate,
          round(tuning.sampleRate * maxAverageWindowInSeconds)
        );
      } else if (maxAverageWindowInSeconds < 0.) {
        movingAverage = new FastMovingAverage(
          tuning.bands,
          tuning.sampleRate
        );
      } else {
        movingAverage = NULL;
      }
    }

    ~SlidingDFT() {
      delete [] levels;
      delete ringBuffer;
      delete movingAverage;
    }

    float* process(float samples[], unsigned samplesLength, double averageWindowInSeconds = 0.) {
      if (movingAverage != NULL)
        movingAverage->averageWindowInSeconds(averageWindowInSeconds);

      const unsigned binsNum = bins.size();

      // store in the ring buffer & process
      for (unsigned i = 0; i < samplesLength; i++) {
        const float currentSample = samples[i];
        samples[i] = 0.;
        ringBuffer->write(currentSample);

        for (unsigned band = 0; band < binsNum; band++) {
          DFTBin* bin = bins[band];
          const float previousSample = ringBuffer->read(bin->N);
          bin->update(previousSample, currentSample);
          levels[band] = bin->normalizedAmplitudeSpectrum();
          // levels[band] = bin->logarithmicUnitDecibels();
        }

        if (movingAverage != NULL)
          movingAverage->update(levels);
      }

      // snapshot of the levels, after smoothing
      if (movingAverage != NULL && movingAverage->averageWindow > 0)
        for (unsigned band = 0; band < binsNum; band++)
          levels[band] = movingAverage->read(band);

      return levels;
    }
};
