/**
 * Convenience wrapper that ties together the rest of this library, with sane defaults.
 *
 * @export
 * @class Pianolizer
 */
export class Pianolizer {
  /**
   * Creates an instance of Pianolizer.
   * @param {Number} sampleRate in Hz
   * @memberof Pianolizer
   */
  constructor (sampleRate) {
    this.slidingDFT = new SlidingDFT(
      new PianoTuning(sampleRate),
      -1
    )
  }

  /**
   * Process a batch of samples.
   *
   * @param {Float32Array} samples Array with the batch of samples to process.
   * @param {Number} [averageWindowInSeconds=0] Adjust the moving average window size.
   * @return {Float32Array} Snapshot of the levels after processing all the samples.
   * @memberof Pianolizer
   */
  process (samples, averageWindowInSeconds = 0) {
    return this.slidingDFT.process(samples, averageWindowInSeconds)
  }
}

/**
 * Minimal implementation of Complex numbers required for the Discrete Fourier Transform computations.
 *
 * @class Complex
 */
export class Complex {
  /**
   * Creates an instance of Complex.
   * @param {Number} [re=0] Real part.
   * @param {Number} [im=0] Imaginary part.
   * @memberof Complex
   */
  constructor (re = 0, im = 0) {
    this.re = re
    this.im = im
  }

  /**
   * Addition.
   *
   * @param {Complex} z Complex number to add.
   * @return {Complex} Sum of the instance and z.
   * @memberof Complex
   */
  add (z) {
    return new Complex(
      this.re + z.re,
      this.im + z.im
    )
  }

  /**
   * Subtraction.
   *
   * @param {Complex} z Complex number to subtract.
   * @return {Complex} Sum of the instance and z.
   * @memberof Complex
   */
  sub (z) {
    return new Complex(
      this.re - z.re,
      this.im - z.im
    )
  }

  /**
   * Multiplication.
   *
   * @param {Complex} z Complex number to multiply.
   * @return {Complex} Product of the instance and z.
   * @memberof Complex
   */
  mul (z) {
    return new Complex(
      this.re * z.re - this.im * z.im,
      this.re * z.im + this.im * z.re
    )
  }

  /**
   * Exponential.
   *
   * @return {Complex} Exponential of the instance.
   * @memberof Complex
   */
  exp () {
    const tmp = Math.exp(this.re)
    return new Complex(
      tmp * Math.cos(this.im),
      tmp * Math.sin(this.im)
    )
  }

  /**
   * Magnitude.
   *
   * @readonly
   * @memberof Complex
   */
  get magnitude () {
    return Math.sqrt(
      this.re * this.re +
      this.im * this.im
    )
  }
}

/**
 * Reasonably fast Ring Buffer implementation.
 * Caveat: the size of the allocated memory is always a power of two!
 *
 * @class RingBuffer
 */
export class RingBuffer {
  /**
   * Creates an instance of RingBuffer.
   * @param {Number} requestedSize How long the RingBuffer is expected to be.
   * @memberof RingBuffer
   */
  constructor (requestedSize) {
    const bits = Math.ceil(Math.log2(requestedSize + 1)) | 0
    // console.info(`Allocating RingBuffer for ${bits} address bits`)

    const size = 1 << bits
    this.mask = size - 1
    this.buffer = new Float32Array(size)
    this.index = 0
  }

  /**
   * Shifts the RingBuffer and stores the value in the latest position.
   *
   * @param {Number} value Value to be stored in an Float32Array.
   * @memberof RingBuffer
   */
  write (value) {
    this.index &= this.mask
    this.buffer[this.index++] = value
  }

  /**
   * Retrieves the value stored at the position.
   *
   * @param {Number} position Position within the RingBuffer.
   * @return {Number} The value at the position.
   * @memberof RingBuffer
   */
  read (position) {
    return this.buffer[(this.index + (~position)) & this.mask]
  }
}

/**
 * Discrete Fourier Transform computation for one single bin.
 *
 * @class DFTBin
 */
