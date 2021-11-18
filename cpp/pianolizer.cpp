#include "emscripten/bind.h"

using namespace emscripten;

const unsigned KEYS = 61;

class Pianolizer {
  public:
    Pianolizer(unsigned sampleRate) {
    }

    val process(uintptr_t samplesInput, unsigned samplesLength, int averageWindowInSeconds = 0) {
      float* samples = reinterpret_cast<float*>(samplesInput);

      for (unsigned i = 0; i < samplesLength; i++) {
        levels[0] += samples[i];
      }
      levels[0] /= samplesLength;

      return val(typed_memory_view(sizeof(levels) / sizeof(float), levels));
    }
  
  private:
    float levels[KEYS];
};

EMSCRIPTEN_BINDINGS(CLASS_Pianolizer) {
  class_<Pianolizer>("Pianolizer")
      .constructor<unsigned>()
      .function("process", &Pianolizer::process, allow_raw_pointers());
}