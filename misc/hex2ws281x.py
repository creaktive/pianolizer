#!/usr/bin/env python3
import argparse
import fileinput
import re
import sys

from rpi_ws281x import PixelStrip, Color

from palette import Palette

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Pipe pixel colors to a LED strip')
    parser.add_argument('--keys', default=61, type=int, help='How many keys on the piano keyboard')
    parser.add_argument('--leds-per-key', default=2, type=int, help='How many LEDs per key')
    parser.add_argument('--skip', default=0, type=int, help='Skip first N LED addresses')
    parser.add_argument('--gpio', default=12, type=int, help='GPIO pin connected to the LED strip')
    parser.add_argument('--brightness', default=255, type=int, help='Set to 0 for darkest and 255 for brightest')
    args = parser.parse_args()
    sys.argv = []

    KEYS = args.keys
    LEDS_PER_KEY = args.leds_per_key
    LED_OFFSET = args.skip
    LED_PIN = args.gpio
    LED_BRIGHTNESS = args.brightness
    LED_COUNT = KEYS * LEDS_PER_KEY + LED_OFFSET

    strip = PixelStrip(
        LED_COUNT,
        LED_PIN,
        brightness=LED_BRIGHTNESS,
    )
    strip.begin()

    validator = re.compile(r'^\s*[0-9a-f]{%d}\b' % (KEYS * 2), re.IGNORECASE)
    palette = Palette('palette.json')

    try:
        for line in fileinput.input():
            match = validator.match(line)
            if match:
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
            else:
                print(f'bad input: {line.strip()}')
    except KeyboardInterrupt:
        print('')
    finally:
        for i in range(LED_COUNT):
            strip.setPixelColor(i, Color(0, 0, 0))
        strip.show()
