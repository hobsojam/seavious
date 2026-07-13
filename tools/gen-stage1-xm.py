#!/usr/bin/env python3
"""Generate assets/audio/stage1_drums_bass.xm: the shared drum+bass template.

152 BPM, 4/4, linear frequency table. 16 rows/bar (speed=6 ticks/row gives
exactly 4 rows/beat at this BPM). One 64-row pattern = 4 bars:
  - Drums (channels 1-3): fixed 2-bar loop (rows 0-31), repeated once more
    (rows 32-63) to fill the pattern, with a snare fill on every 2nd bar.
  - Bass (channel 4): 4-bar walking bass over I-vi-IV-V in A major
    (A - F#m - D - E), one chord per bar, quarter-note motion connecting
    each chord's root/3rd/5th with a passing tone into the next root.

All samples are synthesized at 8363 Hz (XM's fixed C-4 reference rate under
the linear frequency table) and triggered with relative_note=0/finetune=0,
so the tracker's own note-to-pitch math (which we don't otherwise touch)
comes out correct without per-note tuning tricks.
"""
import struct
import math
import random

SR = 8363  # XM C-4 reference sample rate (linear frequency table, note 49)

SEMI = {'C': 0, 'C#': 1, 'D': 2, 'D#': 3, 'E': 4, 'F': 5, 'F#': 6, 'G': 7,
        'G#': 8, 'A': 9, 'A#': 10, 'B': 11}


def note_num(name, octave):
    return 1 + 12 * octave + SEMI[name]


def to_i8(x):
    return max(-128, min(127, int(round(x))))


def delta_encode(raw):
    out = bytearray()
    old = 0
    for v in raw:
        d = (v - old) % 256
        if d >= 128:
            d -= 256
        out += struct.pack('b', d)
        old = v
    return bytes(out)


# ---------------------------------------------------------------- samples --

def gen_kick():
    dur = 0.18
    n = int(SR * dur)
    out = []
    phase = 0.0
    for i in range(n):
        t = i / SR
        freq = 50 + 150 * math.exp(-t * 18)
        phase += 2 * math.pi * freq / SR
        amp = math.exp(-t * 14)
        out.append(to_i8(110 * amp * math.sin(phase)))
    return out


def gen_snare():
    dur = 0.15
    n = int(SR * dur)
    out = []
    phase = 0.0
    rnd = random.Random(1)
    for i in range(n):
        t = i / SR
        amp = math.exp(-t * 22)
        phase += 2 * math.pi * 200 / SR
        tone = 0.35 * math.sin(phase)
        noise = rnd.uniform(-1, 1)
        out.append(to_i8(110 * amp * (tone + 0.8 * noise)))
    return out


def gen_hihat():
    dur = 0.07
    n = int(SR * dur)
    rnd = random.Random(2)
    raw_noise = [rnd.uniform(-1, 1) for _ in range(n)]
    out = []
    prev = 0.0
    for i in range(n):
        t = i / SR
        amp = math.exp(-t * 40)
        hp = raw_noise[i] - prev  # crude high-pass: emphasize high freq content
        prev = raw_noise[i]
        out.append(to_i8(90 * amp * hp))
    return out


def gen_bass_cycle():
    n = 32
    duty = 8  # 25% duty pulse
    return [100 if (i % n) < duty else -100 for i in range(n)]


# --------------------------------------------------------------- XM build --

def sample_header(length, loop_start, loop_len, volume, sample_type, name):
    name_b = name.encode('ascii')[:22].ljust(22, b'\x00')
    return struct.pack('<iiiBBBBbB', length, loop_start, loop_len, volume,
                        0, sample_type, 128, 0, 0) + name_b


