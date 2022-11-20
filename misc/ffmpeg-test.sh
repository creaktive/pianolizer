#!/bin/sh
BUFFER_SIZE=${BUFFER:-512}
FILTERS=${FILTERS:-"asubcut=27,asupercut=20000"}
INPUT=${INPUT:-"../audio/chromatic.mp3"}
KEYS=${KEYS:-88}
REFERENCE=${REFERENCE:-48}
RESAMPLE=${RESAMPLE:-46520}
SMOOTHING=${SMOOTHING:-0.04}
THRESHOLD=${THRESHOLD:-0.05}
exec \
    ffmpeg -loglevel quiet -i $INPUT -ac 1 -af $FILTERS -ar $RESAMPLE -f f32le -c:a pcm_f32le - \
    | ../pianolizer -a $SMOOTHING -b $BUFFER_SIZE -k $KEYS -r $REFERENCE -s $RESAMPLE -t $THRESHOLD
