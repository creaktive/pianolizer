#include <cmath>

using namespace std;

class RingBuffer {
  private:
    unsigned mask;
    unsigned index;
    float* buffer;

  public:
    RingBuffer(unsigned requestedSize) {
      const unsigned bits = ceil(log2(requestedSize + 1));
      const unsigned size = 1 << bits;
      mask = size - 1;
      buffer = new float[size];
      index = 0;
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