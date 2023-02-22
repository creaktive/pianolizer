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
#include <cstring>
#include <memory>
#include <vector>

// For C++11 compatibility: https://herbsutter.com/gotw/_102/
#if __cplusplus < 201402L
  namespace std {
    template<typename T, typename... Args>
    std::unique_ptr<T> make_unique(Args&&... args) {
      return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
    }
  }
#endif

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
    std::vector<float> buffer;

  public:
    unsigned size;

    /**
     * Creates an instance of RingBuffer.
     * @param requestedSize How long the RingBuffer is expected to be.
     * @memberof RingBuffer
     */
    RingBuffer(const unsigned requestedSize) {
      const unsigned bits = std::ceil(std::log2(requestedSize));
      size = static_cast<unsigned>(1) << bits;
      mask = size - 1;
      buffer.reserve(size);
      memset(buffer.data(), 0, sizeof(float) * size);
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
    double r;
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
        throw std::invalid_argument("N=0 is so not supported (Y THO?)");

      const double q = 2. * M_PI * k / N;
      r = 2. / N;
      coeff = std::complex<double>(cos(q), -sin(q));
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
      return std::sqrt(totalPower / N);
    }

    /**
     * Amplitude spectrum in volts RMS.
     *
     * @see https://www.sjsu.edu/people/burford.furman/docs/me120/FFT_tutorial_NI.pdf
     * @memberof DFTBin
     */
    double amplitudeSpectrum() {
      return M_SQRT2 * std::sqrt(norm(dft)) / N;
    }

    /**
     * Normalized amplitude (always returns a value between 0.0 and 1.0).
     * This is well suited to detect pure tones, and can be used to decode DTMF or FSK modulation.
     * Depending on the application, you might need sqrt(d.normalizedAmplitudeSpectrum()).
     *
     * @memberof DFTBin
     */
    double normalizedAmplitudeSpectrum() {
      return totalPower > 0.
        // ? amplitudeSpectrum() / rms()
        ? r * norm(dft) / totalPower // same as the square of the above, but uses less FLOPs
        : 0.;
    }

    /**
     * Using this unit of measure, it is easy to view wide dynamic ranges; that is,
     * it is easy to see small signal components in the presence of large ones.
     *
     * @memberof DFTBin
     */
    double logarithmicUnitDecibels() {
      return 20. * std::log10(amplitudeSpectrum() / referenceAmplitude);
    }
};

/**
 * Base class for FastMovingAverage & HeavyMovingAverage. Must implement the update(levels) method.
 *
 * @class MovingAverage
 */
class MovingAverage {
  public:
    unsigned channels, sampleRate;
    int averageWindow = -1;
    int targetAverageWindow;
    std::vector<float> sum;

    /**
     * Creates an instance of MovingAverage.
     * @param channels_ Number of channels to process.
     * @param sampleRate_ Sample rate, used to convert between time and amount of samples.
     * @memberof MovingAverage
     */
    MovingAverage(const unsigned channels_, const unsigned sampleRate_)
      : channels(channels_), sampleRate(sampleRate_) {
      sum.reserve(channels);
      memset(sum.data(), 0, sizeof(float) * channels);
    }

    virtual ~MovingAverage() = default;

    /**
     * Get the current window size (in seconds).
     *
     * @memberof MovingAverage
     */
    float averageWindowInSeconds() {
      return averageWindow / static_cast<float>(sampleRate);
    }

    /**
     * Set the current window size (in seconds).
     *
     * @memberof MovingAverage
     */
    void averageWindowInSeconds(const float value) {
      targetAverageWindow = std::round(value * sampleRate);
      if (averageWindow == -1)
        averageWindow = targetAverageWindow;
    }

    /**
     * Adjust averageWindow in steps.
     *
     * @memberof MovingAverage
     */
    void updateAverageWindow() {
      if (targetAverageWindow > averageWindow)
        averageWindow++;
      else if (targetAverageWindow < averageWindow)
        averageWindow--;
    }

    /**
     * Retrieve the current moving average value for a given channel.
     *
     * @param n Number of channel to retrieve the moving average for.
     * @return Current moving average value for the specified channel.
     * @memberof MovingAverage
     */
    float read(const unsigned n) {
      return sum[n] / averageWindow;
    }

    virtual void update(const std::vector<float>& levels) = 0;
};

