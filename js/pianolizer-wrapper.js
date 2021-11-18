// eslint-disable-next-line no-unused-vars
class Pianolizer {
  /* global Module */
  constructor (sampleRate) {
    this.pianolizer = new Module.Pianolizer(sampleRate)
  }

  process (samples, averageWindowInSeconds = 0) {
    const samplesBufferSize = samples.length
    const samplesBuffer = Module._malloc(samplesBufferSize * Float32Array.BYTES_PER_ELEMENT)
    const startOffset = samplesBuffer / Float32Array.BYTES_PER_ELEMENT
    const endOffset = startOffset + samplesBufferSize
    const samplesView = Module.HEAPF32.subarray(startOffset, endOffset)

    for (let i = 0; i < samplesBufferSize; i++) {
      samplesView[i] = samples[i]
    }

    const levels = this.pianolizer.process(
      samplesBuffer,
      samplesBufferSize,
      averageWindowInSeconds
    )

    Module._free(samplesBuffer)
    return new Float32Array(levels)
  }
}