export class DFTBin {
  /**
   * Creates an instance of DFTBin.
   * @param {Number} k Frequency divided by the bandwidth (must be an integer!).
   * @param {Number} N Sample rate divided by the bandwidth (must be an integer!).
   * @memberof DFTBin
   */
  constructor (k, N) {
    if (k === 0) {
      throw new RangeError('k=0 (DC) not implemented')
    } else if (N === 0) {
      throw new RangeError('N=0 is soooo not supported (Y THO?)')
    } else if (k !== Math.round(k)) {
      throw new RangeError('k must be an integer')
    } else if (N !== Math.round(N)) {
      throw new RangeError('N must be an integer')
    }

    this.k = k
    this.N = N
    this.coeff = (new Complex(0, 2 * Math.PI * (k / N))).exp()
    this.dft = new Complex()
    this.totalPower = 0.0
    this.referenceAmplitude = 1.0 // 0 dB level
  }

  /**
   * Do the Sliding DFT computation.
   *
   * @param {Number} previousSample Sample from N frames ago.
   * @param {Number} currentSample The latest sample.
   * @memberof DFTBin
   */
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

  /**
   * Root Mean Square.
   *
   * @readonly
   * @memberof DFTBin
   */
  get rms () {
    return Math.sqrt(this.totalPower / this.N)
  }

  /**
   * Amplitude spectrum in volts RMS.
   *
   * @see {@link https://www.sjsu.edu/people/burford.furman/docs/me120/FFT_tutorial_NI.pdf}
   * @readonly
   * @memberof DFTBin
   */
  get amplitudeSpectrum () {
    return Math.SQRT2 * (this.dft.magnitude / this.N)
  }

  /**
   * Normalized amplitude (always returns a value between 0.0 and 1.0).
   * This is well suited to detect pure tones, and can be used to decode DTMF or FSK modulation.
   *
   * @readonly
   * @memberof DFTBin
   */
  get normalizedAmplitudeSpectrum () {
    return this.totalPower > 0
      ? this.amplitudeSpectrum / this.rms
      : 0
  }

  /**
   * Using this unit of measure, it is easy to view wide dynamic ranges; that is,
   * it is easy to see small signal components in the presence of large ones.
   *
   * @readonly
   * @memberof DFTBin
   */
  get logarithmicUnitDecibels () {
    return 20 * Math.log10(this.amplitudeSpectrum / this.referenceAmplitude)
  }
}

/**
 * Base class for FastMovingAverage & HeavyMovingAverage. Must implement the update(levels) method.
 *
 * @class MovingAverage
 */
export class MovingAverage {
  /**
   * Creates an instance of MovingAverage.
   * @param {Number} channels Number of channels to process.
   * @param {Number} sampleRate Sample rate, used to convert between time and amount of samples.
   * @memberof MovingAverage
   */
  constructor (channels, sampleRate) {
    this.channels = channels
    this.sampleRate = sampleRate
    this.sum = new Float32Array(channels)
    this.averageWindow = null
  }

  /**
   * Get the current window size (in seconds).
   *
   * @memberof MovingAverage
   */
  get averageWindowInSeconds () {
    return this.averageWindow / this.sampleRate
  }

  /**
   * Set the current window size (in seconds).
   *
   * @memberof MovingAverage
   */
  set averageWindowInSeconds (value) {
    this.targetAverageWindow = Math.round(value * this.sampleRate)
    if (this.averageWindow === null) {
      this.averageWindow = this.targetAverageWindow
    }
  }

  /**
   * Adjust averageWindow in steps.
   *
   * @memberof MovingAverage
   */
  updateAverageWindow () {
    if (this.targetAverageWindow > this.averageWindow) {
      this.averageWindow++
    } else if (this.targetAverageWindow < this.averageWindow) {
      this.averageWindow--
    }
  }

  /**
   * Retrieve the current moving average value for a given channel.
   *
   * @param {Number} n Number of channel to retrieve the moving average for.
   * @return {Number} Current moving average value for the specified channel.
   * @memberof MovingAverage
   */
  read (n) {
    return this.sum[n] / this.averageWindow
  }
}

/**
 * Moving average of the output (effectively a low-pass to get the general envelope).
 * Fast approximation of the MovingAverage; requires significantly less memory.
 *
 * @see {@link https://www.daycounter.com/LabBook/Moving-Average.phtml}
 * @class FastMovingAverage
 * @extends {MovingAverage}
 */
