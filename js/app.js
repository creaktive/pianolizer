import { PianoKeyboard, Spectrogram, Palette } from './visualization.js'

let audioContext, audioSource, microphoneSource, pianolizer
let levels, palette

const audioElement = document.getElementById('input')
const playToggle = document.getElementById('play-toggle')
const playRestart = document.getElementById('play-restart')
const sourceSelect = document.getElementById('source')
const pianolizerUI = document.getElementById('pianolizer')
const rotationInput = document.getElementById('rotation')
const smoothingInput = document.getElementById('smoothing')
const thresholdInput = document.getElementById('threshold')

function isPureJS () {
  return window.location.search.toLowerCase() === '?purejs'
}

function loadSettings (reset = false) {
  if (reset === true) {
    localStorage.clear()
  }

  const inputEvent = new Event('input')

  rotationInput.value = localStorage.getItem('rotation') || 0
  rotationInput.dispatchEvent(inputEvent)

  smoothingInput.value = localStorage.getItem('smoothing') || 0.040
  smoothingInput.dispatchEvent(inputEvent)

  thresholdInput.value = localStorage.getItem('threshold') || 0.050
  thresholdInput.dispatchEvent(inputEvent)
}

async function loadLocalFile () {
  const fileHandles = await window.showOpenFilePicker({
    types: [
      {
        description: 'Audio',
        accept: {
          'audio/*': ['.mp3', '.flac', '.ogg', '.wav']
        }
      }
    ],
    excludeAcceptAllOption: true,
    multiple: false
  })
  const fileData = await fileHandles[0].getFile()
  audioElement.src = URL.createObjectURL(fileData)
}

async function setupAudio () {
  if (audioContext === undefined) {
    audioContext = new (window.AudioContext || window.webkitAudioContext)()

    // Thanks for nothing, Firefox!
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1572644
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1636121
    // https://github.com/WebAudio/web-audio-api-v2/issues/109#issuecomment-756634198
    const fetchText = url => fetch(url).then(response => response.text())
    const pianolizerImplementation = isPureJS()
      ? 'js/pianolizer.js'
      : 'js/pianolizer-wasm.js'
    const modules = await Promise.all([
      fetchText(pianolizerImplementation),
      fetchText('js/pianolizer-worklet.js')
    ])
    const blob = new Blob(modules, { type: 'application/javascript' })
    await audioContext.audioWorklet.addModule(URL.createObjectURL(blob))

    pianolizer = new AudioWorkletNode(audioContext, 'pianolizer-worklet')
    pianolizer.port.onmessage = event => {
      // TODO: use SharedArrayBuffer for syncing levels
      levels = event.data
    }

    audioSource = audioContext.createMediaElementSource(audioElement)
    audioSource.connect(pianolizer)
  }

  audioSource.connect(audioContext.destination)
  pianolizer.parameters.get('smooth').value = parseFloat(smoothingInput.value)
  pianolizer.parameters.get('threshold').value = parseFloat(thresholdInput.value)
}

async function setupMicrophone (deviceId) {
  await setupAudio()
  audioSource.disconnect(audioContext.destination)

  if (microphoneSource === undefined) {
    await navigator.mediaDevices.getUserMedia({ audio: { deviceId }, video: false })
      .then(stream => {
        microphoneSource = audioContext.createMediaStreamSource(stream)
        microphoneSource.connect(pianolizer)

        // for whatever reason, once selected, the input can't be switched
        const selectedLabel = '*' + deviceId
        for (const option of sourceSelect.options) {
          if (option.value.charAt(0) === '*' && option.value !== selectedLabel) {
            option.disabled = true
          }
        }
      })
      .catch(error => window.alert('Audio input access denied: ' + error))
  } else {
    microphoneSource.connect(pianolizer)
  }
}

function setupMIDI (midi) {
  if (navigator.requestMIDIAccess) {
    navigator.requestMIDIAccess()
      .then(
        midiAccess => {
          // connecting MIDI to input function
          for (const input of midiAccess.inputs.values()) {
            input.onmidimessage = message => {
              const firstNoteIndex = 36
              const [command, note, velocity] = message.data
              switch (command) {
                case 0x90:
                  midi[note - firstNoteIndex] = velocity / 0x7f
                  break
                case 0x80:
                  midi[note - firstNoteIndex] = 0
                  break
              }
            }
          }

          sourceSelect.add(new Option('MIDI input only', '#'))
        })
  }
}

