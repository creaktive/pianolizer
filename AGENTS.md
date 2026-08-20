# Pianolizer — Agent Instructions

## Build & Run

- `make` — builds native binary (`pianolizer`) and WASM target (`js/pianolizer-wasm.js`)
- `make pianolizer` — native only (g++, C++17, `-O3 -flto`, strips binary)
- `make emscripten` — WASM only (requires Emscripten SDK; uses `--bind`, `SINGLE_FILE=1`)
- `make test` — compiles and runs `cpp/test.cpp`; **requires GoogleTest installed** (`-lgtest -lgtest_main`)
- `make clean` — removes all build artifacts

No npm scripts. JS is ES6 module (`"type": "module"` in package.json). Lint: `npx eslint . --ext .js,.html`.

## Architecture

- **C++ core** — header-only library at `cpp/pianolizer.hpp`. Include it; no separate compilation needed for library use.
- **CLI binary** — `cpp/main.cpp`: reads 32-bit float PCM from stdin, emits hex strings to stdout (61 piano keys C2–C7).
- **WASM** — compiled via Emscripten with `js/pianolizer-wrapper.js` as `--post-js`. Output: `js/pianolizer-wasm.js`.
- **Pure JS fallback** — `js/pianolizer.js`: port of the C++ algorithm, no WASM needed. Toggle via URL param `?purejs=1`.
- **Browser app** — `index.html` + `js/app.js`. Uses Web Audio API `AudioWorkletProcessor` (`js/pianolizer-worklet.js`). Canvas/SVG for visualization (`js/visualization.js`).
- **Hardware scripts** — `misc/hex2ws281x.py` drives WS2812B LEDs on Raspberry Pi (requires `rpi_ws281x`).

## Gotchas

- Browser app needs a server (no `file://` for AudioWorklet). Chrome recommended; Safari performance is poor.
- WASM build requires Emscripten (`em++`). Native build requires g++ and GoogleTest (for tests).
- CFLAGS include `-Werror` — all warnings are errors.
- JS uses ES6 modules throughout; no bundler, no transpilation.
