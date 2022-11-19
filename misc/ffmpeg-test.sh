#!/bin/sh
INPUT=../audio/chromatic.mp3
RESAMPLE=8000
BUFFER_SIZE=2000
THRESHOLD=0.05
exec \
    ffmpeg -loglevel quiet -i $INPUT -ar $RESAMPLE -ac 1 -f f32le -c:a pcm_f32le - \
    | ../pianolizer -b $BUFFER_SIZE -s $RESAMPLE -t $THRESHOLD
