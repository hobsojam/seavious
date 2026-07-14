#!/usr/bin/env python3
"""End-to-end test of tools/gen-sfx.py and the committed SFX .wav assets.

Runs the generator into a temp dir, validates every output's WAV format
and audible content, checks the output-dir guard, and byte-compares
against the committed assets/audio/sfx files so the generator and the
checked-in assets can't drift apart.

Runs standalone: python3 tests/test_sfx_assets.py
"""
import importlib.util
import os
import struct
import sys
import tempfile
import wave

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET_DIR = os.path.join(REPO, 'assets', 'audio', 'sfx')

FILES = {
    # name -> (min_duration_s, max_duration_s)
    'gun_shot.wav': (0.05, 0.10),
    'torpedo_launch.wav': (0.25, 0.35),
    'torpedo_splash.wav': (0.10, 0.20),
    'explosion.wav': (0.45, 0.55),
    'air_pop.wav': (0.15, 0.22),
    'player_death.wav': (0.60, 0.70),
    'ui_blip.wav': (0.07, 0.12),
}

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print('FAIL:', msg)


def load_generator():
    path = os.path.join(REPO, 'tools', 'gen-sfx.py')
    spec = importlib.util.spec_from_file_location('gen_sfx', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def validate_wav(name, path, min_dur, max_dur):
    with wave.open(path, 'rb') as w:
        check(w.getnchannels() == 1, f'{name}: {w.getnchannels()} channels, want mono')
        check(w.getsampwidth() == 2, f'{name}: sample width {w.getsampwidth()}, want 16-bit')
        check(w.getframerate() == 22050, f'{name}: rate {w.getframerate()}, want 22050')
        frames = w.readframes(w.getnframes())
        duration = w.getnframes() / w.getframerate()
        check(min_dur <= duration <= max_dur,
              f'{name}: duration {duration:.2f}s outside [{min_dur}, {max_dur}]')
    samples = struct.unpack(f'<{len(frames) // 2}h', frames)
    peak = max(abs(s) for s in samples)
    check(peak > 8000, f'{name}: peak {peak} too quiet - synthesis broken?')
    check(peak <= 32000, f'{name}: peak {peak} clipping past the write ceiling')


def main():
    gen = load_generator()

    # The output-dir guard must reject paths outside the repo/system temp.
    try:
        gen.genlib.validated_out_dir('/definitely/not/allowed')
        check(False, 'validated_out_dir accepted a path outside allowed roots')
    except ValueError:
        pass

    with tempfile.TemporaryDirectory() as tmp:
        gen.main(tmp)

        generated = sorted(os.listdir(tmp))
        check(generated == sorted(FILES),
              f'generator wrote {generated}, expected {sorted(FILES)}')

        for name, (min_dur, max_dur) in FILES.items():
            path = os.path.join(tmp, name)
            validate_wav(name, path, min_dur, max_dur)

            committed_path = os.path.join(ASSET_DIR, name)
            check(os.path.exists(committed_path), f'{name}: missing from assets/audio/sfx')
            if os.path.exists(committed_path):
                with open(path, 'rb') as f:
                    data = f.read()
                with open(committed_path, 'rb') as f:
                    committed = f.read()
                check(committed == data,
                      f'{name}: committed asset differs from generator output '
                      f'- rerun tools/gen-sfx.py and commit the result')

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print(f'OK: {len(FILES)} SFX files validated')
    return 0


if __name__ == '__main__':
    sys.exit(main())
