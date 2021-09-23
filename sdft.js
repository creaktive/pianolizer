class Complex {
  constructor (re, im) {
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
}

class RingBuffer {
  constructor (bits = 16) {
    this.size = 1 << bits
    this.mask = this.size - 1
    this.buffer = new Float32Array(this.size)
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

    // console.log(this)
  }

  // static get parameterDescriptors () {
  //   return [{
  //     name: 'amount',
  //     defaultValue: 0.5,
  //     minValue: 0,
  //     maxValue: 10,
  //     automationRate: 'k-rate'
  //   }]
  // }

  process (input, output, parameters) {
    // `input` is an array of input ports, each having multiple channels.
    // For each channel of each input port, a Float32Array holds the audio
    // input data.
    // `output` is an array of output ports, each having multiple channels.
    // For each channel of each output port, a Float32Array must be filled
    // to output data.
    // `parameters` is an object having a property for each parameter
    // describing its value over time.
    // const amount = parameters.amount[0]

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
      this.ringBuffer.write(samples[i] /= count)
    }

    return true
  }
}

registerProcessor('sliding-dft-node', SlidingDFT)