def instrument_block(name, sample_bytes_header, sample_data):
    name_b = name.encode('ascii')[:22].ljust(22, b'\x00')
    base = struct.pack('<i22sBH', 243, name_b, 0, 1)  # header size, name, type, #samples
    ext = struct.pack('<i', 40)          # sample header size
    ext += b'\x00' * 96
    ext += b'\x00' * 48
    ext += b'\x00' * 48
    ext += struct.pack('<BBBBBBBBBBBBBBH', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
    ext += b'\x00\x00'  # reserved
    assert len(ext) == 214, len(ext)
    header = base + ext
    assert len(header) == 243, len(header)
    return header + sample_bytes_header + sample_data


def pack_empty():
    return b'\x80'


def pack_note(note, inst):
    return bytes([0x83, note, inst])


def build_pattern(rows, num_channels, events):
    """events: dict[(row, channel)] = (note, inst)"""
    data = bytearray()
    for row in range(rows):
        for ch in range(num_channels):
            ev = events.get((row, ch))
            if ev is None:
                data += pack_empty()
            else:
                data += pack_note(*ev)
    header = struct.pack('<IBHH', 9, 0, rows, len(data))
    return header + bytes(data)


def main():
    KICK, SNARE, HIHAT, BASS = 0, 1, 2, 3

    events = {}

    def drum_bar(base_row, fill=False):
        events[(base_row + 0, KICK)] = (49, 1)
        events[(base_row + 8, KICK)] = (49, 1)
        events[(base_row + 4, SNARE)] = (49, 2)
        if not fill:
            events[(base_row + 12, SNARE)] = (49, 2)
            hats = [0, 2, 4, 6, 8, 10, 12, 14]
        else:
            for r in (12, 13, 14, 15):
                events[(base_row + r, SNARE)] = (49, 2)
            hats = [0, 2, 4, 6, 8, 10]
        for r in hats:
            events[(base_row + r, HIHAT)] = (49, 3)

    drum_bar(0, fill=False)
    drum_bar(16, fill=True)
    drum_bar(32, fill=False)
    drum_bar(48, fill=True)

    bass_notes = [
        (0, ('A', 3)), (4, ('C#', 4)), (8, ('E', 4)), (12, ('F#', 3)),
        (16, ('F#', 3)), (20, ('A', 3)), (24, ('C#', 4)), (28, ('C#', 3)),
        (32, ('D', 3)), (36, ('F#', 3)), (40, ('A', 3)), (44, ('D#', 3)),
        (48, ('E', 3)), (52, ('G#', 3)), (56, ('B', 3)), (60, ('A', 3)),
    ]
    for row, (name, octave) in bass_notes:
        events[(row, BASS)] = (note_num(name, octave), 4)

    pattern = build_pattern(64, 4, events)

    kick = delta_encode(gen_kick())
    snare = delta_encode(gen_snare())
    hihat = delta_encode(gen_hihat())
    bass = delta_encode(gen_bass_cycle())

    kick_hdr = sample_header(len(kick), 0, 0, 56, 0x00, 'kick')
    snare_hdr = sample_header(len(snare), 0, 0, 52, 0x00, 'snare')
    hihat_hdr = sample_header(len(hihat), 0, 0, 32, 0x00, 'hihat')
    bass_hdr = sample_header(len(bass), 0, len(bass), 48, 0x01, 'bass_pulse25')

    inst_kick = instrument_block('kick', kick_hdr, kick)
    inst_snare = instrument_block('snare', snare_hdr, snare)
    inst_hihat = instrument_block('hihat', hihat_hdr, hihat)
    inst_bass = instrument_block('bass_pulse25', bass_hdr, bass)

    header_name = b'stage1 drums+bass'.ljust(20, b'\x00')[:20]
    tracker_name = b'seavious-gen'.ljust(20, b'\x00')[:20]
    order_table = bytes([0]) + b'\x00' * 255

    header_after_size = struct.pack('<HHHHHHHH',
                                     1,    # song length
                                     0,    # restart position
                                     4,    # channels
                                     1,    # patterns
                                     4,    # instruments
                                     1,    # flags: linear freq table
                                     6,    # default tempo (ticks/row)
                                     152)  # default BPM
    header_after_size += order_table
    assert len(header_after_size) == 272, len(header_after_size)

    xm = b'Extended Module: ' + header_name + b'\x1a' + tracker_name
    xm += struct.pack('<H', 0x0104)
    xm += struct.pack('<I', 272)
    xm += header_after_size
    xm += pattern
    xm += inst_kick + inst_snare + inst_hihat + inst_bass

    out_path = '/tmp/claude-1000/-home-james-dev-seavious/2f12700f-cf2c-48f4-b052-7201e0131b1f/scratchpad/stage1_drums_bass.xm'
    with open(out_path, 'wb') as f:
        f.write(xm)
    print('wrote', out_path, len(xm), 'bytes')


if __name__ == '__main__':
    main()
