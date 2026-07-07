#!/usr/bin/env bash
# Nagrywa wyjście FiSynth standalone do WAV (PipeWire).
#
# Użycie: record_fisynth.sh [plik.wav] [sekundy] [--monitor]
#   domyślnie: fisynth.wav, 5 s, wprost z node'a aplikacji
#   --monitor: nagrywa monitor domyślnego sinka (wszystko, co gra system)
#
# Tło: FiSynth (JUCE) gra przez API ALSA -> pipewire-alsa wystawia go w grafie
# PipeWire jako node "alsa_playback.FiSynth". `pw-record --target <sink>` BEZ
# flagi stream.capture.sink podpina się do domyślnego ŹRÓDŁA (wejście analogowe
# z DC offsetem = śmieci), dlatego zawsze nagrywamy z node'a aplikacji albo
# jawnie z monitora sinka.
set -euo pipefail

OUT="${1:-fisynth.wav}"
SECS="${2:-5}"
MODE="${3:-app}"

if [[ "$MODE" == "--monitor" ]]; then
    exec timeout "$SECS" pw-record -P '{ stream.capture.sink = true }' \
        --target "$(pactl get-default-sink)" "$OUT" || true
fi

# Czekaj aż node aplikacji pojawi się w grafie (do ~5 s).
for _ in $(seq 50); do
    if pw-link -o | grep -q "alsa_playback.FiSynth"; then
        break
    fi
    sleep 0.1
done

if ! pw-link -o | grep -q "alsa_playback.FiSynth"; then
    echo "BŁĄD: nie widzę node'a alsa_playback.FiSynth — czy standalone działa?" >&2
    exit 1
fi

timeout "$SECS" pw-record --target alsa_playback.FiSynth "$OUT" || true
echo "nagrano: $OUT ($SECS s)"
