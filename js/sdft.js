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
  constructor (requestedSize) {
    const bits = Math.ceil(Math.log2(requestedSize + 1)) | 0
    // console.info(`Allocating RingBuffer for ${bits} address bits`)

    this.size = 1 << bits
    this.mask = this.size - 1
    this.buffer = new Float64Array(this.size)
    this.index = 0 // WARNING: overflows after ~6472 years of continuous operation!
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
    this.totalPower += currentSample * currentSample
    this.totalPower -= previousSample * previousSample

    const previousComplexSample = new Complex(previousSample, 0)
    const currentComplexSample = new Complex(currentSample, 0)

    this.dft = this.dft
      .sub(previousComplexSample)
      .add(currentComplexSample)
      .mul(this.coeff)
  }

  get level () {
    const level = (this.dft.magnitude / this.bands) / Math.sqrt(this.totalPower / this.bands)
    return level <= 1 ? level : 0
  }
}

// moving average of the output (effectively a low-pass to get the general envelope)
class MovingAverage {
  constructor (channels, sampleRate, maxWindow = sampleRate) {
    this.channels = channels
    this.sampleRate = sampleRate

    this.history = new Array(channels)
    this.sum = new Float64Array(channels)
    for (let n = 0; n < channels; n++) {
      this.history[n] = new RingBuffer(maxWindow)
    }
  }

  get windowLengthInSeconds () {
    return this.averageWindow / this.sampleRate
  }

  set windowLengthInSeconds (value) {
    this.targetAverageWindow = Math.round(value * this.sampleRate)
    if (this.averageWindow === undefined) {
      this.averageWindow = this.targetAverageWindow
    }
  }

  update (levels) {
    const keysNum = levels.length
    for (let key = 0; key < keysNum; key++) {
      const value = levels[key]
      this.history[key].write(value)
      this.sum[key] += value

      if (this.targetAverageWindow === this.averageWindow) {
        this.sum[key] -= this.history[key].read(this.averageWindow)
      } else if (this.targetAverageWindow < this.averageWindow) {
        this.sum[key] -= this.history[key].read(this.averageWindow)
        this.sum[key] -= this.history[key].read(this.averageWindow - 1)
      }
    }

    if (this.targetAverageWindow > this.averageWindow) {
      this.averageWindow++
    } else if (this.targetAverageWindow < this.averageWindow) {
      this.averageWindow--
    }
  }

  read (n) {
    return this.sum[n] / this.averageWindow
  }
}

class SlidingDFT extends AudioWorkletProcessor {
  /* global currentTime, sampleRate */
  constructor () {
    super()

    this.updateInterval = 1.0 / 60 // to be rendered at 60fps
    this.nextUpdateFrame = 0

    this.pitchFork = 440.0 // A4 is 440 Hz
    this.binsNum = 88
    this.bins = new Array(this.binsNum)
    this.levels = new Float64Array(this.binsNum)

    let maxN = 0
    for (let key = 0; key < this.binsNum; key++) {
      const freq = this.keyToFreq(key)
      const bandwidth = 2 * (this.keyToFreq(key + 0.5) - freq)
      let N = Math.ceil(sampleRate / bandwidth)
      const k = Math.ceil(freq / bandwidth)

      let newFreq
      do {
        newFreq = sampleRate * (k / N++)
      } while (newFreq - freq > 0)

      this.bins[key] = new DFTBin(k, N)
      maxN = Math.max(maxN, N)
    }

    this.ringBuffer = new RingBuffer(maxN)
    this.movingAverage = new MovingAverage(this.binsNum, sampleRate, sampleRate * 0.25)
  }

  static get parameterDescriptors () {
    return [{
      name: 'smooth',
      defaultValue: 0.05,
      minValue: 0,
      maxValue: 0.25,
      automationRate: 'k-rate'
    }]
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

    // I hope all the channels have the same # of samples; but 128 frames per block
    // is subject to change:
    // https://developer.mozilla.org/en-US/docs/Web/API/AudioWorkletProcessor/process
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

    this.movingAverage.windowLengthInSeconds = parameters.smooth[0]

    // store in the ring buffer
    for (let i = 0; i < windowSize; i++) {
      const currentSample = this.samples[i]
      this.samples[i] = 0
      this.ringBuffer.write(currentSample)

      for (let key = 0; key < this.binsNum; key++) {
        const bin = this.bins[key]
        const previousSample = this.ringBuffer.read(bin.N)
        bin.update(previousSample, currentSample)
        this.levels[key] = bin.level
      }

      this.movingAverage.update(this.levels)
    }

    // update and sync the levels property with the main thread.
    if (this.nextUpdateFrame <= currentTime) {
      this.nextUpdateFrame = currentTime + this.updateInterval

      // snapshot of the levels
      if (this.movingAverage.averageWindow > 0) {
        for (let key = 0; key < this.binsNum; key++) {
          this.levels[key] = this.movingAverage.read(key)
        }
      }
      this.port.postMessage(this.levels)
    }

    return true
  }
}

registerProcessor('sliding-dft-node', SlidingDFT)
