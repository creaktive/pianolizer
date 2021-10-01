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
    this.buffer = new Float32Array(size)
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
  constructor () {
    super()

    this.ringBuffer = new RingBuffer()

    this.updateIntervalInMS = 1000 / 60 // to be rendered at 60fps
    this.nextUpdateFrame = this.updateIntervalInMS

    // eslint-disable-next-line no-undef
    this.sampleRate = sampleRate

    this.bins = new Array(88)
    for (let i = 0; i < this.bins.length; i++) {
      // https://en.wikipedia.org/wiki/Piano_key_frequencies
      const k = 440 * Math.pow(2, (i - 48) / 12)
      this.bins[i] = new DFTBin(k, this.sampleRate)
    }

    this.levels = new Float32Array(this.bins.length)
  }

  get intervalInFrames () {
    return this.updateIntervalInMS / 1000 * this.sampleRate
  }

  process (input, output, parameters) {
    // I hope all the channels have the same # of samples
    const windowSize = input[0][0].length

    // mix down the inputs into single array
    const samples = new Float32Array(windowSize)
    let count = 0
    const inputPortCount = input.length
    for (let portIndex = 0; portIndex < inputPortCount; portIndex++) {
      const channelCount = input[portIndex].length
      for (let channelIndex = 0; channelIndex < channelCount; channelIndex++) {
        for (let sampleIndex = 0; sampleIndex < windowSize; sampleIndex++) {
          const sample = input[portIndex][channelIndex][sampleIndex]
          output[portIndex][channelIndex][sampleIndex] = sample
          samples[sampleIndex] += sample
          count++
        }
      }
    }

    // normalize & store in the ring buffer
    const n = count / windowSize
    for (let i = 0; i < windowSize; i++) {
      const currentSample = samples[i] / n
      this.ringBuffer.write(currentSample)

      for (const bin of this.bins) {
        const previousSample = this.ringBuffer.read(bin.N)
        bin.update(previousSample, currentSample)
      }
    }

    for (let i = 0; i < this.bins.length; i++) {
      this.levels[i] = this.bins[i].level
    }

    // Update and sync the volume property with the main thread.
    this.nextUpdateFrame -= windowSize
    if (this.nextUpdateFrame < 0) {
      this.nextUpdateFrame += this.intervalInFrames
      this.port.postMessage({ levels: this.levels })
    }

    return true
  }
}

registerProcessor('sliding-dft-node', SlidingDFT)