export class FastMovingAverage extends MovingAverage {
  /**
   * Update the internal state with from the input.
   *
   * @param {Float32Array} levels Array of level values, one per channel.
   * @memberof FastMovingAverage
   */
  update (levels) {
    this.updateAverageWindow()
    for (let n = 0; n < this.channels; n++) {
      const currentSum = this.sum[n]
      this.sum[n] = this.averageWindow
        ? currentSum + levels[n] - currentSum / this.averageWindow
        : levels[n]
    }
  }
}

/**
 * Moving average of the output (effectively a low-pass to get the general envelope).
 * This is the "proper" implementation; it does require lots of memory allocated for the RingBuffers!
 *
 * @class HeavyMovingAverage
 * @extends {MovingAverage}
 */
export class HeavyMovingAverage extends MovingAverage {
  /**
   * Creates an instance of HeavyMovingAverage.
   * @param {Number} channels Number of channels to process.
   * @param {Number} sampleRate Sample rate, used to convert between time and amount of samples.
   * @param {Number} [maxWindow=sampleRate] Preallocate buffers of this size, per channel.
   * @memberof HeavyMovingAverage
   */
  constructor (channels, sampleRate, maxWindow = sampleRate) {
    super(channels, sampleRate)
    this.history = []
    for (let n = 0; n < channels; n++) {
      this.history.push(new RingBuffer(maxWindow))
    }
  }

  /**
   * Update the internal state with from the input.
   *
   * @param {Float32Array} levels Array of level values, one per channel.
   * @memberof HeavyMovingAverage
   */
  update (levels) {
    for (let n = 0; n < this.channels; n++) {
      const value = levels[n]
      this.history[n].write(value)
      this.sum[n] += value

      if (this.targetAverageWindow === this.averageWindow) {
        this.sum[n] -= this.history[n].read(this.averageWindow)
      } else if (this.targetAverageWindow < this.averageWindow) {
        this.sum[n] -= this.history[n].read(this.averageWindow)
        this.sum[n] -= this.history[n].read(this.averageWindow - 1)
      }
    }
    this.updateAverageWindow()
  }
}

/**
 * Base class for PianoTuning. Must implement this.mapping array.
 *
 * @class MovingAverage
 */
export class Tuning {
  /**
   * Creates an instance of Tuning.
   * @param {Number} sampleRate Self-explanatory.
   * @param {Number} bands How many filters.
   */
  constructor (sampleRate, bands) {
    this.sampleRate = sampleRate
    this.bands = bands
  }

  /**
   * Approximate k & N values for DFTBin.
   *
   * @param {Number} frequency In Hz.
   * @param {Number} bandwidth In Hz.
   * @return {Object} Object containing k & N that best approximate for the given frequency & bandwidth.
   * @memberof Tuning
   */
  frequencyAndBandwidthToKAndN (frequency, bandwidth) {
    let N = Math.floor(this.sampleRate / bandwidth)
    const k = Math.floor(frequency / bandwidth)

    // find such N that (sampleRate * (k / N)) is the closest to freq
    // (sacrifices the bandwidth precision; bands will be *wider*, and, therefore, will overlap a bit!)
    let delta = Math.abs(this.sampleRate * (k / N) - frequency)
    for (let i = N - 1; ; i--) {
      const tmpDelta = Math.abs(this.sampleRate * (k / i) - frequency)
      if (tmpDelta < delta) {
        delta = tmpDelta
        N = i
      } else {
        return { k, N }
      }
    }
  }
}

/*
// Proof of concept; there's no advantage in using Sliding DFT if we need to cover the full spectrum
export class RegularTuning extends Tuning {
  constructor (sampleRate, bands) {
    super(sampleRate, bands)
    this.mapping = []
    for (let band = 0; band < this.mapping.length; band++) {
      this.mapping.push({ k: band, N: bands * 2 })
    }
  }
}
*/

/**
 * Essentially, creates an instance that provides the 'mapping',
 * which is an array of objects providing the values for i, k & N.
 *
 * @class PianoTuning
 * @extends {Tuning}
 */
