#!/usr/bin/env python3
"""End-to-end test of tools/gen-music-xm.py and the committed .xm assets.

Three layers of checking:
  1. Run the generator into a temp dir and structurally validate every
     output against the XM spec: header fields (including the header-size
     dword counting itself - the bug class that shipped once), packed
     pattern-cell decoding with exact byte accounting, instrument/sample
     chains, and non-silent sample data.
  2. Byte-compare the fresh output against the committed assets/audio
     files, so the generator and the checked-in assets can't drift apart.
  3. If libopenmpt is available (CI installs it; skipped gracefully
     elsewhere), load each file with the reference engine and check
     duration, pattern layout, and that rendered audio is audible.

Runs standalone: python3 tests/test_xm_assets.py
"""
import ctypes
import ctypes.util
import importlib.util
import os
import struct
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSET_DIR = os.path.join(REPO, 'assets', 'audio')

FILES = {
    # name -> (patterns, channels, instruments, duration_seconds,
    #          song_length, restart_position)
    # The stage gameplay themes carry the full song form: 12 order entries
    # over 10 unique patterns (the closing A' dedups to A''s patterns) with
    # the restart pointing past the intro. Everything else is a plain loop.
    'stage1_drums_bass.xm': (1, 4, 4, 6.31, 1, 0),
    'boss1_drums_bass.xm': (1, 4, 4, 6.31, 1, 0),
    'stage1_theme_a.xm': (10, 6, 6, 75.79, 12, 1),
    'stage1_theme_b.xm': (2, 6, 5, 12.62, 2, 0),
    'boss1_theme_a.xm': (2, 6, 5, 12.62, 2, 0),
    'boss1_theme_b.xm': (2, 6, 5, 12.62, 2, 0),
    'stage2_theme_a.xm': (10, 6, 6, 75.79, 12, 1),
    'boss2_theme_a.xm': (2, 6, 5, 12.62, 2, 0),
}

failures = []


def check(cond, msg):
    if not cond:
        failures.append(msg)
        print('FAIL:', msg)


