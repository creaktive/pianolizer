#!/bin/sh
SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
INPUT=${INPUT:-"$SCRIPT_DIR/../audio/chromatic.mp3"}
FILTERS=${FILTERS:-"asubcut=27,asupercut=20000"}
KEYS=${KEYS:-88}
REFERENCE=${REFERENCE:-48}
RESAMPLE=${RESAMPLE:-46536}
BUFFER_SIZE=${BUFFER:-554}
SMOOTHING=${SMOOTHING:-0.04}
THRESHOLD=${THRESHOLD:-0.05}
exec \
    ffmpeg -loglevel quiet -i $INPUT -ac 1 -af $FILTERS -ar $RESAMPLE -f f32le -c:a pcm_f32le - \
    | "$SCRIPT_DIR/../pianolizer" -a $SMOOTHING -b $BUFFER_SIZE -k $KEYS -r $REFERENCE -s $RESAMPLE -t $THRESHOLD
