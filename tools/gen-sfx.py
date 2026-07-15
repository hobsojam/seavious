#!/usr/bin/env python3
"""Generate the first-pass SFX .wav files into assets/audio/sfx/.

The game's 8-bit-style one-shots synthesized from squares, sines, and noise
(22050 Hz, 16-bit mono PCM), matching the chiptune music. Deterministic
(fixed noise seeds) so regenerating always produces identical bytes.
First-pass programmatic versions, same convention as the sprites and
music: refine in ChipTone later if a sound needs more character.

  gun_shot.wav        short bright square blip, fast pitch drop; plays on
                      every auto-fire shot so it has to stay tiny and quiet
  torpedo_launch.wav  low sine dive with a water-hiss tail
  torpedo_splash.wav  soft noise plip for unarmed direct hits
  explosion.wav       the armed-torpedo boom: heavy noise + sub thump
  air_pop.wav         drone kill: quick square drop + noise burst
  player_death.wav    long dramatic square dive with a noise swell
  relay_launch.wav    Relay Node drone launch: broadcast-y double warble
  mine_detonation.wav contact-mine blast: sharp bright crack + metal ring
  ui_blip.wav         clean rising square, run-restart confirmation

Run from anywhere:  python3 tools/gen-sfx.py [out_dir]
"""
import math
import os
import random
import struct
import sys
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib

SR = 22050


def square(phase, duty=0.5):
    return 1.0 if (phase % 1.0) < duty else -1.0


def sweep_phase(n, freq_fn):
    """Accumulate phase for a time-varying frequency; yields (i, t, phase)."""
    phase = 0.0
    for i in range(n):
        t = i / SR
        phase += freq_fn(t) / SR
        yield i, t, phase


def envelope(t, duration, decay):
    return math.exp(-t * decay) * (1.0 - t / duration)


def gen_gun_shot():
    duration = 0.07
    n = int(SR * duration)
    out = [0.0] * n
    for i, t, phase in sweep_phase(n, lambda t: 900.0 * math.exp(-t * 25.0) + 300.0):
        out[i] = 0.9 * envelope(t, duration, 45.0) * square(phase, duty=0.25)
    return out


def gen_torpedo_launch():
    # Covers the whole flight, not just the launch: the max torpedo run is
    # ~0.85s (170px at 200px/s), so the dive thunk blends into a propulsion
    # bed that lasts until impact would cut it off (audio.c stops this
    # sound on the impact event, and on player death mid-flight).
    duration = 0.95
    n = int(SR * duration)
    rnd = random.Random(11)
    out = [0.0] * n
    prev = 0.0
    for i, t, phase in sweep_phase(n, lambda t: 220.0 * math.exp(-t * 6.0) + 60.0):
        dive = math.sin(2.0 * math.pi * phase) * math.exp(-t * 9.0)
        # Propulsion: lowpassed noise with a slow throb over a low hum.
        prev += 0.22 * (rnd.uniform(-1.0, 1.0) - prev)
        throb = 0.75 + 0.25 * math.sin(2.0 * math.pi * 8.0 * t)
        bed = min(t / 0.2, 1.0) * throb * (1.8 * prev + 0.3 * math.sin(2.0 * math.pi * 90.0 * t)) * 0.45
        fade = min(1.0, (duration - t) / 0.08)
        out[i] = (0.8 * dive + bed) * fade
    return out


def gen_torpedo_splash():
    duration = 0.15
    n = int(SR * duration)
    rnd = random.Random(12)
    out = [0.0] * n
    prev = 0.0
    for i in range(n):
        t = i / SR
        # One-pole lowpass on noise keeps the plip watery instead of harsh.
        prev += 0.25 * (rnd.uniform(-1.0, 1.0) - prev)
        blip = 0.4 * math.sin(2.0 * math.pi * 260.0 * t)
        out[i] = envelope(t, duration, 28.0) * (2.2 * prev + blip)
    return out


