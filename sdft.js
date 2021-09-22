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
    if (z.im === 0 && this.im === 0) {
      return new Complex(this.re * z.re, 0)
    }

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

class SlidingDFT extends AudioWorkletProcessor {
  // constructor () {
  //   super()
  // }

  static get parameterDescriptors () {
    return [{
      name: 'amount',
      defaultValue: 0.5,
      minValue: 0,
      maxValue: 10,
      automationRate: 'k-rate'
    }]
  }

  process (input, output, parameters) {
    // `input` is an array of input ports, each having multiple channels.
    // For each channel of each input port, a Float32Array holds the audio
    // input data.
    // `output` is an array of output ports, each having multiple channels.
    // For each channel of each output port, a Float32Array must be filled
    // to output data.
    // `parameters` is an object having a property for each parameter
    // describing its value over time.
    const amount = parameters.amount[0]
    // const inputPortCount = input.length
    for (let portIndex = 0; portIndex < input.length; portIndex++) {
      const channelCount = input[portIndex].length
      for (let channelIndex = 0; channelIndex < channelCount; channelIndex++) {
        const sampleCount = input[portIndex][channelIndex].length
        for (let sampleIndex = 0; sampleIndex < sampleCount; sampleIndex++) {
          output[0][channelIndex][sampleIndex] =
            Math.tanh(amount * input[portIndex][channelIndex][sampleIndex])
        }
      }
    }
    return true
  }
}

registerProcessor('sliding-dft-node', SlidingDFT)
