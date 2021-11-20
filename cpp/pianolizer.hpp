#include <cmath>
#include <complex>
#include <vector>

using namespace std;

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
    }

    ~RingBuffer() {
      free(buffer);
    }

    void write(float value) {
      index &= mask;
      buffer[index++] = value;
    }

    float read(unsigned position) {
      return buffer[(index + (~position)) & mask];
    }
};

class DFTBin {
  private:
    const double PI = acos(-1.);
    const double SQRT2 = sqrt(2.);
    float k, N;
    float totalPower = 0.;
    complex<float> coeff;
    complex<float> dft = complex<float>(0., 0.);

  public:
    float referenceAmplitude = 1.; // 0 dB level

    DFTBin(unsigned k_, unsigned N_) {
      if (k_ == 0) {
        throw invalid_argument("k=0 (DC) not implemented");
      } else if (N_ == 0) {
        throw invalid_argument("N=0 is soooo not supported (Y THO?)");
      }

      k = k_;
      N = N_;
      coeff = exp(complex<float>(0., 2. * PI * (k / N)));
    }

    void update(float previousSample, float currentSample) {
      totalPower += currentSample * currentSample;
      totalPower -= previousSample * previousSample;

      dft = coeff * ((dft - complex<float>(previousSample, 0.)) + complex<float>(currentSample, 0.));
    }

    float rms() {
      return sqrt(totalPower / N);
    }

    float amplitudeSpectrum() {
      return SQRT2 * (sqrt(norm(dft)) / N);
    }

    float normalizedAmplitudeSpectrum() {
      return totalPower > 0.
        ? amplitudeSpectrum() / rms()
        : 0.;
    }

    float logarithmicUnitDecibels() {
      return 20. * log10(amplitudeSpectrum() / referenceAmplitude);
    }
};

class MovingAverage {
  public:
    unsigned channels, sampleRate;
    int averageWindow = -1;
    unsigned targetAverageWindow;
    float *sum;

    MovingAverage(unsigned channels_, unsigned sampleRate_) {
      channels = channels_;
      sampleRate = sampleRate_;
      sum = new float[channels];
    }

    ~MovingAverage() {
      free(sum);
    }

    float averageWindowInSeconds() {
      return averageWindow / sampleRate;
    }

    void averageWindowInSeconds(float value) {
      targetAverageWindow = round(value * sampleRate);
      if (averageWindow == -1) {
        averageWindow = targetAverageWindow;
      }
    }

    void updateAverageWindow() {
      if (targetAverageWindow > averageWindow) {
        averageWindow++;
      } else if (targetAverageWindow < averageWindow) {
        averageWindow--;
      }
    }

    float read(unsigned n) {
      return sum[n] / averageWindow;
    }
};

class FastMovingAverage : public MovingAverage {
  public:
    void update(float levels[]) {
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
    vector<RingBuffer> history;

  private:
    HeavyMovingAverage(unsigned channels_, unsigned sampleRate_, unsigned maxWindow = 0)
      : MovingAverage{ channels_, sampleRate_ }
    {
      for (unsigned n = 0; n < channels; n++) {
        const RingBuffer *ringBuffer = new RingBuffer(maxWindow ? maxWindow : sampleRate);
        history.push_back(*ringBuffer);
      }
    }

    void update(float levels[]) {
      for (unsigned n = 0; n < channels; n++) {
        const float value = levels[n];
        history[n].write(value);
        sum[n] += value;

        if (targetAverageWindow == averageWindow) {
          sum[n] -= history[n].read(averageWindow);
        } else if (targetAverageWindow < averageWindow) {
          sum[n] -= history[n].read(averageWindow);
          sum[n] -= history[n].read(averageWindow - 1);
        }
      }
      updateAverageWindow();
    }
};