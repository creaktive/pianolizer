#include "emscripten/bind.h"
#include "pianolizer.hpp"

using namespace emscripten;

class Pianolizer {
  private:
    std::shared_ptr<Tuning> tuning;
    std::unique_ptr<SlidingDFT> slidingDFT;

  public:
    Pianolizer(
      const unsigned sampleRate,
      const unsigned keysNum = 61,
      const unsigned referenceKey = 33,
      const double pitchFork = 440.0,
      const double tolerance = 1.
    ) {
      tuning = std::make_shared<PianoTuning>(
        sampleRate,
        keysNum,
        referenceKey,
        pitchFork,
        tolerance
      );
      slidingDFT = std::make_unique<SlidingDFT>(tuning, -1.);
    }

    val process(const uintptr_t samplesPtr, const unsigned samplesLength, const double averageWindowInSeconds = 0.) {
      auto samples = reinterpret_cast<float*>(samplesPtr);
      auto levels = slidingDFT->process(samples, samplesLength, averageWindowInSeconds);
      return val(typed_memory_view(tuning->bands, levels));
    }
};

EMSCRIPTEN_BINDINGS(CLASS_Pianolizer) {
  class_<Pianolizer>("Pianolizer")
      .constructor<
        const unsigned,
        const unsigned,
        const unsigned,
        const double,
        const double
      >()
      .function("process", &Pianolizer::process, allow_raw_pointers());
}