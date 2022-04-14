# Pianolizer

## Description

A simple and efficient algorithm for spectral analysis of music.
Examples for browser and Raspberry Pi are provided.

## tl;dr

- [Pianolizer app](https://sysd.org/pianolizer/) - runs directly in the browser. Also, mobile browser. Chrome is recommended for the best experience.
- [Algorithm benchmark](https://sysd.org/pianolizer/benchmark.html) - test the speed of the core algorithm, in the browser. WASM results closely match what is expected from the native binary performance, on the same machine. 44100 samples per second is enough for realtime performance. As a matter of fact, one can get decent results with as little as 8000 samples per second (saving both CPU & RAM resources!); however the resampling algorithm implementation is left as an exercise to the reader (that is, for the JS/browser; the `pianolizer` CLI utility is meant to be paired with [sox](http://sox.sourceforge.net)).

## Building

The [C++ implementation](cpp/pianolizer.hpp) should compile just fine on any platform that supports C++14, there are no dependencies as the code uses C++14 standard data types. 
It is known to compile & run successfully with [Clang](https://clang.llvm.org), [GCC](https://gcc.gnu.org) and [Emscripten](https://emscripten.org).
The target platform should support `double float` operations efficiently (in other words, hardware FPU is rather mandatory).

Compile the [native binary](cpp/main.cpp) (AKA the `pianolizer` CLI utility) _and_ to [WebAssembly](https://webassembly.org/):

```
make
```

Compile only the [native binary](cpp/main.cpp):

```
make pianolizer
```

To compile only to WebAssembly:

```
make emscripten
```

[Test and benchmark](cpp/test.cpp) the [C++ implementation](cpp/pianolizer.hpp) (**optional**; depends on [GoogleTest](https://github.com/google/googletest/)):

```
make test
```

Delete all the compiled files:

```
make clean
```

## Using the CLI utility

### General instructions

```
$ ./pianolizer -h
Usage:
        sox -V -d -traw -r44100 -b32 -c1 -efloat - | ./pianolizer | sudo misc/hex2ws281x.py

Options:
        -h      this
        -b      buffer size; default: 256
        -s      sample rate; default: 44100 (Hz)
        -p      A4 reference frequency; default: 440 (Hz)
        -a      average window (effectively a low-pass filter for the output); default: 0.04 (seconds; 0 to disable)

Description:
Consumes an audio stream (1 channel, 32-bit float PCM)
and emits the volume levels of 61 notes (from C2 to C7) as a hex string.
```

The `pianolizer` CLI utility receives an input stream of following specifications:

```
Channels       : 1
Sample Rate    : 44100
Sample Encoding: 32-bit Floating Point PCM
```

And emits a stream of 122-character hexadecimal strings representing the _volume level_ of the 61 consecutive notes (from C2 to C6):

```
...
000000000000000000010004020100000000000001010419110101010102182e040203192202040b080201010001000105010901060101000000000000
0000000000000000000100040201000000000000010104151301010101021634030202192202040a070101010001000104010801060101000000000000
00000000000000000000000402010000000000000101031215010101010215380302021922020309060101010001000104010701050101000000000000
000000000000000000000004030100000000000000010310160101010103163a0302021a20020308050101010001000104010601040101000000000000
00000000000000000000000404010000000000000001030e1601010102041839030202181f020307050101000001000103010601040101000000000000
...
```

Each level uses 2 hexadecimal characters (therefore, the value range is 0-255).
A new string is emitted every 256 samples (adjustable with `-b` option); that amounts to ~6ms of audio.

[sox](http://sox.sourceforge.net) is recommended to provide the input for `pianolizer`.
When using a microphone source, `sox` will insert tiny delays between the samples, so that the audio stream is synchronized with the sample rate.
When decoding an audio file, `sox` spits out the data as fast as possible.
With that, it should be trivial to convert the `pianolizer` output into a static spectrogram image.

### Raspberry Pi specific

The included [Python script](misc/hex2ws281x.py) consumes the hexadecimal output of `pianolizer` and drives a WS2812B LED strip (depends on the [rpi_ws281x library](https://github.com/rpi-ws281x/rpi-ws281x-python)). 
Conveniently, 1m LED strip with _144 diodes/meter_ matches precisely the standard piano keyboard dimensions and is enough to cover 61 keys.

Raspberry Pi has no audio input hardware at the time of writing, therefore an expansion board is required.
I am using [ReSpeaker 2-Mics Pi HAT by Seeed](https://www.seeedstudio.com/ReSpeaker-2-Mics-Pi-HAT.html).

Check [rc.local](misc/rc.local) for an example of how to drive the LED strip with a microphone.
You will probably need to adjust the sample rate and volume in this script, and also the I/O pin number in the [Python script](misc/hex2ws281x.py).

## Using the library

The main purpose of Pianolizer is _music visualization_.
Because of this, the volume level values are squared (more contrast, less CPU usage) and averaged (otherwise the output is unpleasant and potentially harmful to look at, due to flickering).
However, the library is modular by design, so you can shuffle things around and implement other stuff like [DTMF decoder](https://en.wikipedia.org/wiki/Dual-tone_multi-frequency_signaling) or even a [vocoder](https://en.wikipedia.org/wiki/Vocoder) (YMMV!).

### C++

Standard: C++14

- Include [pianolizer.hpp](cpp/pianolizer.hpp) in your project. It is reasonably well commented and documented and relevant examples are provided inline.
- [benchmark.cpp](cpp/benchmark.cpp) is a good starting point.
- [test.cpp](cpp/test.cpp) describes the expected behavior of the library.

### JavaScript & WebAssembly

Standard: ECMAScript 6

- Include [pianolizer.js](js/pianolizer.js) in your project. It is reasonably well commented and documented and relevant examples are provided inline.
- [benchmark.js](js/benchmark.js) is a good starting point. It runs directly via [Node.js](https://nodejs.org/). Also check [benchmark.html](benchmark.html), it works in the browser.
- [AudioWorkletProcessor](https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor) compatibility layer can be found in [pianolizer-worklet.js](js/pianolizer-worklet.js). Worklet is set up in [index.html](index.html).

## Influenced & inspired by
- [Speaking Piano - Now with (somewhat decent) captions!](https://youtu.be/muCPjK4nGY4) - YouTube video.
- [Rousseau](https://www.youtube.com/c/Rousseau) - YouTube channel.
- [Piano LED Visualizer](https://github.com/onlaj/Piano-LED-Visualizer) - GitHub repository.
- [Music Transcription with Convolutional Neural Networks (Jun 2016)](https://www.lunaverus.com/cnn) - the method used by [AnthemScore](https://www.lunaverus.com) (the automatic music transcription app).

## References

- [Chopin Mazurka in F minor, op. 68 no. 4](https://youtu.be/kfFyITaBpac) - music used in the demo; performance by [Pianotomy](https://www.youtube.com/channel/UCRS3ZdjDQZJ8p24MZjy_T_g).
- [Piano key frequencies](https://en.wikipedia.org/wiki/Piano_key_frequencies) - Wikipedia article.
- [The Fundamentals of FFT-Based Signal Analysis and Measurement](https://www.sjsu.edu/people/burford.furman/docs/me120/FFT_tutorial_NI.pdf) - tutorial by National Instruments.
- [Sliding DFT](https://en.wikipedia.org/wiki/Sliding_DFT) - Wikipedia article.
- [The Sliding DFT](http://www.comm.toronto.edu/~dimitris/ece431/slidingdft.pdf) - tutorial featuring GNU Octave/MATLAB implementation.
- [The sliding DFT](https://ieeexplore.ieee.org/document/1184347) - Published in: IEEE Signal Processing Magazine (Volume: 20, Issue: 2, March 2003).
- [Sliding Discrete Fourier Transform](http://dream.cs.bath.ac.uk/SDFT/) - Development & Research in Electro-Acoustic Media page.
- [Constant-Q transform](https://en.wikipedia.org/wiki/Constant-Q_transform) - Wikipedia article.
- [Computationally Efficient Moving Average for Microcontrollers](https://www.daycounter.com/LabBook/Moving-Average.phtml) - fast approximation of moving average.

## Copyright

MIT License

Copyright (c) 2022 Stanislaw Pusep