function setupUI (keysNum) {
  sourceSelect.onchange = event => {
    audioElement.pause()
    playToggle.innerText = 'Play'

    const selectedValue = event.target.value
    if (selectedValue.charAt(0) === '*') {
      // microphone source
      playToggle.disabled = true
      playRestart.disabled = true
      setupMicrophone(selectedValue.substring(1))
    } else {
      // <audio> element source
      playToggle.disabled = false
      playRestart.disabled = false
      try { microphoneSource.disconnect(pianolizer) } catch { console.warn('Microphone was not connected') }
      audioElement.style['pointer-events'] = 'auto'
      if (selectedValue === '/') {
        // only Chrome & Opera can do this at the time of writing
        loadLocalFile()
      } else if (selectedValue === '#') {
        // "MIDI solo" mode
        playToggle.disabled = true
        playRestart.disabled = true
        levels = new Float32Array(keysNum)
      } else {
        audioElement.src = `${selectedValue}?_=${Date.now()}` // never cache
      }
    }
  }

  playToggle.onclick = async event => {
    if (audioElement.paused) {
      await setupAudio()
      audioElement.play()
      playToggle.innerText = 'Pause'
    } else {
      audioElement.pause()
      playToggle.innerText = 'Play'
    }
  }

  playRestart.onclick = event => {
    audioElement.load()
    playToggle.innerText = 'Play'
  }

  rotationInput.oninput = event => {
    const value = parseInt(event.target.value)
    localStorage.setItem('rotation', value)
    palette.rotation = value
  }

  smoothingInput.oninput = event => {
    const value = parseFloat(event.target.value)
    localStorage.setItem('smoothing', value)
    document.getElementById('smoothing-value').innerText = `${value.toFixed(3)}s`
    if (pianolizer !== undefined) {
      pianolizer.parameters.get('smooth').value = value
    }
  }

  thresholdInput.oninput = event => {
    const value = parseFloat(event.target.value)
    localStorage.setItem('threshold', value)
    document.getElementById('threshold-value').innerText = value.toFixed(3)
    if (pianolizer !== undefined) {
      pianolizer.parameters.get('threshold').value = value
    }
  }

  pianolizerUI.ondragover = event => {
    event.preventDefault()
  }

  pianolizerUI.ondrop = event => {
    event.preventDefault()
    if (event.dataTransfer.items) {
      for (const item of event.dataTransfer.items) {
        if (item.kind === 'file' && item.type.match('^audio/(flac|mpeg|ogg|x-wav)$')) {
          audioElement.pause()
          playToggle.innerText = 'Play'

          const fileData = item.getAsFile()
          audioElement.src = URL.createObjectURL(fileData)

          sourceSelect.value = '?'
          document.getElementById('drop-label').innerText = fileData.name
          break
        }
      }
      event.dataTransfer.items.clear()
    } else {
      console.warn('DataTransferItemList interface unavailable')
    }
  }

  navigator.mediaDevices.getUserMedia({ audio: true, video: false })
    .then(() => {
      navigator.mediaDevices.enumerateDevices()
        .then(devices => {
          devices.filter(device => device.kind === 'audioinput')
            .reverse()
            .forEach(device => {
              sourceSelect.add(
                new Option('Input: ' + device.label, '*' + device.deviceId),
                0
              )
            })
        })
    })

  if (window.showOpenFilePicker !== undefined) {
    sourceSelect.add(
      new Option('Load local audio file', '/'),
      0
    )
  }

  const myURL = window.location.href.replace(/\?.*/, '')
  const implementationWASM = document.getElementById('implementation-wasm')
  implementationWASM.onclick = () => {
    window.location.href = myURL
  }
  const implementationPureJS = document.getElementById('implementation-purejs')
  implementationPureJS.onclick = () => {
    window.location.href = myURL + '?purejs'
  }
  if (isPureJS()) {
    implementationPureJS.checked = true
  } else {
    implementationWASM.checked = true
  }
}

async function app () {
  function draw (currentTimestamp) {
    if (
      (playToggle.disabled || !audioElement.paused) &&
    levels !== undefined &&
    palette !== undefined
    ) {
      const audioColors = palette.getKeyColors(levels)
      const midiColors = palette.getKeyColors(midi)
      pianoKeyboard.update(audioColors, midiColors)
      spectrogram.update(audioColors, midiColors)
    }
    window.requestAnimationFrame(draw)
  }

  const paletteData = await fetch('palette.json').then(response => response.json())
  palette = new Palette(paletteData)

  const pianoKeyboard = new PianoKeyboard(document.getElementById('keyboard'))
  pianoKeyboard.drawKeyboard()
  const spectrogram = new Spectrogram(
    document.getElementById('spectrogram'),
    pianoKeyboard.keySlices,
    600
  )

  const midi = new Float32Array(pianoKeyboard.keysNum)

  setupMIDI(midi)
  setupUI(midi.length)
  loadSettings()

  window.requestAnimationFrame(draw)
}

app()
