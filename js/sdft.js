class Complex {
  constructor (re = 0, im = 0) {
    this.re = re
    this.im = im
  }

  add (z) {
    return new Complex(
      this.re + z.re,
      this.im + z.im
    )
  }

  sub (z) {
    return new Complex(
      this.re - z.re,
      this.im - z.im
    )
  }

  mul (z) {
    return new Complex(
      this.re * z.re - this.im * z.im,
      this.re * z.im + this.im * z.re
    )
  }

  exp () {
    const tmp = Math.exp(this.re)
    return new Complex(
      tmp * Math.cos(this.im),
      tmp * Math.sin(this.im)
    )
  }

  get magnitude () {
    return Math.sqrt(
      this.re * this.re +
      this.im * this.im
    )
  }
}

class RingBuffer {
  constructor (bits = 16) {
    const size = 1 << bits
    this.mask = size - 1
    this.buffer = new Float64Array(size)
    this.index = 0
  }

  write (value) {
    this.buffer[(this.index++) & this.mask] = value
  }

  read (index) {
    return this.buffer[(this.index + (~index)) & this.mask]
  }
}

class DFTBin {
  constructor (k, N) {
    this.k = k
    this.N = N
    this.bands = N / 2
    this.coeff = (new Complex(0, 2 * Math.PI * (k / N))).exp()
    this.dft = new Complex()
    this.totalPower = 0
  }

  update (previousSample, currentSample) {
    this.totalPower -= previousSample * previousSample
    this.totalPower += currentSample * currentSample

    const previousComplexSample = new Complex(previousSample, 0)
    const currentComplexSample = new Complex(currentSample, 0)

    this.dft = this.dft
      .sub(previousComplexSample)
      .add(currentComplexSample)
      .mul(this.coeff)
  }

  get level () {
    return (this.dft.magnitude / this.bands) / Math.sqrt(this.totalPower / this.bands)
  }
}

class SlidingDFT extends AudioWorkletProcessor {
  /* global currentTime, sampleRate */
  constructor () {
    super()

    this.ringBuffer = new RingBuffer()

    this.updateInterval = 1.0 / 60 // to be rendered at 60fps
    this.nextUpdateFrame = 0

    this.pitchFork = 440 // A4 is 440 Hz
    this.bins = new Array(88)
    for (let key = 0; key < this.bins.length; key++) {
      const freq = this.keyToFreq(key)
      const bandwidth = 2 * (this.keyToFreq(key + 0.5) - freq)
      let N = Math.ceil(sampleRate / bandwidth)
      const k = Math.ceil(freq / bandwidth)

      let newFreq
      do {
        newFreq = sampleRate * (k / N++)
      } while (newFreq - freq > 0)

      this.bins[key] = new DFTBin(k, N)
    }

    this.levels = new Float64Array(this.bins.length)
  }

  keyToFreq (key) {
    // https://en.wikipedia.org/wiki/Piano_key_frequencies
    return this.pitchFork * Math.pow(2, (key - 48) / 12)
  }

  process (input, output, parameters) {
    // if no inputs are connected then zero channels will be passed in
    if (input[0].length === 0) {
      return true
    }

    // I hope all the channels have the same # of samples,
    // and that it stays constand during the lifetime of the worklet
    const windowSize = input[0][0].length
    if (this.samples === undefined) {
      this.samples = new Float64Array(windowSize)
    }

    // mix down the inputs into single array
    const inputPortCount = input.length
    for (let portIndex = 0; portIndex < inputPortCount; portIndex++) {
      const channelCount = input[portIndex].length
      for (let channelIndex = 0; channelIndex < channelCount; channelIndex++) {
        for (let sampleIndex = 0; sampleIndex < windowSize; sampleIndex++) {
          const sample = input[portIndex][channelIndex][sampleIndex]
          output[portIndex][channelIndex][sampleIndex] = sample
          this.samples[sampleIndex] += sample
        }
      }
    }

    // store in the ring buffer
    for (let i = 0; i < windowSize; i++) {
      const currentSample = this.samples[i]
      this.samples[i] = 0
      this.ringBuffer.write(currentSample)

      for (const bin of this.bins) {
        const previousSample = this.ringBuffer.read(bin.N)
        bin.update(previousSample, currentSample)
      }
    }

    // snapshot of the levels
    for (let i = 0; i < this.bins.length; i++) {
      this.levels[i] = this.bins[i].level
    }

    // update and sync the levels property with the main thread.
    if (this.nextUpdateFrame <= currentTime) {
      this.nextUpdateFrame = currentTime + this.updateInterval
      this.port.postMessage({ levels: this.levels })
    }

    return true
  }
}

registerProcessor('sliding-dft-node', SlidingDFT)