/**
 * Moving average of the output (effectively a low-pass to get the general envelope).
 * Fast approximation of the MovingAverage; requires significantly less memory.
 *
 * @see https://www.daycounter.com/LabBook/Moving-Average.phtml
 * @class FastMovingAverage
 * @extends MovingAverage
 * @par EXAMPLE
 * // initialize the moving average object
 * unsigned channels = 2;
 * auto movingAverage = FastMovingAverage(
 *   channels,
 *   sampleRate
 * );
 * // averageWindowInSeconds can be updated on-fly!
 * movingAverage.averageWindowInSeconds(0.05);
 * // for every processed frame
 * movingAverage.update(levels);
 * // overwrite the levels with the averaged ones
 * for (unsigned band = 0; band < channels; band++)
 *   levels[band] = movingAverage.read(band);
 */
class FastMovingAverage : public MovingAverage {
  public:
    FastMovingAverage(const unsigned channels_, const unsigned sampleRate_)
      : MovingAverage{ channels_, sampleRate_ }
    {}

    /**
     * Update the internal state with from the input.
     *
     * @param levels Array of level values, one per channel.
     * @memberof FastMovingAverage
     */
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

/**
 * Moving average of the output (effectively a low-pass to get the general envelope).
 * This is the "proper" implementation; it does require lots of memory allocated for the RingBuffers!
 *
 * @class HeavyMovingAverage
 * @extends MovingAverage
 * @par EXAMPLE
 * // initialize the moving average object
 * unsigned channels = 2;
 * movingAverage = HeavyMovingAverage(
 *   channels,
 *   sampleRate,
 *   round(sampleRate * maxAverageWindowInSeconds)
 * )
 * // averageWindowInSeconds can be updated on-fly!
 * movingAverage.averageWindowInSeconds(0.05);
 * // for every processed frame
 * movingAverage.update(levels);
 * // overwrite the levels with the averaged ones
 * for (unsigned band = 0; band < channels; band++)
 *   levels[band] = movingAverage.read(band);
 */
class HeavyMovingAverage : public MovingAverage {
  private:
    std::vector<std::unique_ptr<RingBuffer>> history;

  public:
    /**
     * Creates an instance of HeavyMovingAverage.
     * @param channels Number of channels to process.
     * @param sampleRate Sample rate, used to convert between time and amount of samples.
     * @param [maxWindow=sampleRate] Preallocate buffers of this size, per channel.
     * @memberof HeavyMovingAverage
     */
    HeavyMovingAverage(const unsigned channels_, const unsigned sampleRate_, const unsigned maxWindow = 0)
      : MovingAverage{ channels_, sampleRate_ }
    {
      history.reserve(channels);
      for (unsigned n = 0; n < channels; n++)
        history.push_back(std::make_unique<RingBuffer>(maxWindow ? maxWindow : sampleRate));
    }

    /**
     * Update the internal state with from the input.
     *
     * @param levels Array of level values, one per channel.
     * @memberof HeavyMovingAverage
     */
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

/**
 * Base class for PianoTuning. Must implement mapping().
 *
 * @class Tuning
 */
class Tuning {
  public:
    unsigned sampleRate, bands;
    struct tuningValues {
      unsigned k, N;
    };

    /**
     * Creates an instance of Tuning.
     * @param sampleRate_ Self-explanatory.
     * @param bands_ How many filters.
     */
    Tuning(const unsigned sampleRate_, const unsigned bands_)
      : sampleRate(sampleRate_), bands(bands_)
    {}

    virtual ~Tuning() = default;

