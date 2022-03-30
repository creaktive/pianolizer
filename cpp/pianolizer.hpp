/**
 * @file pianolizer.hpp
 * @brief Musical tone pitch detection library based on the Sliding Discrete Fourier Transform algorithm.
 * @see http://github.com/creaktive/pianolizer
 * @author Stanislaw Pusep
 * @copyright MIT
 */

#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

/**
 * Reasonably fast Ring Buffer implementation.
 * Caveat: the size of the allocated memory is always a power of two!
 *
 * @class RingBuffer
 * @par EXAMPLE
 * auto rb = RingBuffer(100);
 * for (unsigned i = 0; i < 200; i++)
 *   rb.write(i);
 * // prints 174:
 * std::cout << rb.read(25) << std::endl;
 */
class RingBuffer {
  private:
    unsigned mask;
    unsigned index = 0;
    std::unique_ptr<float[]> buffer;

  public:
    /**
     * Creates an instance of RingBuffer.
     * @param requestedSize How long the RingBuffer is expected to be.
     * @memberof RingBuffer
     */
    RingBuffer(const unsigned requestedSize) {
      const unsigned bits = ceil(log2(requestedSize + 1));
      const unsigned size = static_cast<unsigned>(1) << bits;
      mask = size - 1;
      buffer = std::make_unique<float[]>(size);
    }

    /**
     * Shifts the RingBuffer and stores the value in the latest position.
     *
     * @param value Value to be stored.
     * @memberof RingBuffer
     */
    void write(const float value) {
      index &= mask;
      buffer[index++] = value;
    }

    /**
     * Retrieves the value stored at the position.
     *
     * @param position Position within the RingBuffer.
     * @return The value at the position.
     * @memberof RingBuffer
     */
    float read(const unsigned position) {
      return buffer[(index + (~position)) & mask];
    }
};

/**
 * Discrete Fourier Transform computation for one single bin.
 *
 * @class DFTBin
 * @par EXAMPLE
 * // Detect a 441Hz tone when the sample rate is 44100Hz
 * const unsigned N = 1700;
 * auto bin = DFTBin(17, N);
 * auto rb = RingBuffer(N);
 * for (unsigned i = 0; i < 2000; i++) {
 *   const double currentSample = sin(M_PI / 50 * i); // sine wave oscillator
 *   rb.write(currentSample);
 *   // previousSample should be taken N samples before currentSample is taken
 *   const double previousSample = rb.read(N);
 *   bin.update(previousSample, currentSample);
 * }

 * std::cout << bin.rms() << std::endl;
 * std::cout << bin.amplitudeSpectrum() << std::endl;
 * std::cout << bin.normalizedAmplitudeSpectrum() << std::endl;
 * std::cout << bin.logarithmicUnitDecibels() << std::endl;
 */
class DFTBin {
  private:
    double totalPower = 0.;
    std::complex<double> coeff;
    std::complex<double> dft = std::complex<double>(0., 0.);

  public:
    double k, N;
    double referenceAmplitude = 1.; // 0 dB level

    /**
     * Creates an instance of DFTBin.
     * @param k_ Frequency divided by the bandwidth.
     * @param N_ Sample rate divided by the bandwidth.
     * @memberof DFTBin
     * @par EXAMPLE
     * // (provided the sample rate of 44100Hz)
     * // center: 439.96Hz
     * // bandwidth: 25.88Hz
     * auto bin = DFTBin(17, 1704);
     * // samples are *NOT* complex!
     * bin.update(previousSample, currentSample);
     */
    DFTBin(const unsigned k_, const unsigned N_)
      : k(k_), N(N_) {
      if (k == 0)
        throw std::invalid_argument("k=0 (DC) not implemented");
      else if (N == 0)
        throw std::invalid_argument("N=0 is soooo not supported (Y THO?)");

      const double q = 2. * M_PI * k / N;
      coeff = std::complex<double>(cos(q), sin(q));
    }