def load_generator():
    path = os.path.join(REPO, 'tools', 'gen-music-xm.py')
    spec = importlib.util.spec_from_file_location('gen_music_xm', path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


# ------------------------------------------------- structural validation --

def decode_pattern(name, cells, rows, n_channels):
    """Decode packed pattern data, returning consumed byte count."""
    i = 0
    for _row in range(rows):
        for _ch in range(n_channels):
            b = cells[i]
            i += 1
            if b & 0x80:
                for flag in (0x01, 0x02, 0x04, 0x08, 0x10):
                    if b & flag:
                        i += 1
            else:
                i += 4  # unpacked: b is the note; inst/vol/effect/param follow
    check(i == len(cells),
          f'{name}: pattern decode consumed {i} of {len(cells)} packed bytes')


def validate_structure(name, data, exp_patterns, exp_channels, exp_instruments,
                       exp_song_len, exp_restart):
    check(data[0:17] == b'Extended Module: ', f'{name}: bad ID text')
    check(data[37] == 0x1A, f'{name}: bad 0x1A marker')

    header_size, = struct.unpack_from('<I', data, 60)
    # The size field counts from offset 60 and includes its own dword (276).
    # Writing 272 here once made every pattern decode as empty in OpenMPT.
    check(header_size == 276, f'{name}: header_size {header_size} != 276')

    (song_len, restart, n_channels, n_patterns, n_instruments, flags,
     _tempo, bpm) = struct.unpack_from('<HHHHHHHH', data, 64)
    check(n_channels == exp_channels, f'{name}: channels {n_channels}')
    check(n_patterns == exp_patterns, f'{name}: patterns {n_patterns}')
    check(n_instruments == exp_instruments, f'{name}: instruments {n_instruments}')
    check(song_len == exp_song_len, f'{name}: song length {song_len}')
    check(restart == exp_restart, f'{name}: restart position {restart}')
    # The loop must land inside the song, and every played order entry
    # must reference a real pattern (jar_xm reads these fields verbatim).
    check(restart < song_len, f'{name}: restart {restart} >= length {song_len}')
    order = data[80:80 + song_len]
    check(all(o < n_patterns for o in order),
          f'{name}: order table references pattern beyond {n_patterns}')
    check(flags & 1 == 1, f'{name}: linear frequency flag not set')
    check(bpm == 152, f'{name}: bpm {bpm} != 152')

    pos = 60 + header_size  # spec: pattern data starts here
    for p in range(n_patterns):
        phdr_len, packing, rows, packed_size = struct.unpack_from('<IBHH', data, pos)
        check(phdr_len == 9, f'{name}: pattern {p} header length {phdr_len}')
        check(packing == 0, f'{name}: pattern {p} packing type {packing}')
        check(rows == 64, f'{name}: pattern {p} rows {rows} != 64')
        decode_pattern(name, data[pos + phdr_len:pos + phdr_len + packed_size],
                       rows, n_channels)
        pos += phdr_len + packed_size

    for i in range(n_instruments):
        ihdr_size, = struct.unpack_from('<I', data, pos)
        n_samples, = struct.unpack_from('<H', data, pos + 27)
        check(n_samples == 1, f'{name}: instrument {i} has {n_samples} samples')
        sample_pos = pos + ihdr_size
        lengths = []
        for _s in range(n_samples):
            length, _ls, _ll, volume = struct.unpack_from('<iiiB', data, sample_pos)
            check(length > 0, f'{name}: instrument {i} sample empty')
            check(0 < volume <= 64, f'{name}: instrument {i} volume {volume}')
            lengths.append(length)
            sample_pos += 40
        pos = sample_pos
        for length in lengths:
            raw = data[pos:pos + length]
            check(len(raw) == length, f'{name}: instrument {i} sample truncated')
            # Delta-decode and confirm the waveform actually moves.
            old, lo, hi = 0, 0, 0
            for b in raw:
                d = b if b < 128 else b - 256
                old = (old + d + 128) % 256 - 128
                lo, hi = min(lo, old), max(hi, old)
            check(hi - lo > 0, f'{name}: instrument {i} sample is silent')
            pos += length

    check(pos == len(data),
          f'{name}: {len(data) - pos} trailing bytes unaccounted for')


# ------------------------------------------------ reference-engine check --

def find_libopenmpt():
    candidates = []
    env_dir = os.environ.get('SEAVIOUS_LIBOPENMPT_DIR')
    if env_dir:
        candidates.append(os.path.join(env_dir, 'libopenmpt.so.0'))
    found = ctypes.util.find_library('openmpt')
    if found:
        candidates.append(found)
    candidates += ['libopenmpt.so.0',
                   '/usr/lib/x86_64-linux-gnu/libopenmpt.so.0']
    for c in candidates:
        try:
            return ctypes.CDLL(c)
        except OSError:
            continue
    return None


def validate_with_libopenmpt(lib, name, data, exp_patterns, exp_duration):
    lib.openmpt_module_create_from_memory2.restype = ctypes.c_void_p
    lib.openmpt_module_create_from_memory2.argtypes = [
        ctypes.c_char_p, ctypes.c_size_t, ctypes.c_void_p, ctypes.c_void_p,
        ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int),
        ctypes.POINTER(ctypes.c_char_p), ctypes.c_void_p]
    lib.openmpt_module_get_duration_seconds.restype = ctypes.c_double
    lib.openmpt_module_get_duration_seconds.argtypes = [ctypes.c_void_p]
    lib.openmpt_module_get_num_patterns.restype = ctypes.c_int
    lib.openmpt_module_get_num_patterns.argtypes = [ctypes.c_void_p]
    lib.openmpt_module_read_interleaved_stereo.restype = ctypes.c_size_t
    lib.openmpt_module_read_interleaved_stereo.argtypes = [
        ctypes.c_void_p, ctypes.c_int, ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_int16)]
    lib.openmpt_module_destroy.argtypes = [ctypes.c_void_p]

    err = ctypes.c_int(0)
    errmsg = ctypes.c_char_p(None)
    mod = lib.openmpt_module_create_from_memory2(
        data, len(data), None, None, None, None,
        ctypes.byref(err), ctypes.byref(errmsg), None)
    check(bool(mod), f'{name}: libopenmpt failed to load the module')
    if not mod:
        return
    dur = lib.openmpt_module_get_duration_seconds(mod)
    check(abs(dur - exp_duration) < 0.1,
          f'{name}: libopenmpt duration {dur:.2f}s != ~{exp_duration}s')
    pats = lib.openmpt_module_get_num_patterns(mod)
    check(pats == exp_patterns, f'{name}: libopenmpt sees {pats} patterns')
    n = 48000 * 2
    buf = (ctypes.c_int16 * (n * 2))()
    got = lib.openmpt_module_read_interleaved_stereo(mod, 48000, n, buf)
    peak = max(abs(v) for v in buf[:got * 2]) if got else 0
    check(peak > 500, f'{name}: rendered audio is silent (peak {peak})')
    lib.openmpt_module_destroy(mod)


def main():
    gen = load_generator()
    lib = find_libopenmpt()
    if lib is None:
        print('note: libopenmpt not found, reference-engine checks skipped')

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

        for name, (patterns, channels, instruments, duration,
                   song_len, restart) in FILES.items():
            with open(os.path.join(tmp, name), 'rb') as f:
                data = f.read()
            validate_structure(name, data, patterns, channels, instruments,
                               song_len, restart)

            committed_path = os.path.join(ASSET_DIR, name)
            check(os.path.exists(committed_path),
                  f'{name}: missing from assets/audio')
            if os.path.exists(committed_path):
                with open(committed_path, 'rb') as f:
                    committed = f.read()
                check(committed == data,
                      f'{name}: committed asset differs from generator output '
                      f'- rerun tools/gen-music-xm.py and commit the result')

            if lib is not None:
                validate_with_libopenmpt(lib, name, data, patterns, duration)

    if failures:
        print(f'\n{len(failures)} failure(s)')
        return 1
    print(f'OK: {len(FILES)} XM files validated'
          + (' (including libopenmpt reference checks)' if lib else ''))
    return 0


if __name__ == '__main__':
    sys.exit(main())
