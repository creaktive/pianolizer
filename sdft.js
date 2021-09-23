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

class SlidingDFT extends AudioWorkletProcessor {
  constructor () {
    super()

    this.ringBuffer = new RingBuffer()

    this.k = 440
    // eslint-disable-next-line no-undef
    this.N = sampleRate
    this.coeff = (new Complex(0, 2 * Math.PI * (this.k / this.N))).exp()
    this.dft = new Complex()

    // console.log(this)
  }

  process (input, output, parameters) {
    // mix down the inputs into single array
    const samples = new Float32Array(128)
    let count = 0
    const inputPortCount = input.length
    for (let portIndex = 0; portIndex < inputPortCount; portIndex++) {
      const channelCount = input[portIndex].length
      for (let channelIndex = 0; channelIndex < channelCount; channelIndex++) {
        const sampleCount = input[portIndex][channelIndex].length
        for (let sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++) {
          const sample = input[portIndex][channelIndex][sampleIndex]
          output[portIndex][channelIndex][sampleIndex] = sample
          samples[sampleIndex] += sample
          count++
        }
      }
    }

    // normalize & store in the ring buffer
    for (let i = 0; i < samples.length; i++) {
      const sample = new Complex(samples[i] / count, 0)
      this.ringBuffer.write(sample.re)
      const previousSample = new Complex(this.ringBuffer.read(this.N), 0)

      this.dft = this.dft
        .sub(previousSample)
        .add(sample)
        .mul(this.coeff)
    }

    return true
  }
}

registerProcessor('sliding-dft-node', SlidingDFT)