    /**
     * Do the Sliding DFT computation.
     *
     * @param previousSample Sample from N frames ago.
     * @param currentSample The latest sample.
     * @memberof DFTBin
     */
    void update(const double previousSample, const double currentSample) {
      totalPower += currentSample * currentSample;
      totalPower -= previousSample * previousSample;

      dft = coeff * (dft - std::complex<double>(previousSample, 0.) + std::complex<double>(currentSample, 0.));
    }

    /**
     * Root Mean Square.
     *
     * @memberof DFTBin
     */
    double rms() {
      return sqrt(totalPower / N);
    }

    /**
     * Amplitude spectrum in volts RMS.
     *
     * @see https://www.sjsu.edu/people/burford.furman/docs/me120/FFT_tutorial_NI.pdf
     * @memberof DFTBin
     */
    double amplitudeSpectrum() {
      return M_SQRT2 * (sqrt(norm(dft)) / N);
    }

    /**
     * Normalized amplitude (always returns a value between 0.0 and 1.0).
     * This is well suited to detect pure tones, and can be used to decode DTMF or FSK modulation.
     *
     * @memberof DFTBin
     */
    double normalizedAmplitudeSpectrum() {
      return totalPower > 0.
        ? amplitudeSpectrum() / rms()
        : 0.;
    }

    /**
     * Using this unit of measure, it is easy to view wide dynamic ranges; that is,
     * it is easy to see small signal components in the presence of large ones.
     *
     * @memberof DFTBin
     */
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

    virtual ~MovingAverage() = default;

    float averageWindowInSeconds() {
      return averageWindow / static_cast<float>(sampleRate);
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
          sum[n] -= history[n]->read(static_cast<unsigned>(averageWindow));
        } else if (targetAverageWindow < averageWindow) {
          sum[n] -= history[n]->read(static_cast<unsigned>(averageWindow));
          sum[n] -= history[n]->read(static_cast<unsigned>(averageWindow) - 1);
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

    virtual ~Tuning() = default;

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
          return { static_cast<unsigned>(k), static_cast<unsigned>(N) };
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
    PianoTuning(const unsigned sampleRate_, const double pitchFork_ = 440.0, const unsigned keysNum = 61, const unsigned referenceKey_ = 33)
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
#ifndef DISABLE_MOVING_AVERAGE
    std::shared_ptr<MovingAverage> movingAverage;
#endif

  public:
    unsigned sampleRate, bands;

    SlidingDFT(const std::shared_ptr<Tuning> tuning, const double maxAverageWindowInSeconds = 0.) {
      sampleRate = tuning->sampleRate;
      bands = tuning->bands;
      bins.reserve(bands);
      levels.reserve(bands);

      unsigned maxN = 0;
      for (auto band : tuning->mapping()) {
        bins.push_back(std::make_shared<DFTBin>(band.k, band.N));
        maxN = maxN > band.N ? maxN : band.N;
      }

      ringBuffer = std::make_unique<RingBuffer>(maxN);

#ifndef DISABLE_MOVING_AVERAGE
      if (maxAverageWindowInSeconds > 0.) {
        movingAverage = std::make_shared<HeavyMovingAverage>(
          bands,
          sampleRate,
          round(sampleRate * maxAverageWindowInSeconds)
        );
      } else if (maxAverageWindowInSeconds < 0.) {
        movingAverage = std::make_shared<FastMovingAverage>(
          bands,
          sampleRate
        );
      } else {
        movingAverage = nullptr;
      }
#endif
    }

    const float* process(const float samples[], const size_t samplesLength, const double averageWindowInSeconds = 0.) {
#ifndef DISABLE_MOVING_AVERAGE
      if (movingAverage != nullptr)
        movingAverage->averageWindowInSeconds(averageWindowInSeconds);
#endif

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

#ifdef DISABLE_MOVING_AVERAGE
      }
#else
        if (movingAverage != nullptr)
          movingAverage->update(levels);
      }

      // snapshot of the levels, after smoothing
      if (movingAverage != nullptr && movingAverage->averageWindow > 0)
        for (unsigned band = 0; band < binsNum; band++)
          levels[band] = movingAverage->read(band);
#endif

      return levels.data();
    }
};
