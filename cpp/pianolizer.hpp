#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

class RingBuffer {
  private:
    unsigned mask;
    unsigned index = 0;
    std::unique_ptr<float[]> buffer;

  public:
    RingBuffer(const unsigned requestedSize) {
      const unsigned bits = ceil(log2(requestedSize + 1));
      const unsigned size = 1 << bits;
      mask = size - 1;
      buffer = std::make_unique<float[]>(size);
    }

    void write(const float value) {
      index &= mask;
      buffer[index++] = value;
    }

    float read(const unsigned position) {
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

    DFTBin(const unsigned k_, const unsigned N_)
      : k(k_), N(N_) {
      if (k == 0)
        throw std::invalid_argument("k=0 (DC) not implemented");
      else if (N == 0)
        throw std::invalid_argument("N=0 is soooo not supported (Y THO?)");

      coeff = exp(std::complex<double>(0., 2. * M_PI * (k / N)));
    }

    void update(const double previousSample, const double currentSample) {
      totalPower += currentSample * currentSample;
      totalPower -= previousSample * previousSample;

      dft = coeff * (dft - std::complex<double>(previousSample, 0.) + std::complex<double>(currentSample, 0.));
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
    int targetAverageWindow;
    std::unique_ptr<float[]> sum;

    MovingAverage(const unsigned channels_, const unsigned sampleRate_)
      : channels(channels_), sampleRate(sampleRate_) {
      sum = std::make_unique<float[]>(channels);
    }

    virtual ~MovingAverage() {}

    float averageWindowInSeconds() {
      return averageWindow / sampleRate;
    }

    void averageWindowInSeconds(const float value) {
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

    float read(const unsigned n) {
      return sum[n] / averageWindow;
    }

    virtual void update(const std::vector<float>& levels) = 0;
};

class FastMovingAverage : public MovingAverage {
  public:
    FastMovingAverage(const unsigned channels_, const unsigned sampleRate_)
      : MovingAverage{ channels_, sampleRate_ }
    {}

    void update(const std::vector<float>& levels) {
      updateAverageWindow();
      for (unsigned n = 0; n < channels; n++) {
        const float currentSum = sum[n];
        sum[n] = averageWindow
          ? currentSum + levels[n] - currentSum / averageWindow
          : levels[n];
      }
    }
};

class HeavyMovingAverage : public MovingAverage {
  private:
    std::vector<std::unique_ptr<RingBuffer>> history;

  public:
    HeavyMovingAverage(const unsigned channels_, const unsigned sampleRate_, const unsigned maxWindow = 0)
      : MovingAverage{ channels_, sampleRate_ }
    {
      history.reserve(channels);
      for (unsigned n = 0; n < channels; n++)
        history.push_back(std::make_unique<RingBuffer>(maxWindow ? maxWindow : sampleRate));
    }

    void update(const std::vector<float>& levels) {
      for (unsigned n = 0; n < channels; n++) {
        const float value = levels[n];
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

    Tuning(const unsigned sampleRate_, const unsigned bands_)
      : sampleRate(sampleRate_), bands(bands_)
    {}

    virtual ~Tuning() {}

    const tuningValues frequencyAndBandwidthToKAndN(const double frequency, const double bandwidth) {
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
          return { (unsigned) k, (unsigned) N };
        }
      }
    }

    const virtual std::vector<tuningValues> mapping() = 0;
};

class PianoTuning : public Tuning {
  private:
    unsigned referenceKey;
    double pitchFork;

  public:
    PianoTuning(const unsigned sampleRate_, const unsigned keysNum = 61, const unsigned referenceKey_ = 33, const double pitchFork_ = 440.0)
      : Tuning{ sampleRate_, keysNum }, referenceKey(referenceKey_), pitchFork(pitchFork_)
    {}

    double keyToFreq(const double key) {
      return pitchFork * pow(2., (key - referenceKey) / 12.);
    }

    const std::vector<tuningValues> mapping() {
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
    std::vector<std::shared_ptr<DFTBin>> bins;
    std::vector<float> levels;
    std::unique_ptr<RingBuffer> ringBuffer;
    std::shared_ptr<MovingAverage> movingAverage;

  public:
    SlidingDFT(const std::shared_ptr<Tuning> tuning, const double maxAverageWindowInSeconds = 0.) {
      bins.reserve(tuning->bands);
      levels.reserve(tuning->bands);

      unsigned maxN = 0;
      for (auto band : tuning->mapping()) {
        bins.push_back(std::make_shared<DFTBin>(band.k, band.N));
        maxN = maxN > band.N ? maxN : band.N;
      }

      ringBuffer = std::make_unique<RingBuffer>(maxN);

      if (maxAverageWindowInSeconds > 0.) {
        movingAverage = std::make_shared<HeavyMovingAverage>(
          tuning->bands,
          tuning->sampleRate,
          round(tuning->sampleRate * maxAverageWindowInSeconds)
        );
      } else if (maxAverageWindowInSeconds < 0.) {
        movingAverage = std::make_shared<FastMovingAverage>(
          tuning->bands,
          tuning->sampleRate
        );
      } else {
        movingAverage = nullptr;
      }
    }

    const float* process(const float samples[], const size_t samplesLength, const double averageWindowInSeconds = 0.) {
      if (movingAverage != nullptr)
        movingAverage->averageWindowInSeconds(averageWindowInSeconds);

      const unsigned binsNum = bins.size();

      // store in the ring buffer & process
      for (unsigned i = 0; i < samplesLength; i++) {
        const float currentSample = samples[i];
        ringBuffer->write(currentSample);

        unsigned band = 0;
        for (auto bin : bins) {
          const float previousSample = ringBuffer->read(bin->N);
          bin->update(previousSample, currentSample);
          levels[band] = bin->normalizedAmplitudeSpectrum();
          // levels[band] = bin->logarithmicUnitDecibels();
          band++;
        }

        if (movingAverage != nullptr)
          movingAverage->update(levels);
      }

      // snapshot of the levels, after smoothing
      if (movingAverage != nullptr && movingAverage->averageWindow > 0)
        for (unsigned band = 0; band < binsNum; band++)
          levels[band] = movingAverage->read(band);

      return levels.data();
    }
};
