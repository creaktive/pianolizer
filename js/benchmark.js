import Pianolizer from './pianolizer.js'

const print = globalThis.process?.release?.name
  ? console.log // Browser
  : postMessage // NodeJS

const pianolizer = new Pianolizer(44100)
const bufferSize = 128
const input = new Float32Array(bufferSize)
let output

const start = performance.now()
let i
for (i = 0; i < bufferSize * 10000; i++) {
  const j = i % bufferSize
  input[j] = ((i % 100) / 50.0) - 1.0 // sawtooth, 441Hz
  if (j === bufferSize - 1) {
    output = pianolizer.process(input, 0.05)
  }
}
const end = performance.now()
const elapsed = (end - start) / 1000
const message = '# benchmark: ' + Math.round(i / elapsed) + ' samples per second'
print(message)

const test = {
  21: 0.0039842028203572,
  33: 0.7777003371299069,
  45: 0.3889044217798130,
  52: 0.2582185831581467,
  57: 0.1949653144384861
}
for (const key in test) {
  if (Math.abs(output[key] - test[key]) > 1e-5) {
    throw new Error('BAD OUTPUT FOR KEY ' + key)
  }
}
