#!/usr/bin/env python3
import sys
import time

from rtmidi.midiutil import open_midiinput
from rpi_ws281x import PixelStrip, Color

from palette import Palette

KEYS = 61
LEDS_PER_KEY = 2
LED_OFFSET = 2

# LED strip configuration:
LED_COUNT = KEYS * LEDS_PER_KEY + LED_OFFSET
LED_PIN = 12          # GPIO pin connected to the pixels
LED_BRIGHTNESS = 255  # Set to 0 for darkest and 255 for brightest

class MidiInputHandler(object):
    def __init__(self, port, strip, first_key=36, palette_file='palette.json'):
        self.port = port
        self.strip = strip
        self.first_key = first_key
        self.palette = Palette(palette_file)

    def __call__(self, event, data=None):
        message, deltatime = event
        status, key, velocity = message
        key -= self.first_key

        color = None
        if status == 0x90:
            c = self.palette.getKeyColor(
                key,
                velocity * 2, # max velocity is 127; max brightness 255
            )
            color = Color(c[0], c[1], c[2])
        elif status == 0x80:
            color = Color(0, 0, 0)
        else:
            print("WTF: %r" % message)

        if color is not None:
            for i in range(LEDS_PER_KEY):
                self.strip.setPixelColor(LED_OFFSET + key * LEDS_PER_KEY + i, color)

            strip.show()

if __name__ == '__main__':
    strip = PixelStrip(
        LED_COUNT,
        LED_PIN,
        brightness=LED_BRIGHTNESS,
    )

    # Prompts user for MIDI input port, unless a valid port number or name
    # is given as the first argument on the command line.
    # API backend defaults to ALSA on Linux.
    port = sys.argv[1] if len(sys.argv) > 1 else None

    try:
        midiin, port_name = open_midiinput(port)
    except (EOFError, KeyboardInterrupt):
        sys.exit()

    midiin.set_callback(MidiInputHandler(port_name, strip))

    print("Entering main loop. Press Control-C to exit.")
    try:
        strip.begin()
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print('')
    finally:
        print("Exit.")
        midiin.close_port()
        del midiin
