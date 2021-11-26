#include "emscripten/bind.h"
#include "pianolizer.hpp"

using namespace emscripten;

class Pianolizer {
  private:
    SlidingDFT* slidingDFT;
    unsigned bands;

  public:
    Pianolizer(unsigned sampleRate) {
      PianoTuning tuning = PianoTuning(sampleRate);
      bands = tuning.bands;
      slidingDFT = new SlidingDFT(tuning);
    }

    ~Pianolizer() {
      delete slidingDFT;
    }

    val process(uintptr_t samplesPtr, unsigned samplesLength, float averageWindowInSeconds = 0) {
      float* samples = reinterpret_cast<float*>(samplesPtr);
      float* levels = slidingDFT->process(samples, samplesLength, averageWindowInSeconds);
      return val(typed_memory_view(bands, levels));
    }
};

EMSCRIPTEN_BINDINGS(CLASS_Pianolizer) {
  class_<Pianolizer>("Pianolizer")
      .constructor<unsigned>()
      .function("process", &Pianolizer::process, allow_raw_pointers());
}