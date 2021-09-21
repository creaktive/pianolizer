#!/bin/sh
sox -n saw.wav synth 2 saw 440
sox -n sin.wav synth 2 sin 440
sox -n squ.wav synth 2 squ 440
sox -n noise.wav synth 2 whitenoi
