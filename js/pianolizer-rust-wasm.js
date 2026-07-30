/**
 * JavaScript wrapper for pianolizer Rust WASM module.
 * Mirrors the current Pianolizer interface from pianolizer-wrapper.js.
 *
 * Usage:
 *   const wasmModule = await WebAssembly.compile(await fetch('js/pianolizer-rust.wasm').then(r => r.arrayBuffer()));
 *   const pianolizer = new Pianolizer(wasmModule, 44100);
 *   const levels = pianolizer.process(samples, 0.05);
 */

export default class Pianolizer {
  constructor (wasmModule, sampleRate, keysNum = 61, referenceKey = 33, pitchFork = 440.0, tolerance = 1.0) {
    const inst = new WebAssembly.Instance(wasmModule, {})
    this.exports = inst.exports

    // Allocate output buffer (61 keys is fixed max)
    this.outputPtr = this.exports.pianolizer_alloc(61)
    this.memory = this.exports.memory.buffer

    // Create pianolizer instance
    this.ptr = this.exports.pianolizer_new(
      sampleRate,
      keysNum,
      referenceKey,
      pitchFork,
      tolerance
    )

    this.samplesBufferSize = 0
    this.samplesBuffer = 0
  }

  adjustSamplesBuffer (requestedSamplesBufferSize) {
    if (this.samplesBufferSize === requestedSamplesBufferSize) {
      return
    }

    if (this.samplesBuffer !== 0) {
      this.exports.pianolizer_free(this.samplesBuffer, this.samplesBufferSize)
    }

    this.samplesBufferSize = requestedSamplesBufferSize
    this.samplesBuffer = this.exports.pianolizer_alloc(requestedSamplesBufferSize)
  }

  process (samples, averageWindowInSeconds = 0) {
    this.adjustSamplesBuffer(samples.length)

    // Copy samples into WASM memory
    const view = new Float32Array(this.memory, this.samplesBuffer, samples.length)
    for (let i = 0; i < samples.length; i++) {
      view[i] = samples[i]
    }

    // Call Rust process function
    const len = this.exports.pianolizer_process(
      this.ptr,
      this.samplesBuffer,
      samples.length,
      this.outputPtr,
      averageWindowInSeconds
    )

    // Read output from WASM memory
    const resultView = new Float32Array(this.memory, this.outputPtr, len)
    return new Float32Array(resultView)
  }

  destroy () {
    if (this.samplesBuffer !== 0) {
      this.exports.pianolizer_free(this.samplesBuffer, this.samplesBufferSize)
    }
    if (this.outputPtr !== 0) {
      this.exports.pianolizer_free(this.outputPtr, 61)
    }
    this.exports.pianolizer_delete(this.ptr)
  }
}
