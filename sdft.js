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

    this.updateInterval = 1 / 60 // to be rendered at 60fps
    this.nextUpdateFrame = currentTime + this.updateInterval

    this.bandwidth = 5 // Hz
    this.N = sampleRate / this.bandwidth
    this.bins = new Array(88)
    for (let i = 0; i < this.bins.length; i++) {
      // https://en.wikipedia.org/wiki/Piano_key_frequencies
      const freq = 440 * Math.pow(2, (i - 48) / 12)
      const k = Math.round(freq / this.bandwidth)
      this.bins[i] = new DFTBin(k, this.N)
    }

    this.levels = new Float64Array(this.bins.length)
    this.samples = new Float64Array(512)
  }

  process (input, output, parameters) {
    // I hope all the channels have the same # of samples
    const windowSize = input[0][0].length

    // mix down the inputs into single array
    let count = 0
    const inputPortCount = input.length
    for (let portIndex = 0; portIndex < inputPortCount; portIndex++) {
      const channelCount = input[portIndex].length
      for (let channelIndex = 0; channelIndex < channelCount; channelIndex++) {
        for (let sampleIndex = 0; sampleIndex < windowSize; sampleIndex++) {
          const sample = input[portIndex][channelIndex][sampleIndex]
          output[portIndex][channelIndex][sampleIndex] = sample
          this.samples[sampleIndex] += sample
          count++
        }
      }
    }

    // normalize & store in the ring buffer
    const n = count / windowSize
    for (let i = 0; i < windowSize; i++) {
      const currentSample = this.samples[i] / n
      this.samples[i] = 0
      this.ringBuffer.write(currentSample)

      for (const bin of this.bins) {
        const previousSample = this.ringBuffer.read(bin.N)
        bin.update(previousSample, currentSample)
      }
    }

    // Update and sync the volume property with the main thread.
    if (this.nextUpdateFrame <= currentTime) {
      this.nextUpdateFrame = currentTime + this.updateInterval

      for (let i = 0; i < this.bins.length; i++) {
        this.levels[i] = this.bins[i].level
      }
      this.port.postMessage({ levels: this.levels })
    }

    return true
  }
}

registerProcessor('sliding-dft-node', SlidingDFT)
