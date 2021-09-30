// eslint-disable-next-line no-unused-vars
class PianoOctave {
  // Shamelessly stolen from http://www.quadibloc.com/other/cnv05.htm
  constructor (scale = 1.0) {
    this.scale = scale

    this.whiteHeight = 150 * scale
    this.blackHeight = 100 * scale
    this.whiteKeys = [23, 24, 23, 24, 23, 23, 24].map(x => x * scale)
    this.whiteTone = [1, 3, 5, 6, 8, 10, 12]
    this.blackKeys = [14, 14, 14, 14, 14, 13, 14, 13, 14, 13, 14, 13].map(x => x * scale)
    this.blackTone = [0, 2, 0, 4, 0, 0, 7, 0, 9, 0, 11, 0]

    this.width = this.whiteKeys.reduce((a, b) => a + b)
  }

  // Inspired by https://github.com/davidgilbertson/sight-reader/blob/master/app/client/Piano.js
  draw (svgId) {
    const ns = 'http://www.w3.org/2000/svg'

    // render whites
    const whiteKeyGroup = document.createElementNS(ns, 'g')
    for (
      let i = 0, offset = 0;
      i < this.whiteKeys.length;
      offset += this.whiteKeys[i++]
    ) {
      // const tone = this.whiteTone[i]

      const keyElement = document.createElementNS(ns, 'rect')
      keyElement.setAttribute('x', offset)
      keyElement.setAttribute('y', 0)
      keyElement.setAttribute('rx', 2)
      keyElement.setAttribute('width', this.whiteKeys[i])
      keyElement.setAttribute('height', this.whiteHeight)
      keyElement.setAttribute('style', 'stroke: #000; fill: #fff')
      whiteKeyGroup.appendChild(keyElement)
    }
    svgId.appendChild(whiteKeyGroup)

    // render blacks
    const blackKeyGroup = document.createElementNS(ns, 'g')
    for (
      let i = 0, offset = 0;
      i < this.blackKeys.length;
      offset += this.blackKeys[i++]
    ) {
      const tone = this.blackTone[i]
      if (tone === 0) { continue }

      const keyElement = document.createElementNS(ns, 'rect')
      keyElement.setAttribute('x', offset)
      keyElement.setAttribute('y', 0)
      keyElement.setAttribute('rx', 2)
      keyElement.setAttribute('width', this.blackKeys[i])
      keyElement.setAttribute('height', this.blackHeight)
      keyElement.setAttribute('style', 'stroke: #000; fill: #fff')
      blackKeyGroup.appendChild(keyElement)
    }
    svgId.appendChild(blackKeyGroup)
  }
}
