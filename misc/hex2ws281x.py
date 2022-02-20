#!/usr/bin/env python3
import fileinput

from rpi_ws281x import PixelStrip, Color

# LED strip configuration:
LED_COUNT = 61 * 2    # Number of LED pixels.
LED_PIN = 12          # GPIO pin connected to the pixels
LED_BRIGHTNESS = 255  # Set to 0 for darkest and 255 for brightest

if __name__ == '__main__':
    strip = PixelStrip(
        LED_COUNT,
        LED_PIN,
        brightness=LED_BRIGHTNESS,
    )
    strip.begin()

    for line in fileinput.input():
        levels = [c for c in bytes.fromhex(line.rstrip())]
        for key in range(len(levels)):
            color = Color(levels[key], levels[key], levels[key])
            for i in range(2):
                strip.setPixelColor(key * 2 + i, color)
        strip.show()