    /**
     * Approximate k & N values for DFTBin.
     *
     * @param frequency In Hz.
     * @param bandwidth In Hz.
     * @return tuningValues struct containing k & N that best approximate for the given frequency & bandwidth.
     * @memberof Tuning
     */
    const tuningValues frequencyAndBandwidthToKAndN(const double frequency, const double bandwidth) {
      double N = std::floor(sampleRate / bandwidth);
      const double k = std::floor(frequency / bandwidth);

      // find such N that (sampleRate * (k / N)) is the closest to freq
      // (sacrifices the bandwidth precision; bands will be *wider*, and, therefore, will overlap a bit!)
      double delta = std::fabs(sampleRate * (k / N) - frequency);
      for (unsigned i = N - 1; ; i--) {
        const double tmpDelta = std::fabs(sampleRate * (k / i) - frequency);
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

/**
 * Essentially, creates an instance that provides the 'mapping',
 * which is an array of objects providing the values for i, k & N.
 *
 * @class PianoTuning
 * @extends Tuning
 * @example
 * // a common sample rate
 * auto tuning = PianoTuning(44100);
 *
 * // prints 17 for the note C2:
 * std::cout << tuning.mapping[0].k << std::endl;
 * // prints 11462 for the note C2:
 * std::cout << tuning.mapping[0].N << std::endl;
 *
 * // prints 17 for the note C7:
 * std::cout << tuning.mapping[60].k << std::endl;
 * // prints 358 for the note C7:
 * std::cout << tuning.mapping[60].N << std::endl;
 */
class PianoTuning : public Tuning {
  private:
    unsigned referenceKey;
    double pitchFork;
    double tolerance;

  public:
    /**
     * Creates an instance of PianoTuning.
     * @param sampleRate This directly influences the memory usage: 44100Hz or 48000Hz will both allocate a buffer of 64KB (provided 32-bit floats are used).
     * @param [keysNum=61] Most pianos will have 61 keys.
     * @param [referenceKey=33] Key index for the pitchFork reference (A4 is the default).
     * @param [pitchFork=440.0] A4 is 440 Hz by default.
     * @param [tolerance=1.0] frequency tolerance, range (0.0, 1.0].
     * @memberof PianoTuning
     */
    PianoTuning(
      const unsigned sampleRate_,
      const unsigned keysNum = 61,
      const unsigned referenceKey_ = 33,
      const double pitchFork_ = 440.0,
      const double tolerance_ = 1.
    ) : Tuning{ sampleRate_, keysNum }, referenceKey(referenceKey_), pitchFork(pitchFork_), tolerance(tolerance_)
    {}

    /**
     * Converts the piano key number to it's fundamental frequency.
     *
     * @see https://en.wikipedia.org/wiki/Piano_key_frequencies
     * @param key
     * @return frequency
     * @memberof PianoTuning
     */
    double keyToFreq(const double key) {
      return pitchFork * std::pow(2., (key - referenceKey) / 12.);
    }

    /**
     * Computes the array of tuningValues structs that specify the frequencies to analyze.
     *
     * @memberof PianoTuning
     */
    const std::vector<tuningValues> mapping() {
      std::vector<tuningValues> output;
      output.reserve(bands);
      for (unsigned key = 0; key < bands; key++) {
        const double frequency = keyToFreq(key);
        const double bandwidth = 2. * (keyToFreq(key + .5 * tolerance) - frequency);
        output.push_back(frequencyAndBandwidthToKAndN(frequency, bandwidth));
      }
      return output;
    }
};

/**
 * Sliding Discrete Fourier Transform implementation for (westerns) musical frequencies.
 *
 * @see https://www.comm.utoronto.ca/~dimitris/ece431/slidingdft.pdf
 * @class SlidingDFT
 * @par EXAMPLE
 * // a common sample rate
 * auto tuning = PianoTuning(44100);
 * // no moving average
 * auto slidingDFT = SlidingDFT(tuning);
 * auto input = make_unique<float[]>(128);
 * // fill the input buffer with the samples
 * float *output = nullptr;
 * // just process; no moving average
 * output = slidingDFT.process(input);
 */
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

    /**
     * Creates an instance of SlidingDFT.
     * @param tuning Tuning instance (a class derived from Tuning; for instance, PianoTuning).
     * @param [maxAverageWindowInSeconds=0] Positive values are passed to MovingAverage implementation; negative values trigger FastMovingAverage implementation. Zero disables averaging.
     * @memberof SlidingDFT
     */
    SlidingDFT(const std::shared_ptr<Tuning> tuning, const double maxAverageWindowInSeconds = 0.) {
      sampleRate = tuning->sampleRate;
      bands = tuning->bands;
      bins.reserve(bands);
      levels.reserve(bands);

      unsigned maxN = 0;
      for (auto band : tuning->mapping()) {
        bins.push_back(std::make_shared<DFTBin>(band.k, band.N));
        maxN = std::max(maxN, band.N);
      }

      ringBuffer = std::make_unique<RingBuffer>(maxN);

#ifndef DISABLE_MOVING_AVERAGE
      if (maxAverageWindowInSeconds > 0.) {
        movingAverage = std::make_shared<HeavyMovingAverage>(
          bands,
          sampleRate,
          std::round(sampleRate * maxAverageWindowInSeconds)
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

    /**
     * Process a batch of samples.
     *
     * @param samples Array with the batch of samples to process. Value range is irrelevant (can be from -1.0 to 1.0 or 0 to 255 or whatever, as long as it is consistent).
     * @param [averageWindowInSeconds=0] Adjust the moving average window size.
     * @return Snapshot of the *squared* levels after processing all the samples. Value range is between 0.0 and 1.0. Depending on the application, you might need sqrt() of the level values (for visualization purposes it is actually better as is).
     * @memberof SlidingDFT
     */
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
