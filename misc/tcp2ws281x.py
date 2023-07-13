#!/usr/bin/env python3
'''
mido3-connect raspberrypi:9080 'piano:MIDI out 129:0'
'''
import sys
import time

import asyncore
from rpi_ws281x import PixelStrip, Color

from palette import Palette

KEYS = 88
FIRST_KEY = 21
LEDS_PER_KEY = 2
LED_OFFSET = 0

# LED strip configuration:
LED_COUNT = KEYS * LEDS_PER_KEY + LED_OFFSET
LED_PIN = 12          # GPIO pin connected to the pixels
LED_BRIGHTNESS = 255  # Set to 0 for darkest and 255 for brightest

palette = Palette('palette.json')
strip = PixelStrip(
    LED_COUNT,
    LED_PIN,
    brightness=LED_BRIGHTNESS,
)

class MIDIHandler(asyncore.dispatcher_with_send):
    def handle_read(self):
        message = self.recv(3)
        if len(message) == 3:
            status, key, velocity = bytearray(message)
            key -= FIRST_KEY

            color = None
            if status == 0x90:
                c = palette.getKeyColor(
                    key,
                    velocity / 127, # max velocity is 127
                )
                color = Color(c[0], c[1], c[2])
            elif status == 0x80:
                color = Color(0, 0, 0)
            else:
                print("WTF: %r" % message)

            if color is not None:
                for i in range(LEDS_PER_KEY):
                    strip.setPixelColor(LED_OFFSET + key * LEDS_PER_KEY + i, color)
                strip.show()

class MIDIServer(asyncore.dispatcher):
    def __init__(self, host, port):
        asyncore.dispatcher.__init__(self)
        self.create_socket()
        self.set_reuse_addr()
        self.bind((host, port))
        self.listen(5)

    def handle_accepted(self, sock, addr):
        #print('Incoming connection from %s' % repr(addr))
        handler = MIDIHandler(sock)

strip.begin()
server = MIDIServer('', 9080)
asyncore.loop()
