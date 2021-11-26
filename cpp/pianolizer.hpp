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
    double k, N;
    double totalPower = 0.;
    std::complex<double> coeff;
    std::complex<double> dft = std::complex<double>(0., 0.);

  public:
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
};

class FastMovingAverage : public MovingAverage {
  public:
    FastMovingAverage(unsigned channels_, unsigned sampleRate_)
      : MovingAverage{ channels_, sampleRate_ }
    {}

    void update(double levels[]) {
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

    void update(double levels[]) {
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