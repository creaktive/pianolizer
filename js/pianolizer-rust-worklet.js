/**
 * AudioWorkletProcessor for pianolizer using Rust WASM backend.
 *
 * Pattern from PaulBatchelor/rust-wasm-audioworklet:
 * - Main thread fetches .wasm binary and passes ArrayBuffer via processorOptions
 * - Worklet synchronously instantiates WebAssembly.Module in constructor
 * - Zero-copy buffer sharing via Float32Array views of linear memory
 */

/* global sampleRate */

class PianolizerRustWorklet extends AudioWorkletProcessor {
  static get parameterDescriptors () {
    return [{
      name: 'smooth',
      defaultValue: 0.04,
      minValue: 0,
      maxValue: 0.25,
      automationRate: 'k-rate'
    }, {
      name: 'threshold',
      defaultValue: 0.05,
      minValue: 0,
      maxValue: 1.0,
      automationRate: 'k-rate'
    }]
  }

  constructor (options) {
    super()

    const wasmBytes = options.processorOptions.wasmBytes
    const mod = new WebAssembly.Module(wasmBytes)
    this.wasm = new WebAssembly.Instance(mod, {})
    this.exports = this.wasm.exports

    const opts = options.processorOptions
    this.keysNum = opts.keysNum || 61

    // Create pianolizer instance (fast moving average default)
    this.ptr = this.exports.pianolizer_new(
      sampleRate,
      this.keysNum,
      opts.referenceKey || 33,
      opts.pitchFork || 440.0,
      opts.tolerance || 1.0
    )

    // Pre-allocate buffers in constructor
    this.outPtr = this.exports.pianolizer_alloc(61)

    // Input buffer: pre-allocate for max expected block size (48kHz * 0.5s = 24000)
    const maxBlockSize = 24000
    this.samplesPtr = this.exports.pianolizer_alloc(maxBlockSize)

    // Create Float32Array views — always read buffer fresh from exports
    this.outView = new Float32Array(this.wasm.exports.memory.buffer, this.outPtr, 61)

    this.samplesView = null // allocated lazily in process() based on actual block size
  }

  /**
   * SDFT processing algorithm for the audio processor worklet.
   */
  process (input, output, parameters) {
    if (input[0].length === 0) {
      return true
    }

    const windowSize = input[0][0].length

    // Lazily allocate samples view if block size changed
    if (!this.samplesView || this.samplesView.length < windowSize) {
      this.samplesView = new Float32Array(this.wasm.exports.memory.buffer, this.samplesPtr, windowSize)
    }

    // Mix down all input channels into single array
    const inputPortCount = input.length
    for (let portIndex = 0; portIndex < inputPortCount; portIndex++) {
      const channelCount = input[portIndex].length
      for (let channelIndex = 0; channelIndex < channelCount; channelIndex++) {
        const channel = input[portIndex][channelIndex]
        for (let sampleIndex = 0; sampleIndex < windowSize; sampleIndex++) {
          this.samplesView[sampleIndex] += channel[sampleIndex]
        }
      }
    }

    // Call Rust WASM process function
    this.exports.pianolizer_process(
      this.ptr,
      this.samplesPtr,
      windowSize,
      this.outPtr,
      0 // moving average handled internally by Rust (default: fast MA)
    )

    // Read output from WASM memory — always read buffer fresh
    const levelsView = new Float32Array(this.wasm.exports.memory.buffer, this.outPtr, this.keysNum)
    const levels = new Float32Array(levelsView)

    // Apply threshold filtering
    const threshold = parameters.threshold[0]
    for (let i = 0; i < this.keysNum; i++) {
      if (levels[i] < threshold) {
        levels[i] = 0
      }
    }

    this.port.postMessage(levels)

    return true
  }
}

registerProcessor('pianolizer-rust-worklet', PianolizerRustWorklet)
