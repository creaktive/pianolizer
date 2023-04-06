#!/bin/sh
sox -r 44100 -n saw.wav synth 3 saw 110
sox -r 44100 -n sin.wav synth 3 sin 440
sox -r 44100 -n squ.wav synth 3 squ 110
sox -r 44100 -n noise.wav synth 3 pink
