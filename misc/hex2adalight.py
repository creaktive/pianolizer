#!/usr/bin/env python3
import serial
import colorsys
import sys
import re

from palette import Palette

LEDS_PER_KEY = 2
LED_OFFSET = 3

ttydev = sys.argv[1]
if not 'dev/tty' in ttydev:
    print("missing expected /dev/tty* as parameter")
    sys.exit(1)

ser = serial.Serial(ttydev, 115200)

def send_line(line):
    rgb_data = bytearray()
    rgb_data.extend([0] * 3 * LED_OFFSET * LEDS_PER_KEY)
    levels = [c / 255 for c in bytes.fromhex(match.group())]
    for rgb in palette.getKeyColors(levels):
        rgb_data.extend(rgb*LEDS_PER_KEY)

    l = int( len(rgb_data) / 3 ) -1
    hi = l>>8
    lo = l&0xff

    # Calculate the checksum
    chk = hi ^ lo ^ 0x55

    # Send the data
    data = b"Ada" + bytes([hi, lo, chk]) + rgb_data
    ser.write(data)
    ser.flush()

KEYS = 61
validator = re.compile(r'^\s*[0-9a-f]{%d}\b' % (KEYS * 2), re.IGNORECASE)
palette = Palette('palette.json')

skip=0
for line in sys.stdin:
    match = validator.match(line)
    if match:
        ## don't transfer every line, since this introduces lag/latency as the data has to go through a serial-line
        skip+=1
        if skip > 2:
            skip=0
            send_line(match.group())
    else:
        print(f'bad input: {line.strip()}')

ser.close()
