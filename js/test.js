import { RingBuffer, DFTBin } from './pianolizer.js'

const waveform = {
  SINE: 0,
  SAWTOOTH: 1,
  SQUARE: 2,
  NOISE: 3
}

// 441Hz wave period is 100 samples when the sample rate is 44100Hz
function oscillator (s, type = waveform.SINE) {
  switch (type) {
    case waveform.SINE:
      return Math.sin((2.0 * Math.PI) / 100.0 * s)
    case waveform.SAWTOOTH:
      return ((s % 100) / 50.0) - 1.0
    case waveform.SQUARE:
      return ((s % 100) < 50) ? 1.0 : -1.0
    case waveform.NOISE:
      return 2.0 * Math.random() - 1.0
  }
}

function testDFT (type, expected) {
  const N = 1700
  const bin = new DFTBin(17, N)
  const rb = new RingBuffer(N)
  for (let i = 0; i < 2000; i++) {
    const currentSample = oscillator(i, type)
    rb.write(currentSample)
    const previousSample = rb.read(N)
    bin.update(previousSample, currentSample)
  }

  const nas = bin.normalizedAmplitudeSpectrum
  if (Math.floor(nas * 1_000_000) === expected) {
    console.log('ok')
  } else {
    console.log('not ok')
  }
}

testDFT(waveform.SINE, 999999)
testDFT(waveform.SAWTOOTH, 779747)
testDFT(waveform.SQUARE, 900464)
