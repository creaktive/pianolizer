// eslint-disable-next-line no-unused-vars
class PianoOctave {
  // Shamelessly stolen from http://www.quadibloc.com/other/cnv05.htm
  constructor (scale = 1) {
    this.scale = scale

    this.whiteHeight = 150 * scale
    this.blackHeight = 100 * scale
    this.whiteKeys = [23, 24, 23, 24, 23, 23, 24].map(x => x * scale)
    this.whiteTone = [1, 3, 5, 6, 8, 10, 12]
    this.blackKeys = [14, 14, 14, 14, 14, 13, 14, 13, 14, 13, 14, 13].map(x => x * scale)
    this.blackTone = [0, 2, 0, 4, 0, 0, 7, 0, 9, 0, 11, 0]

    this.ns = 'http://www.w3.org/2000/svg'
  }

  drawKey (index, offset, width, height, group) {
    const keyElement = document.createElementNS(this.ns, 'rect')
    keyElement.setAttribute('data-ref', `pianoKey${index}`)
    keyElement.setAttribute('x', offset)
    keyElement.setAttribute('y', 0)
    keyElement.setAttribute('rx', 2)
    keyElement.setAttribute('width', width)
    keyElement.setAttribute('height', height)
    keyElement.setAttribute('style', 'stroke: #000; fill: #fff')
    group.appendChild(keyElement)
  }

  // Inspired by https://github.com/davidgilbertson/sight-reader/blob/master/app/client/Piano.js
  drawKeyboard (svgId) {
    const whiteKeyGroup = document.createElementNS(this.ns, 'g')
    const blackKeyGroup = document.createElementNS(this.ns, 'g')

    let blackOffset = 0
    let whiteOffset = 0
    let whiteIndex = 0
    for (let i = 0; i < 61; i++) {
      // black
      const blackIndex = i % this.blackKeys.length
      const blackWidth = this.blackKeys[blackIndex]
      if (this.blackTone[blackIndex]) {
        this.drawKey(i, blackOffset, blackWidth, this.blackHeight, blackKeyGroup)
      } else {
        // white
        const whiteWidth = this.whiteKeys[whiteIndex % this.whiteKeys.length]
        this.drawKey(i, whiteOffset, whiteWidth, this.whiteHeight, whiteKeyGroup)
        whiteIndex++
        whiteOffset += whiteWidth
      }
      blackOffset += blackWidth
    }

    svgId.appendChild(whiteKeyGroup)
    svgId.appendChild(blackKeyGroup)
  }
}
