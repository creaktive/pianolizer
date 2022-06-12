/**
 * Color management helper class.
 *
 * @class Palette
 */
export class Palette {
  /**
   * Creates an instance of Palette.
   * @param {Array} palette RGB tuples; one per semitone.
   * @memberof Palette
   */
  constructor (palette) {
    this.palette = palette
    this.startOffset = 0
  }

  /**
   * Shift the colors around the octave.
   *
   * @memberof Palette
   */
  get rotation () {
    return this.startOffset
  }

  /**
   * Shift the colors around the octave.
   *
   * @memberof Palette
   */
  set rotation (n) {
    this.startOffset = n | 0
  }

  /**
   * Applies the intensity levels to the palette.
   *
   * @param {Float32Array} levels Array with the intensity levels.
   * @return {Uint32Array} Palette with the levels applied, in a format compatible with canvas pixels.
   * @memberof Palette
   */
  getKeyColors (levels) {
    const levelsNum = levels.length
    const keyColors = new Uint32Array(levelsNum)

    const paletteLength = this.palette.length
    for (let key = 0; key < levelsNum; key++) {
      const index = this.startOffset + key // start from C
      const rgbArray = this.palette[index % paletteLength]
        .map(value => Math.round(levels[key] * value) | 0)
      keyColors[key] = (rgbArray[2] << 16) | (rgbArray[1] << 8) | rgbArray[0]
    }

    return keyColors
  }
}

export class PianoKeyboard {
  // Shamelessly stolen from http://www.quadibloc.com/other/cnv05.htm
  constructor (svgElement, scale = 1) {
    this.svgElement = svgElement
    this.scale = scale

    this.roundCorners = 2
    this.whiteHeight = 150 * scale
    this.blackHeight = 100 * scale
    this.whiteKeys = [23, 24, 23, 24, 23, 23, 24].map(x => x * scale)
    this.whiteTone = [1, 3, 5, 6, 8, 10, 12]
    this.blackKeys = [14, 14, 14, 14, 14, 13, 14, 13, 14, 13, 14, 13].map(x => x * scale)
    this.blackTone = [0, 2, 0, 4, 0, 0, 7, 0, 9, 0, 11, 0]
    this.neutralColor = 0x55

    this.ns = 'http://www.w3.org/2000/svg'
    this.keySlices = null

    this.keysNum = 61
    this.keys = new Array(this.keysNum)
    this.labels = new Array(this.keysNum)
    this.blackOffset = 0
    this.whiteOffset = 0
    this.whiteIndex = 0 // C2
    this.startFrom = 0
    this.startOctave = 2
  }

  drawKey (index, offset, width, height, group) {
    const keyElement = document.createElementNS(this.ns, 'rect')
    keyElement.setAttribute('x', offset)
    keyElement.setAttribute('y', 0)
    keyElement.setAttribute('rx', this.roundCorners)
    keyElement.setAttribute('width', width - 1)
    keyElement.setAttribute('height', height)
    keyElement.classList.add('piano-key')
    this.keys[index] = keyElement
    group.appendChild(keyElement)
  }

  drawLabel (index, offset, note, octave, group) {
    const labelElement = document.createElementNS(this.ns, 'text')
    labelElement.setAttribute('x', offset + 5 * this.scale)
    labelElement.setAttribute('y', this.whiteHeight - 6 * this.scale)
    labelElement.classList.add('piano-key-label')
    labelElement.textContent = String.fromCharCode(note + (note < 5 ? 67 : 60)) + octave
    this.labels[index] = labelElement
    group.appendChild(labelElement)
  }

