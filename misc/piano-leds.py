#!/usr/bin/env python3
import sys
import time

from rtmidi.midiutil import open_midiinput
from rpi_ws281x import PixelStrip, Color

# LED strip configuration:
LED_COUNT = 61 * 2    # Number of LED pixels.
LED_PIN = 12          # GPIO pin connected to the pixels
LED_BRIGHTNESS = 255  # Set to 0 for darkest and 255 for brightest

class MidiInputHandler(object):
    def __init__(self, port, strip, first_key=36):
        self.port = port
        self.strip = strip
        self.first_key = first_key

    def __call__(self, event, data=None):
        message, deltatime = event
        status, key, velocity = message
        key -= self.first_key

        color = None
        if status == 0x90:
            c = velocity * 2 # max velocity is 127; max brightness 255
            color = Color(c, c, c)
        elif status == 0x80:
            color = Color(0, 0, 0)
        else:
            print("WTF: %r" % message)

        if color is not None:
            # 2 LEDs per key
            for i in range(2):
                self.strip.setPixelColor(key * 2 + i, color)

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
