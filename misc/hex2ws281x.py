#!/usr/bin/env python3
import fileinput
import re

from rpi_ws281x import PixelStrip, Color

from palette import Palette

KEYS = 61
LEDS_PER_KEY = 2
LED_OFFSET = 2

# LED strip configuration:
LED_COUNT = KEYS * LEDS_PER_KEY + LED_OFFSET
LED_PIN = 12          # GPIO pin connected to the pixels
LED_BRIGHTNESS = 255  # Set to 0 for darkest and 255 for brightest

if __name__ == '__main__':
    strip = PixelStrip(
        LED_COUNT,
        LED_PIN,
        brightness=LED_BRIGHTNESS,
    )
    strip.begin()

    validator = re.compile(r'^[0-9a-f]{%d}$' % (KEYS * 2), re.IGNORECASE)
    palette = Palette('palette.json')

    try:
        for line in fileinput.input():
            match = validator.match(line)
            if not match:
                raise RuntimeError('bad input')

            levels = [c / 255 for c in bytes.fromhex(match.group())]
            leds = list(
                map(
                    lambda c: Color(c[0], c[1], c[2]),
                    palette.getKeyColors(levels)
                )
            )

            for key in range(len(leds)):
                color = leds[key]
                for i in range(LEDS_PER_KEY):
                    strip.setPixelColor(LED_OFFSET + key * LEDS_PER_KEY + i, color)
            strip.show()
    except KeyboardInterrupt:
        print('')
    finally:
        for i in range(LED_COUNT):
            strip.setPixelColor(i, Color(0, 0, 0))
        strip.show()
