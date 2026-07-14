#!/usr/bin/env python3
"""Generate the first-pass SFX .wav files into assets/audio/sfx/.

Seven 8-bit-style one-shots synthesized from squares, sines, and noise
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
  ui_blip.wav         clean rising square, run-restart confirmation

Run from anywhere:  python3 tools/gen-sfx.py [out_dir]
"""
import math
import os
import random
import struct
import tempfile
import wave

SR = 22050


def _repo_root():
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def validated_out_dir(out_dir):
    """Canonicalize a caller-supplied output dir and require it to stay
    inside the repo or the system temp dir, so a bad CLI argument can't
    write outside those roots."""
    resolved = os.path.realpath(out_dir)
    allowed = (os.path.realpath(_repo_root()),
               os.path.realpath(tempfile.gettempdir()))
    for root in allowed:
        if resolved == root or resolved.startswith(root + os.sep):
            return resolved
    raise ValueError(f'output dir {out_dir!r} must be inside one of {allowed}')


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
    duration = 0.30
    n = int(SR * duration)
    rnd = random.Random(11)
    out = [0.0] * n
    for i, t, phase in sweep_phase(n, lambda t: 220.0 * math.exp(-t * 6.0) + 60.0):
        body = math.sin(2.0 * math.pi * phase)
        hiss = rnd.uniform(-1.0, 1.0) * min(t / 0.05, 1.0) * 0.35
        out[i] = envelope(t, duration, 7.0) * (0.75 * body + hiss)
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
    if out_dir is None:
        out_dir = os.path.join(_repo_root(), 'assets', 'audio', 'sfx')
    else:
        out_dir = validated_out_dir(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    for name, gen in SOUNDS.items():
        path = os.path.join(out_dir, name)
        samples = gen()
        write_wav(path, samples)
        print(f'wrote {path} ({len(samples) / SR:.2f}s)')


if __name__ == '__main__':
    import sys
    main(sys.argv[1] if len(sys.argv) > 1 else None)