export class PianoTuning extends Tuning {
  /**
   * Creates an instance of PianoTuning.
   * @param {Number} sampleRate Self-explanatory.
   * @param {Number} [pitchFork=440.0] A4 is 440 Hz by default.
   * @param {Number} [referenceKey=33] Key index for the pitchFork reference (A4 is the default).
   * @param {Number} [keysNum=61] Most pianos will have 61 keys.
   * @memberof PianoTuning
   */
  constructor (sampleRate, keysNum = 61, referenceKey = 33, pitchFork = 440.0) {
    super(sampleRate, keysNum)
    this.pitchFork = pitchFork
    this.referenceKey = referenceKey
  }

  /**
   * Converts the piano key number to it's fundamental frequency.
   *
   * @see {@link https://en.wikipedia.org/wiki/Piano_key_frequencies}
   * @param {Number} key
   * @return {Number}
   * @memberof PianoTuning
   */
  keyToFreq (key) {
    return this.pitchFork * Math.pow(2, (key - this.referenceKey) / 12)
  }

  /**
   * Computes the array of objects that specify the frequencies to analyze.
   *
   * @readonly
   * @memberof PianoTuning
   */
  get mapping () {
    const output = []
    for (let key = 0; key < this.bands; key++) {
      const frequency = this.keyToFreq(key)
      const bandwidth = 2 * (this.keyToFreq(key + 0.5) - frequency)
      output.push(this.frequencyAndBandwidthToKAndN(frequency, bandwidth))
    }
    return output
  }
}

/**
 * Sliding Discrete Fourier Transform implementation for (westerns) musical frequencies.
 *
 * @see {@link https://www.comm.utoronto.ca/~dimitris/ece431/slidingdft.pdf}
 * @class SlidingDFT
 */
export class SlidingDFT {
  /**
   * Creates an instance of SlidingDFT.
   * @param {PianoTuning} tuning PianoTuning instance.
   * @param {Number} [maxAverageWindowInSeconds=0] Positive values are passed to MovingAverage implementation; negative values trigger FastMovingAverage implementation. Zero disables averaging.
   * @memberof SlidingDFT
   */
  constructor (tuning, maxAverageWindowInSeconds = 0) {
    this.bins = []
    this.levels = new Float32Array(tuning.bands)

    let maxN = 0
    tuning.mapping.forEach((band) => {
      this.bins.push(new DFTBin(band.k, band.N))
      maxN = Math.max(maxN, band.N)
    })

    this.ringBuffer = new RingBuffer(maxN)

    if (maxAverageWindowInSeconds > 0) {
      this.movingAverage = new HeavyMovingAverage(
        tuning.bands,
        tuning.sampleRate,
        Math.round(tuning.sampleRate * maxAverageWindowInSeconds)
      )
    } else if (maxAverageWindowInSeconds < 0) {
      this.movingAverage = new FastMovingAverage(
        tuning.bands,
        tuning.sampleRate
      )
    } else {
      this.movingAverage = null
    }
  }

  /**
   * Process a batch of samples.
   *
   * @param {Float32Array} samples Array with the batch of samples to process.
   * @param {Number} [averageWindowInSeconds=0] Adjust the moving average window size.
   * @return {Float32Array} Snapshot of the levels after processing all the samples.
   * @memberof SlidingDFT
   */
  process (samples, averageWindowInSeconds = 0) {
    if (this.movingAverage !== null) {
      this.movingAverage.averageWindowInSeconds = averageWindowInSeconds
    }
    const windowSize = samples.length
    const binsNum = this.bins.length

    // store in the ring buffer & process
    for (let i = 0; i < windowSize; i++) {
      const currentSample = samples[i]
      samples[i] = 0
      this.ringBuffer.write(currentSample)

      for (let band = 0; band < binsNum; band++) {
        const bin = this.bins[band]
        const previousSample = this.ringBuffer.read(bin.N)
        bin.update(previousSample, currentSample)
        this.levels[band] = bin.normalizedAmplitudeSpectrum
        // this.levels[band] = bin.logarithmicUnitDecibels
      }

      if (this.movingAverage !== null) {
        this.movingAverage.update(this.levels)
      }
    }

    // snapshot of the levels, after smoothing
    if (this.movingAverage !== null && this.movingAverage.averageWindow > 0) {
      for (let band = 0; band < binsNum; band++) {
        this.levels[band] = this.movingAverage.read(band)
      }
    }

    return this.levels
  }
}
