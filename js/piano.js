// eslint-disable-next-line no-unused-vars
class Palette {
  constructor (palette) {
    this.palette = palette
    this.keysNum = 88
    this.keyColors = new Uint32Array(this.keysNum)
  }

  getKeyColors (levels) {
    for (let key = 0; key < this.keysNum; key++) {
      const level = levels[key]
      const rgbArray = this.palette[(key + 9) % 12] // start from A
        .map(value => Math.round(level * value) | 0)
      this.keyColors[key] = (rgbArray[0] << 16) | (rgbArray[1] << 8) | rgbArray[2]
    }
    return this.keyColors
  }
}

// eslint-disable-next-line no-unused-vars
class PianoKeyboard {
  // Shamelessly stolen from http://www.quadibloc.com/other/cnv05.htm
  constructor (svg, scale = 1) {
    this.svg = svg
    this.scale = scale

    this.roundCorners = 2
    this.whiteHeight = 150 * scale
    this.blackHeight = 100 * scale
    this.whiteKeys = [23, 24, 23, 24, 23, 23, 24].map(x => x * scale)
    this.whiteTone = [1, 3, 5, 6, 8, 10, 12]
    this.blackKeys = [14, 14, 14, 14, 14, 13, 14, 13, 14, 13, 14, 13].map(x => x * scale)
    this.blackTone = [0, 2, 0, 4, 0, 0, 7, 0, 9, 0, 11, 0]

    this.ns = 'http://www.w3.org/2000/svg'
    this.keysNum = 88
    this.keys = new Array(this.keysNum)
    this.keySlices = null

    this.drawKeyboard()
  }

  drawKey (index, offset, width, height, group) {
    const keyElement = document.createElementNS(this.ns, 'rect')
    keyElement.setAttribute('x', offset)
    keyElement.setAttribute('y', 0)
    keyElement.setAttribute('rx', this.roundCorners)
    keyElement.setAttribute('width', width)
    keyElement.setAttribute('height', height)
    keyElement.classList.add('piano-key')
    this.keys[index] = keyElement
    group.appendChild(keyElement)
  }

  // Inspired by https://github.com/davidgilbertson/sight-reader/blob/master/app/client/Piano.js
  drawKeyboard () {
    const whiteKeyGroup = document.createElementNS(this.ns, 'g')
    const blackKeyGroup = document.createElementNS(this.ns, 'g')

    let blackOffset = 7 * this.scale
    let whiteOffset = 0
    let whiteIndex = 5 // A0
    const startFrom = 9

    const keySlices = []
    for (let i = startFrom; i < this.keysNum + startFrom; i++) {
      // black
      const blackIndex = i % this.blackKeys.length
      const blackWidth = this.blackKeys[blackIndex]
      keySlices.push(blackWidth)
      if (this.blackTone[blackIndex]) {
        this.drawKey(i - startFrom, blackOffset, blackWidth, this.blackHeight, blackKeyGroup)
      } else {
        // white
        const whiteWidth = this.whiteKeys[whiteIndex % this.whiteKeys.length]
        this.drawKey(i - startFrom, whiteOffset, whiteWidth, this.whiteHeight, whiteKeyGroup)
        whiteIndex++
        whiteOffset += whiteWidth
      }
      blackOffset += blackWidth
    }
    this.keySlices = new Uint8Array(keySlices)

    this.svg.appendChild(whiteKeyGroup)
    this.svg.appendChild(blackKeyGroup)
  }

  update (keyColors) {
    for (let key = 0; key < this.keysNum; key++) {
      const rgbString = keyColors[key]
        .toString(16)
        .padStart(6, '0')
      this.keys[key].style.fill = '#' + rgbString
    }
  }
}
