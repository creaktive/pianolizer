#!/bin/bash
# invoked via /lib/systemd/system/pianolizer.service

for card in /proc/asound/*/pcm?c/info; do
    card=$(echo "${card}" | cut -d / -f 4)
    if [[ "${card}" != card* ]]; then
        CAPTURE_DEVICE="${card}"
    fi
done

SAMPLE_RATE=24000
CHANNELS=2
BUFFER_SIZE=240
THRESHOLD=0.05
KEYS=72
SKIP=0
GPIO=12

CONFIG=/boot/pianolizer.txt
if [ -f ${CONFIG} ]; then
    source ${CONFIG}
fi

if [ -z "${CAPTURE_DEVICE}" ]; then
    echo "No capture device"
    exit 1
fi

# This configuration is specific to seeed-voicecard
# https://www.seeedstudio.com/ReSpeaker-2-Mics-Pi-HAT.html
if [ "${CAPTURE_DEVICE}" = "seeed2micvoicec" ]; then
    # a reasonable microphone setting for seeed-voicecard
    amixer -c ${CAPTURE_DEVICE} sset 'ADC PCM' 100%
fi

# start pianolizer
arecord -c ${CHANNELS} -D "plughw:${CAPTURE_DEVICE}" -f FLOAT_LE -r ${SAMPLE_RATE} -t raw \
    | ./pianolizer -b ${BUFFER_SIZE} -c ${CHANNELS} -k ${KEYS} -s ${SAMPLE_RATE} -t ${THRESHOLD} \
    | misc/hex2ws281x.py --keys ${KEYS} --skip ${SKIP} --gpio ${GPIO}

exit 0
