# Pianolizer

## tl;dr

- [Pianolizer app](https://sysd.org/pianolizer/) - runs directly in the browser. Also, mobile browser. Chrome is recommended for the best experience.
- [Algorithm benchmark](https://sysd.org/pianolizer/benchmark.html) - test the speed of the core algorithm, in the browser. WASM results closely match what is expected from the native binary performance, on the same machine. 44100 samples per second is enough for realtime performance. As a matter of fact, one can get decent results with as little as 4000 samples per second (saving both CPU & RAM resources!); however the resampling algorithm implementation is left as an exercise to the reader (that is, for the JS/browser; the `pianolizer` CLI utility is meant to be paired with [sox](https://linux.die.net/man/1/sox)).

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

## Influenced & inspired by
- [Speaking Piano - Now with (somewhat decent) captions!](https://youtu.be/muCPjK4nGY4) - YouTube video.
- [Rousseau](https://www.youtube.com/c/Rousseau) - YouTube channel.
- [Piano LED Visualizer](https://github.com/onlaj/Piano-LED-Visualizer) - GitHub repository.
- [Music Transcription with Convolutional Neural Networks (Jun 2016)](https://www.lunaverus.com/cnn) - the method used by AnthemScore (the automatic music transcription app).

## References

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