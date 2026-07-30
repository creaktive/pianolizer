# Pianolizer - Agent Guidelines

## Project Overview

Pianolizer is a real-time music spectral analysis library using the **Sliding Discrete Fourier Transform (SDFT)** algorithm. It detects 61 piano keys (C2–C7) from audio input and outputs normalized amplitude values in the range `[0.0, 1.0]`.

**Tech stack:** C++14 header-only core library, JavaScript/ES6 browser app, WebAssembly via Emscripten, Python/Perl utility scripts.

## Directory Structure

| Path | Purpose |
|------|---------|
| `cpp/pianolizer.hpp` | Core header-only library — all logic in one file |
| `cpp/main.cpp` | CLI binary (`pianolizer`) — reads raw audio, outputs hex strings |
| `cpp/test.cpp` | GoogleTest unit tests |
| `cpp/benchmark.cpp` | Performance benchmarking |
| `js/pianolizer.js` | JavaScript port of the core algorithm |
| `js/pianolizer-wasm.js` | Emscripten-compiled WASM bundle (single file) |
| `js/pianolizer-worklet.js` | AudioWorkletProcessor layer for real-time browser audio |
| `js/visualization.js` | Canvas-based keyboard & spectrogram renderer |
| `js/app.js` | Main browser app entry point |
| `misc/*.py` | Raspberry Pi LED drivers, MIDI converters |
| `misc/*.pl` | Perl utilities (spectrum generation, MIDI transcription) |

## Build Commands

```bash
make                    # native binary + WASM
make pianolizer         # native CLI only
make emscripten         # WASM only
make test               # run GoogleTest suite (requires gtest)
make clean              # remove all compiled artifacts
```

**Compiler flags:** `-ffast-math -flto -std=c++14 -pedantic -Werror -Wall -Wextra -O3`

## Coding Conventions

### C++ (`cpp/`)
- **Header-only library**: `pianolizer.hpp` is the single source of truth — all classes defined inline
- **C++11 compatible** minimum, but C++14 recommended (includes `make_unique` polyfill)
- No external dependencies beyond standard library
- JSDoc-style `/** ... */` comments with `@class`, `@param`, `@return`, `@memberof`, `@par EXAMPLE`
- Classes: `RingBuffer`, `DFTBin`, `MovingAverage`/`FastMovingAverage`/`HeavyMovingAverage`, `Tuning`/`PianoTuning`, `SlidingDFT`
- Use `std::vector`, `std::unique_ptr`, `std::shared_ptr` — no raw pointers in library code

### JavaScript (`js/`)
- ES6 modules (`export default class`, named exports)
- Linting: ESLint with `eslint-config-standard` (see `package.json`)
- Run linting: `npx standard js/` or `npx eslint js/`
- Parallel C++ implementation with `Complex`, `PianoTuning`, `SlidingDFT`, `Pianolizer` wrapper classes

### Python (`misc/`)
- Utility scripts for Raspberry Pi, LED control, MIDI conversion
- No formal test suite — tested against real hardware

## Dual Implementations: C++ vs JavaScript

Both implementations are algorithmically identical but differ in several important ways. When modifying the library, changes should be mirrored across both unless explicitly scoped to one platform.

### Class-by-Class Comparison

| Class | C++ (`pianolizer.hpp`) | JS (`pianolizer.js`) |
|-------|----------------------|---------------------|
| **Complex** | `std::complex<double>` — in-place arithmetic, no allocations | Custom class with `.add()`/`.sub()`/`.mul()` returning new instances. Every DFT update allocates 2–3 temporary objects per bin. |
| **RingBuffer** | `std::vector<float>`, heap-allocated | `Float32Array` (typed array, contiguous memory). Both use power-of-two sizing with bitwise AND masking — identical algorithm. |
| **DFTBin** | In-place complex arithmetic via `std::complex`. Throws `std::invalid_argument` if k=0 or N=0. | Creates new `Complex` objects per update (major allocation overhead). Adds extra validation that k/N are integers. |
| **MovingAverage** | Virtual base class with pure virtual `update()`. `HeavyMovingAverage` stores `vector<unique_ptr<RingBuffer>>`. | Prototype inheritance, no virtual dispatch. Stores plain JS array of `RingBuffer` objects. Algorithmically identical. |
| **PianoTuning** | `mapping()` is a method returning `const vector<tuningValues>&` — computed once, cached in memory. | `mapping` is a getter that **recomputes the entire 61-element array on every access**. No caching. |
| **SlidingDFT** | Returns `const float*`. Input buffer untouched. Moving average controlled by `#ifndef DISABLE_MOVING_AVERAGE` compile flag. | Returns `Float32Array`. **Mutates input**: zeroes `samples[i]` after reading (side effect). Always includes moving average — no build-time stripping. |
| **Pianolizer** | Default: no moving average (`maxAverageWindowInSeconds = 0`). | Default: fast moving average enabled (`-1`). Minor behavioral difference in output smoothing. |

### Key Behavioral Differences

1. **Complex number allocation (major performance gap)** — JS creates new `Complex` objects on every DFT update; C++ does in-place arithmetic. This is the primary reason WASM (compiled C++) outperforms pure JS.

2. **PianoTuning.mapping recomputation** — JS getter recalculates all 61 key mappings on every read. In C++, `mapping()` returns a cached vector. Only matters if external code reads `.mapping` repeatedly.

3. **Input buffer mutation in JS** — `SlidingDFT.process()` zeroes the input array (`samples[i] = 0`) after reading each sample. C++ does not mutate its input. Callers reusing buffers may see unexpected side effects in pure JS mode.

4. **Default moving average** — JS always applies FastMovingAverage by default; C++ disables it. Output values will differ slightly between the two unless the caller explicitly sets `averageWindowInSeconds`.

5. **Extra validation in JS DFTBin** — JS validates that k and N are integers at construction time, catching errors earlier than C++.

### When to Modify Which

- **Algorithm changes**: Update both implementations simultaneously
- **Performance-critical paths**: Prefer WASM; pure JS has inherent allocation overhead from object creation per sample
- **New features**: Ensure behavioral parity — especially around input buffer mutation and default moving average settings
- **Tests**: C++ has GoogleTest suite (`make test`); JS has `js/test.js` but no formal runner

## Key Architecture

1. **PianoTuning** — maps piano key indices (0–60) to DFT parameters `(k, N)` pairs based on equal temperament tuning
2. **DFTBin** — single-frequency Sliding DFT filter with `update()`, `normalizedAmplitudeSpectrum()`
3. **SlidingDFT** — orchestrates multiple DFTBins over a ring buffer, applies optional moving average
4. Output: squared amplitude values in range `[0.0, 1.0]` per key

## Testing

- **C++**: Requires GoogleTest (`libgtest-dev`). Run: `make test` (compiles and executes automatically)
- **JS**: `js/test.js` exists but has no formal runner — execute via Node.js directly: `node js/test.js`

## Platform Notes

- **Browser**: `index.html` — drag-and-drop or file source, WASM or PureJS toggle
- **Raspberry Pi**: Use `misc/pianolizer.sh` + `misc/hex2ws281x.py` with ReSpeaker hat + WS2812B LEDs
- **Linux desktop**: Use `misc/hex2adalight.py` with Arduino running AdaLight sketch
- **CLI**: Pipe `arecord` or `ffmpeg` output into `./pianolizer`, pipe hex output to Python scripts

## License

MIT — Copyright (c) 2023 Stanislaw Pusep
