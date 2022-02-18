#!/usr/bin/env python3
import sys
import time
from array import array

from rtmidi.midiutil import open_midiinput
from rpi_ws281x import PixelStrip, Color

# LED strip configuration:
LED_COUNT = 144       # Number of LED pixels.
LED_PIN = 12          # GPIO pin connected to the pixels (18 uses PWM!).
# LED_PIN = 10        # GPIO pin connected to the pixels (10 uses SPI /dev/spidev0.0).
LED_FREQ_HZ = 800000  # LED signal frequency in hertz (usually 800khz)
LED_DMA = 10          # DMA channel to use for generating signal (try 10)
LED_BRIGHTNESS = 32   # Set to 0 for darkest and 255 for brightest
LED_INVERT = False    # True to invert the signal (when using NPN transistor level shift)
LED_CHANNEL = 0       # set to '1' for GPIOs 13, 19, 41, 45 or 53

class MidiInputHandler(object):
    def __init__(self, port, keys, first_key=36):
        self.port = port
        self.keys = keys
        self.first_key = first_key
        self.update_pending = 0

    def __call__(self, event, data=None):
        message, deltatime = event
        status, key, velocity = message
        key -= self.first_key

        if status == 0x90:
            self.keys[key] = velocity
        elif status == 0x80:
            self.keys[key] = 0
        else:
            print("WTF: %r" % message)

        self.update_pending = 1

    def update(self):
        tmp = self.update_pending
        self.update_pending = 0
        return tmp

if __name__ == '__main__':
    # Prompts user for MIDI input port, unless a valid port number or name
    # is given as the first argument on the command line.
    # API backend defaults to ALSA on Linux.
    port = sys.argv[1] if len(sys.argv) > 1 else None

    try:
        midiin, port_name = open_midiinput(port)
    except (EOFError, KeyboardInterrupt):
        sys.exit()

    keys = array('B', [0] * 61)
    midi_handler = MidiInputHandler(port_name, keys)
    midiin.set_callback(midi_handler)

    strip = PixelStrip(LED_COUNT, LED_PIN, LED_FREQ_HZ, LED_DMA, LED_INVERT, LED_BRIGHTNESS, LED_CHANNEL)
    strip.begin()

    print("Entering main loop. Press Control-C to exit.")
    try:
        while True:
            if midi_handler.update():
                for i in range(len(keys)):
                    c = keys[i] * 2
                    for j in range(2):
                        strip.setPixelColor(i * 2 + j, Color(c, c, c))
                strip.show()
            time.sleep(1 / 60) # 60 fps
    except KeyboardInterrupt:
        print('')
    finally:
        print("Exit.")
        midiin.close_port()
        del midiin
