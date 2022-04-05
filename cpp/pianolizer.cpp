#include "emscripten/bind.h"
#include "pianolizer.hpp"

using namespace emscripten;

class Pianolizer {
  private:
    std::shared_ptr<Tuning> tuning;
    std::unique_ptr<SlidingDFT> slidingDFT;

  public:
    Pianolizer(unsigned sampleRate) {
      tuning = std::make_shared<PianoTuning>(sampleRate);
      slidingDFT = std::make_unique<SlidingDFT>(tuning, -1.);
    }

    val process(const uintptr_t samplesPtr, const unsigned samplesLength, const number averageWindowInSeconds = 0.) {
      auto samples = reinterpret_cast<number*>(samplesPtr);
      auto levels = slidingDFT->process(samples, samplesLength, averageWindowInSeconds);
      return val(typed_memory_view(tuning->bands, levels));
    }
};

EMSCRIPTEN_BINDINGS(CLASS_Pianolizer) {
  class_<Pianolizer>("Pianolizer")
      .constructor<unsigned>()
      .function("process", &Pianolizer::process, allow_raw_pointers());
}