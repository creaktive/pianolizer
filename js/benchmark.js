import { Pianolizer } from './pianolizer.js'

const pianolizer = new Pianolizer(44100)
const bufferSize = 128
const input = new Float32Array(bufferSize)
let output

const start = performance.now()
let i
for (i = 0; i < bufferSize * 10000; i++) {
  const j = i % bufferSize
  input[j] = ((i % 100) / 50.0) - 1.0
  if (j === bufferSize - 1) {
    output = pianolizer.process(input, 0.05)
  }
}
const end = performance.now()
const elapsed = (end - start) / 1000
console.log('# benchmark: ' + Math.round(i / elapsed) + ' samples per second')
console.log(output[33] === 0.7777045965194702)
