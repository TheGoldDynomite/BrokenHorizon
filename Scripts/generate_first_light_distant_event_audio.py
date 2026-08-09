"""Generate deterministic distant-war event WAV sources for First Light."""

import argparse
import math
import random
import struct
import wave
from pathlib import Path


SAMPLE_RATE = 48000


def _write_wav(path, frames, force):
    if path.exists() and not force:
        print(f"SKIP {path.name}")
        return

    peak = max(max(abs(left), abs(right)) for left, right in frames) or 1.0
    gain = min(1.0, 0.88 / peak)
    pcm = bytearray()
    for left, right in frames:
        pcm.extend(struct.pack("<hh", int(left * gain * 32767), int(right * gain * 32767)))

    with wave.open(str(path), "wb") as output:
        output.setnchannels(2)
        output.setsampwidth(2)
        output.setframerate(SAMPLE_RATE)
        output.writeframes(pcm)
    print(f"WRITE {path.name} frames={len(frames)}")


def _render_artillery():
    duration = 4.5
    count = int(duration * SAMPLE_RATE)
    events = ((0.14, 1.0), (0.72, 0.30), (1.16, 0.16))
    channels = []
    for seed, channel_offset in ((4101, 0.0), (4102, 0.008)):
        rng = random.Random(seed)
        rumble_phase = channel_offset
        samples = []
        for index in range(count):
            time = index / SAMPLE_RATE
            value = 0.018 * math.sin(2.0 * math.pi * 31.0 * time)
            if time >= events[0][0]:
                elapsed = time - events[0][0]
                envelope = math.exp(-elapsed / 1.15)
                frequency = 57.0 - 24.0 * min(elapsed, 1.0)
                rumble_phase += 2.0 * math.pi * frequency / SAMPLE_RATE
                value += envelope * (
                    0.48 * math.sin(rumble_phase)
                    + 0.19 * math.sin(rumble_phase * 0.52)
                    + 0.08 * (rng.random() * 2.0 - 1.0)
                )

            for event_time, scale in events:
                elapsed = time - event_time
                if elapsed < 0.0:
                    continue
                value += scale * math.exp(-elapsed / 0.018) * (
                    0.34 * (rng.random() * 2.0 - 1.0)
                    + 0.10 * math.sin(2.0 * math.pi * (1800.0 + 400.0 * scale) * elapsed)
                )
                value += scale * math.exp(-elapsed / 0.54) * 0.075 * (
                    rng.random() * 2.0 - 1.0
                )

            samples.append(value)
        channels.append(samples)
    return list(zip(channels[0], channels[1]))


def _render_aircraft():
    duration = 8.0
    count = int(duration * SAMPLE_RATE)
    channels = []
    for seed, phase_offset, pan in ((5101, 0.0, -0.08), (5102, 0.31, 0.08)):
        rng = random.Random(seed)
        phase = phase_offset
        filtered_noise = 0.0
        samples = []
        for index in range(count):
            time = index / SAMPLE_RATE
            fade_in = min(1.0, time / 0.9)
            fade_out = min(1.0, max(0.0, (duration - time) / 1.4))
            envelope = fade_in * fade_out
            frequency = 82.0 + 26.0 * math.sin(2.0 * math.pi * time / duration * 0.72 + phase_offset)
            phase += 2.0 * math.pi * frequency / SAMPLE_RATE
            filtered_noise += (rng.random() * 2.0 - 1.0 - filtered_noise) * 0.0035
            turbine = (
                0.20 * math.sin(phase)
                + 0.08 * math.sin(phase * 2.01)
                + 0.035 * math.sin(phase * 3.02)
            )
            distant_wind = 0.08 * filtered_noise
            stereo_motion = 1.0 + pan * math.sin(2.0 * math.pi * time / duration)
            samples.append(envelope * stereo_motion * (turbine + distant_wind))
        channels.append(samples)
    return list(zip(channels[0], channels[1]))


def _render_small_arms():
    duration = 2.6
    count = int(duration * SAMPLE_RATE)
    events = ((0.22, 1.0), (0.50, 0.82), (0.79, 0.92), (1.14, 0.66), (1.55, 0.48))
    channels = []
    for seed, channel_offset in ((6101, 0.0), (6102, 0.006)):
        rng = random.Random(seed)
        samples = []
        for index in range(count):
            time = index / SAMPLE_RATE
            value = 0.012 * (rng.random() * 2.0 - 1.0)
            for event_time, scale in events:
                elapsed = time - event_time - channel_offset
                if elapsed < 0.0:
                    continue
                crack = math.exp(-elapsed / 0.012) * (
                    0.42 * (rng.random() * 2.0 - 1.0)
                    + 0.12 * math.sin(2.0 * math.pi * 3100.0 * elapsed)
                )
                body = math.exp(-elapsed / 0.085) * (
                    0.16 * math.sin(2.0 * math.pi * 118.0 * elapsed)
                    + 0.045 * (rng.random() * 2.0 - 1.0)
                )
                echo_elapsed = elapsed - 0.25
                echo = 0.0
                if echo_elapsed >= 0.0:
                    echo = 0.055 * math.exp(-echo_elapsed / 0.32) * math.sin(
                        2.0 * math.pi * 78.0 * echo_elapsed
                    )
                value += scale * (crack + body + echo)
            samples.append(value)
        channels.append(samples)
    return list(zip(channels[0], channels[1]))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--force", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    _write_wav(args.output / "SW_FirstLight_DistantArtillery.wav", _render_artillery(), args.force)
    _write_wav(args.output / "SW_FirstLight_DistantAircraft.wav", _render_aircraft(), args.force)
    _write_wav(args.output / "SW_FirstLight_DistantSmallArms.wav", _render_small_arms(), args.force)


if __name__ == "__main__":
    main()
