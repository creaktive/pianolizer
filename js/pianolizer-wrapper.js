// eslint-disable-next-line no-unused-vars
class Pianolizer {
  /* global Module */
  constructor (
    sampleRate,
    keysNum = 61,
    referenceKey = 33,
    pitchFork = 440.0,
    tolerance = 1.0
  ) {
    this.pianolizer = new Module.Pianolizer(
      sampleRate,
      keysNum,
      referenceKey,
      pitchFork,
      tolerance
    )
  }

  adjustSamplesBuffer (requestedSamplesBufferSize) {
    if (this.samplesBufferSize === requestedSamplesBufferSize) {
      return
    }

    if (this.samplesBuffer !== undefined) {
      Module._free(this.samplesBuffer)
    }

    this.samplesBufferSize = requestedSamplesBufferSize
    this.samplesBuffer = Module._malloc(this.samplesBufferSize * Float32Array.BYTES_PER_ELEMENT)
    const startOffset = this.samplesBuffer / Float32Array.BYTES_PER_ELEMENT
    const endOffset = startOffset + this.samplesBufferSize
    this.samplesView = Module.HEAPF32.subarray(startOffset, endOffset)
  }

  process (samples, averageWindowInSeconds = 0) {
    this.adjustSamplesBuffer(samples.length)

    for (let i = 0; i < this.samplesBufferSize; i++) {
      this.samplesView[i] = samples[i]
      samples[i] = 0
    }

    const levels = this.pianolizer.process(
      this.samplesBuffer,
      this.samplesBufferSize,
      averageWindowInSeconds
    )

    return new Float32Array(levels)
  }
}