def gen_explosion():
    duration = 0.50
    n = int(SR * duration)
    rnd = random.Random(13)
    out = [0.0] * n
    prev = 0.0
    for i in range(n):
        t = i / SR
        prev += 0.18 * (rnd.uniform(-1.0, 1.0) - prev)
        thump = 0.6 * math.sin(2.0 * math.pi * 55.0 * t) * math.exp(-t * 14.0)
        out[i] = envelope(t, duration, 6.5) * 2.6 * prev + thump
    return out


def gen_air_pop():
    duration = 0.18
    n = int(SR * duration)
    rnd = random.Random(14)
    out = [0.0] * n
    for i, t, phase in sweep_phase(n, lambda t: 660.0 * math.exp(-t * 18.0) + 120.0):
        tone = square(phase, duty=0.5)
        burst = rnd.uniform(-1.0, 1.0) * math.exp(-t * 60.0) * 0.6
        out[i] = envelope(t, duration, 18.0) * (0.7 * tone + burst)
    return out


def gen_player_death():
    duration = 0.65
    n = int(SR * duration)
    rnd = random.Random(15)
    out = [0.0] * n
    for i, t, phase in sweep_phase(n, lambda t: 880.0 * math.exp(-t * 4.5) + 55.0):
        tone = square(phase, duty=0.5)
        swell = rnd.uniform(-1.0, 1.0) * min(t / 0.3, 1.0) * 0.5
        out[i] = envelope(t, duration, 4.0) * (0.65 * tone + swell)
    return out


def gen_relay_launch():
    # Broadcast-y double warble: the Relay Node "transmitting" a drone
    # into the air - two quick rising sine chirps with a soft edge.
    duration = 0.22
    n = int(SR * duration)
    out = [0.0] * n
    for i, t, phase in sweep_phase(n, lambda t: 460.0 + 420.0 * ((t * 9.0) % 1.0)):
        out[i] = 0.7 * envelope(t, duration, 10.0) * math.sin(2.0 * math.pi * phase)
    return out


def gen_mine_detonation():
    # Contact detonation: harder and shorter than the torpedo boom - a
    # sharp bright crack with a metallic ring instead of a rolling
    # explosion, so it reads as "you clipped a mine", not "torpedo hit".
    duration = 0.32
    n = int(SR * duration)
    rnd = random.Random(16)
    out = [0.0] * n
    prev = 0.0
    for i in range(n):
        t = i / SR
        # Lighter lowpass than the explosion keeps the noise bright.
        prev += 0.45 * (rnd.uniform(-1.0, 1.0) - prev)
        crack = 2.4 * prev * math.exp(-t * 16.0)
        ring = 0.35 * math.sin(2.0 * math.pi * 720.0 * t) * math.exp(-t * 18.0)
        thump = 0.5 * math.sin(2.0 * math.pi * 70.0 * t) * math.exp(-t * 20.0)
        out[i] = envelope(t, duration, 8.0) * (crack + ring) + thump
    return out


def gen_ui_blip():
    duration = 0.09
    n = int(SR * duration)
    out = [0.0] * n
    for i, t, phase in sweep_phase(n, lambda t: 520.0 + 5800.0 * t):
        out[i] = 0.8 * envelope(t, duration, 20.0) * square(phase, duty=0.5)
    return out


SOUNDS = {
    'gun_shot.wav': gen_gun_shot,
    'torpedo_launch.wav': gen_torpedo_launch,
    'torpedo_splash.wav': gen_torpedo_splash,
    'explosion.wav': gen_explosion,
    'air_pop.wav': gen_air_pop,
    'player_death.wav': gen_player_death,
    'relay_launch.wav': gen_relay_launch,
    'mine_detonation.wav': gen_mine_detonation,
    'ui_blip.wav': gen_ui_blip,
}


def write_wav(path, samples):
    frames = bytearray()
    for s in samples:
        v = max(-1.0, min(1.0, s))
        frames += struct.pack('<h', int(v * 32000))
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(bytes(frames))


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'audio', 'sfx')
    os.makedirs(out_dir, exist_ok=True)
    for name, gen in SOUNDS.items():
        path = os.path.join(out_dir, name)
        samples = gen()
        write_wav(path, samples)
        print(f'wrote {path} ({len(samples) / SR:.2f}s)')


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