  // Inspired by https://github.com/davidgilbertson/sight-reader/blob/master/app/client/Piano.js
  drawKeyboard () {
    const whiteKeyGroup = document.createElementNS(this.ns, 'g')
    const blackKeyGroup = document.createElementNS(this.ns, 'g')

    let blackOffset = this.blackOffset
    let whiteOffset = this.whiteOffset
    let whiteIndex = this.whiteIndex

    const keySlices = []
    let blackSum = 0
    for (let i = this.startFrom; i < this.keysNum + this.startFrom; i++) {
      // black
      const blackIndex = i % this.blackKeys.length
      const blackWidth = this.blackKeys[blackIndex]
      const index = i - this.startFrom
      keySlices.push(blackWidth)
      blackSum += blackWidth
      if (this.blackTone[blackIndex]) {
        this.drawKey(index, blackOffset, blackWidth, this.blackHeight, blackKeyGroup)
      } else {
        // white
        const note = whiteIndex % this.whiteKeys.length
        const whiteWidth = this.whiteKeys[note]
        this.drawKey(index, whiteOffset, whiteWidth, this.whiteHeight, whiteKeyGroup)

        const octave = 0 | whiteIndex / this.whiteKeys.length + this.startOctave
        this.drawLabel(index, whiteOffset, note, octave, whiteKeyGroup)

        whiteIndex++
        whiteOffset += whiteWidth
      }
      blackOffset += blackWidth
    }

    // adjust padding of the key roots
    this.keySlices = new Uint8Array(keySlices)
    this.keySlices[0] += this.blackOffset
    this.keySlices[this.keySlices.length - 1] += whiteOffset - blackSum - this.blackOffset

    this.svgElement.appendChild(whiteKeyGroup)
    this.svgElement.appendChild(blackKeyGroup)

    this.svgElement.setAttribute('width', whiteOffset)
    this.svgElement.setAttribute('height', this.whiteHeight)
  }

  bgrIntegerToHex (bgrInteger, start = 0) {
    // #kill me
    const range = (0xff - start) / 0xff
    const rgbArray = [
      (bgrInteger & 0x0000ff),
      (bgrInteger & 0x00ff00) >> 8,
      (bgrInteger & 0xff0000) >> 16
    ].map(c => Math.round(start + c * range) | 0)
    return '#' + rgbArray.map(c => c.toString(16).padStart(2, '0')).join('')
  }

  update (audioColors, midiColors) {
    for (let key = 0; key < this.keysNum; key++) {
      this.keys[key].style.fill = this.bgrIntegerToHex(audioColors[key])

      const midiColor = this.bgrIntegerToHex(midiColors[key], this.neutralColor)
      this.keys[key].style.stroke = midiColor
      if (this.labels[key] !== undefined) {
        this.labels[key].style.fill = midiColor
      }
    }
  }
}

/*
export class PianoKeyboardFull extends PianoKeyboard {
  constructor (svgElement, scale = 1) {
    super(svgElement, scale)

    this.keysNum = 88
    this.keys = new Array(this.keysNum)
    this.blackOffset = 7 * scale
    this.whiteOffset = 0
    this.whiteIndex = 5 // A0
    this.startFrom = 9
    this.startOctave = 0
  }
}
*/

export class Spectrogram {
  constructor (canvasElement, keySlices, height) {
    this.canvasElement = canvasElement
    this.keySlices = keySlices
    this.lastMidiColors = new Uint32Array(keySlices.length)

    this.width = keySlices.reduce((a, b) => a + b)
    this.height = height

    canvasElement.width = this.width
    canvasElement.height = this.height

    this.context = canvasElement.getContext('2d')
    this.imageData = this.context.createImageData(this.width, this.height)

    this.bufArray = new ArrayBuffer(this.width * this.height * 4)
    this.buf8 = new Uint8Array(this.bufArray)
    this.buf32 = new Uint32Array(this.bufArray)
  }

  update (audioColors, midiColors) {
    // shift the whole buffer 1 line upwards
    const lastLine = this.width * (this.height - 1)
    for (let i = 0; i < lastLine; i++) {
      this.buf32[i] = this.buf32[i + this.width]
    }

    // fill in the bottom line
    const keysNum = this.keySlices.length
    const alphaOpaque = 0xff000000
    for (let key = 0, j = lastLine; key < keysNum; key++) {
      const slice = this.keySlices[key]
      if (this.lastMidiColors[key] !== midiColors[key]) {
        for (let i = 0; i < slice; i++, j++) {
          const bgrInteger = midiColors[key] || this.lastMidiColors[key]
          this.buf32[j] = alphaOpaque | bgrInteger
        }
        this.lastMidiColors[key] = midiColors[key]
      } else {
        for (let i = 0; i < slice; i++, j++) {
          const bgrInteger = i < 1 || i >= slice - 1
            ? midiColors[key]
            : audioColors[key]
          this.buf32[j] = alphaOpaque | bgrInteger
        }
      }
    }

    // render
    this.imageData.data.set(this.buf8)
    this.context.putImageData(this.imageData, 0, 0)
  }
}
