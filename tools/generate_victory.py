"""Render Shadow Hunt's original victory cue as a stereo PCM WAV."""

import math
import struct
import wave
from pathlib import Path

RATE = 32_000
DURATION = 7.2
OUTPUT = Path(__file__).resolve().parents[1] / "assets/audio/music/victory.wav"

# Time in seconds and MIDI pitch. A plucked melody over D minor,
# with a bell-like high answer and a return to low D.
MELODY = [
    (0.16, 62), (0.59, 65), (0.97, 69), (1.55, 67), (1.92, 72),
    (2.42, 70), (2.86, 65), (3.22, 67), (3.67, 69), (4.19, 74),
    (4.82, 73), (5.21, 74),
]
HARMONY = [
    (0.00, (38, 50, 57)), (0.99, (45, 57, 65)), (1.93, (48, 55, 64)),
    (2.44, (46, 53, 62)), (3.25, (43, 50, 58)), (4.20, (45, 52, 61)),
    (5.21, (38, 50, 57, 65)),
]
BELLS = [(1.95, 72), (3.69, 76), (5.28, 81)]


def tone(mix, onset, pitch, volume, mode):
    frequency = 440.0 * 2 ** ((pitch - 69) / 12)
    attack, decay, harmonics = {
        "lead": (0.016, 3.0, ((1, 1), (2, 0.28), (3, 0.16), (4.01, 0.07))),
        "strings": (0.066, 1.68, ((1, 1), (2, 0.18), (3, 0.09))),
        "bell": (0.014, 2.55, ((1, 1), (2.01, 0.32), (3.12, 0.27), (4.23, 0.095))),
    }[mode]
    start = round(onset * RATE)
    count = min(len(mix) - start, math.ceil(RATE * (2.8 if mode == "strings" else 2.2)))
    radians_per_sample = math.tau * frequency / RATE
    for index in range(count):
        age = index / RATE
        envelope = min(1, age / attack) * math.exp(-age * decay)
        envelope *= min(1, (count - index) / (0.16 * RATE))
        phase = index * radians_per_sample
        sample = sum(weight * math.sin(phase * partial) for partial, weight in harmonics)
        mix[start + index] += volume * envelope * sample


def main():
    dry = [0.0] * round(DURATION * RATE)
    for onset, chord in HARMONY:
        for voice, pitch in enumerate(chord):
            tone(dry, onset + voice * 0.034, pitch, 0.22 if voice == 0 else 0.115, "strings")
    for onset, pitch in MELODY:
        tone(dry, onset, pitch, 0.39, "lead")
    for onset, pitch in BELLS:
        tone(dry, onset, pitch, 0.12, "bell")

    taps_left = [(round(delay * RATE), gain) for delay, gain in ((0.047, 0.16), (0.109, 0.11), (0.189, 0.06), (0.296, 0.032))]
    taps_right = [(round(delay * RATE), gain) for delay, gain in ((0.063, 0.15), (0.131, 0.10), (0.223, 0.06), (0.331, 0.026))]
    stereo = []
    for index, sample in enumerate(dry):
        fade = min(1, (len(dry) - 1 - index) / (0.36 * RATE))
        for taps in (taps_left, taps_right):
            reflections = sum(dry[index - delay] * gain for delay, gain in taps if index >= delay)
            stereo.append((sample + reflections) * fade)

    gain = 0.82 * 32767 / max(map(abs, stereo))
    with wave.open(str(OUTPUT), "wb") as output:
        output.setparams((2, 2, RATE, 0, "NONE", "not compressed"))
        output.writeframes(struct.pack(f"<{len(stereo)}h", *(round(sample * gain) for sample in stereo)))


if __name__ == "__main__":
    main()
