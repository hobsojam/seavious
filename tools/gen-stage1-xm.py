#!/usr/bin/env python3
"""Generate the stage 1 music .xm files into assets/audio/:

  stage1_drums_bass.xm  - the shared drum+bass template (no lead)
  stage1_theme_a.xm     - template + candidate lead tune A ("anthem")
  stage1_theme_b.xm     - template + candidate lead tune B ("driver")
  boss1_drums_bass.xm   - boss backing: same drums, bass in parallel minor
  boss1_theme_a.xm      - boss backing + candidate boss tune A ("siren")
  boss1_theme_b.xm      - boss backing + candidate boss tune B ("hammer")

152 BPM, 4/4, linear frequency table. 16 rows/bar (speed=6 ticks/row gives
exactly 4 rows/beat at this BPM). Each 64-row pattern = 4 bars:
  - Drums (channels 1-3): fixed 2-bar loop (rows 0-31), repeated once more
    (rows 32-63) to fill the pattern, with a snare fill on every 2nd bar.
  - Bass (channel 4): 4-bar walking bass, one chord per bar, quarter-note
    motion connecting each chord's root/3rd/5th with a passing tone into
    the next root. Stage: I-vi-IV-V in A major (A - F#m - D - E). Boss:
    the parallel-minor reharmonization i-VI-iv-v (Am - F - Dm - Em) with
    the identical rhythm and contour - only the color-note pitches move
    (C#->C, F#->F, G#->G), which is the design's whole point: the boss
    switch darkens the same music rather than replacing it.
  - Lead (channel 5, theme files only): an 8-bar tune spanning two passes
    of the bass loop, so each theme is two 64-row patterns played in order.

Stage tune A ("anthem"): long held notes, mostly stepwise, chord tones on
strong beats — a broad heroic line that leaves space over the busy bass.
Ends on the leading tone (G#) so the loop restart resolves onto A.

Stage tune B ("driver"): one syncopated arpeggio cell (with an off-beat
push on the "2-and-a-half") transposed through each chord — denser and
more chiptune-idiomatic, closer to the classic shooter energy. Bar 8
breaks the cell with a run-up and a breath before the loop.

Boss tune A ("siren"): a high wailing lament — long notes descending
A5-G5-F5-E5 across the first pass, collapsing into a lower register for
the second, then climbing back with a chromatic G#5 lift at the loop seam
(the harmonic-minor leading tone resolving onto A). Slow-moving dread
over the churning bass.

Boss tune B ("hammer"): relentless straight 8ths all bar, an arpeggio
grind up each chord capped with a half-step neighbor stab (the b6 F over
Am, the b9 F over Em) and a chromatic G#4 pull-back — mechanical menace,
denser than anything in the stage themes. Bar 8 breaks into the same
G#5 seam lift as the siren so both resolve identically.

All tunes stay in A natural minor for the boss (G# appears only as the
chromatic leading tone at the loop seam), so any boss tune remains
pluggable over the same minor backing.

All samples are synthesized at 8363 Hz (XM's fixed C-4 reference rate under
the linear frequency table) and triggered with relative_note=0/finetune=0,
so the tracker's own note-to-pitch math (which we don't otherwise touch)
comes out correct without per-note tuning tricks.
"""
import math
import os
import random
import struct

SR = 8363  # XM C-4 reference sample rate (linear frequency table, note 49)
KEY_OFF = 97

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


def gen_pulse_cycle(duty_frac):
    n = 32
    duty = int(n * duty_frac)
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
    if inst is None:  # bare key-off, no instrument byte
        return bytes([0x81, note])
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


def build_xm(module_name, n_channels, patterns, instruments):
    header_name = module_name.encode('ascii').ljust(20, b'\x00')[:20]
    tracker_name = b'seavious-gen'.ljust(20, b'\x00')[:20]
    order_table = bytes(range(len(patterns))) + b'\x00' * (256 - len(patterns))

    header_after_size = struct.pack('<HHHHHHHH',
                                     len(patterns),  # song length
                                     0,              # restart position
                                     n_channels,
                                     len(patterns),
                                     len(instruments),
                                     1,    # flags: linear freq table
                                     6,    # default tempo (ticks/row)
                                     152)  # default BPM
    header_after_size += order_table
    assert len(header_after_size) == 272, len(header_after_size)

    xm = b'Extended Module: ' + header_name + b'\x1a' + tracker_name
    xm += struct.pack('<H', 0x0104)
    # Header size is counted from its own offset (60) and *includes* this
    # dword, so it's 4 + the 16 bytes of song fields + the 256-byte order
    # table = 276. Writing 272 here shifts a conforming reader's pattern
    # start 4 bytes early and every pattern decodes as empty.
    xm += struct.pack('<I', 4 + len(header_after_size))
    xm += header_after_size
    for p in patterns:
        xm += p
    for inst in instruments:
        xm += inst
    return xm


# ------------------------------------------------------------ note tables --

# Walking bass, one note per beat. Same rhythm and contour in both modes;
# only the color notes differ (major 3rds/6ths/7ths flatten). The last note
# of bars 1 and 4 anticipates the next root; bars 2 and 3 approach the next
# root chromatically from below.
BASS_MAJOR = [  # I-vi-IV-V: A - F#m - D - E
    (0, ('A', 3)), (4, ('C#', 4)), (8, ('E', 4)), (12, ('F#', 3)),
    (16, ('F#', 3)), (20, ('A', 3)), (24, ('C#', 4)), (28, ('C#', 3)),
    (32, ('D', 3)), (36, ('F#', 3)), (40, ('A', 3)), (44, ('D#', 3)),
    (48, ('E', 3)), (52, ('G#', 3)), (56, ('B', 3)), (60, ('A', 3)),
]

BASS_MINOR = [  # i-VI-iv-v: Am - F - Dm - Em (parallel minor, same roots)
    (0, ('A', 3)), (4, ('C', 4)), (8, ('E', 4)), (12, ('F', 3)),
    (16, ('F', 3)), (20, ('A', 3)), (24, ('C', 4)), (28, ('C#', 3)),
    (32, ('D', 3)), (36, ('F', 3)), (40, ('A', 3)), (44, ('D#', 3)),
    (48, ('E', 3)), (52, ('G', 3)), (56, ('B', 3)), (60, ('A', 3)),
]


def backing_events(bass_notes):
    """Drum + bass events for one 64-row (4-bar) pattern."""
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

    for row, (name, octave) in bass_notes:
        events[(row, BASS)] = (note_num(name, octave), 4)

    return events


# Lead tunes: (bar, row_in_bar, note_name, octave); 'OFF' = key-off.
# 8 bars over two passes of the A - F#m - D - E loop. Chord per bar:
#   bars 1/5: A    bars 2/6: F#m    bars 3/7: D    bars 4/8: E

TUNE_A = [  # "anthem" - long notes, stepwise, space over the bass
    (0, 0, 'A', 4), (0, 2, 'C#', 5), (0, 4, 'E', 5), (0, 12, 'F#', 5),
    (1, 0, 'C#', 5), (1, 8, 'A', 4), (1, 12, 'B', 4),
    (2, 0, 'D', 5), (2, 8, 'F#', 5), (2, 12, 'E', 5),
    (3, 0, 'B', 4), (3, 8, 'G#', 4), (3, 12, 'B', 4), (3, 14, 'C#', 5),
    (4, 0, 'E', 5), (4, 8, 'F#', 5), (4, 12, 'E', 5),
    (5, 0, 'C#', 5), (5, 8, 'B', 4), (5, 12, 'A', 4),
    (6, 0, 'F#', 5), (6, 4, 'E', 5), (6, 8, 'D', 5), (6, 12, 'B', 4),
    # leading tone held to the loop edge resolves onto bar 1's A
    (7, 0, 'B', 4), (7, 8, 'G#', 4),
]

TUNE_B = [  # "driver" - syncopated arpeggio cell transposed through the chords
    (0, 0, 'E', 5), (0, 2, 'C#', 5), (0, 4, 'A', 4), (0, 7, 'C#', 5),
    (0, 8, 'E', 5), (0, 12, 'A', 5),
    (1, 0, 'C#', 5), (1, 2, 'A', 4), (1, 4, 'F#', 4), (1, 7, 'A', 4),
    (1, 8, 'C#', 5), (1, 12, 'F#', 5),
    (2, 0, 'D', 5), (2, 2, 'A', 4), (2, 4, 'F#', 4), (2, 7, 'A', 4),
    (2, 8, 'D', 5), (2, 12, 'F#', 5),
    (3, 0, 'E', 5), (3, 2, 'B', 4), (3, 4, 'G#', 4), (3, 7, 'B', 4),
    (3, 8, 'E', 5), (3, 12, 'D', 5),
    (4, 0, 'E', 5), (4, 2, 'C#', 5), (4, 4, 'A', 4), (4, 7, 'C#', 5),
    (4, 8, 'E', 5), (4, 12, 'A', 5),
    (5, 0, 'C#', 5), (5, 2, 'A', 4), (5, 4, 'F#', 4), (5, 7, 'A', 4),
    (5, 8, 'C#', 5), (5, 12, 'F#', 5),
    (6, 0, 'D', 5), (6, 2, 'A', 4), (6, 4, 'F#', 4), (6, 7, 'A', 4),
    (6, 8, 'D', 5), (6, 12, 'F#', 5),
    # bar 8 breaks the cell: run-up, then a breath before the loop restarts
    (7, 0, 'E', 5), (7, 2, 'B', 4), (7, 4, 'G#', 4), (7, 7, 'B', 4),
    (7, 8, 'E', 5), (7, 10, 'F#', 5), (7, 12, 'G#', 5), (7, 14, 'OFF', 0),
]

# Boss tunes over the minor backing. Chord per bar:
#   bars 1/5: Am    bars 2/6: F    bars 3/7: Dm    bars 4/8: Em

TUNE_BOSS_A = [  # "siren" - wailing descent, register collapse, seam lift
    (0, 0, 'A', 5), (0, 12, 'G', 5),
    (1, 0, 'F', 5), (1, 12, 'E', 5),
    (2, 0, 'D', 5), (2, 8, 'F', 5), (2, 12, 'E', 5),
    (3, 0, 'E', 5), (3, 12, 'D', 5),
    (4, 0, 'C', 5), (4, 8, 'B', 4), (4, 12, 'A', 4),
    (5, 0, 'A', 4), (5, 8, 'C', 5), (5, 12, 'D', 5),
    (6, 0, 'D', 5), (6, 4, 'E', 5), (6, 8, 'F', 5), (6, 12, 'G', 5),
    # chromatic leading tone at the seam resolves onto bar 1's A5
    (7, 0, 'G', 5), (7, 8, 'E', 5), (7, 12, 'G#', 5),
]

TUNE_BOSS_B = [  # "hammer" - straight-8ths arpeggio grind, half-step stabs
    (0, 0, 'A', 4), (0, 2, 'C', 5), (0, 4, 'E', 5), (0, 6, 'F', 5),
    (0, 8, 'E', 5), (0, 10, 'C', 5), (0, 12, 'A', 4), (0, 14, 'G#', 4),
    (1, 0, 'A', 4), (1, 2, 'C', 5), (1, 4, 'F', 5), (1, 6, 'G', 5),
    (1, 8, 'F', 5), (1, 10, 'C', 5), (1, 12, 'A', 4), (1, 14, 'F', 4),
    (2, 0, 'F', 4), (2, 2, 'A', 4), (2, 4, 'D', 5), (2, 6, 'E', 5),
    (2, 8, 'D', 5), (2, 10, 'A', 4), (2, 12, 'F', 4), (2, 14, 'E', 4),
    (3, 0, 'G', 4), (3, 2, 'B', 4), (3, 4, 'E', 5), (3, 6, 'F', 5),
    (3, 8, 'E', 5), (3, 10, 'B', 4), (3, 12, 'G', 4), (3, 14, 'B', 4),
    (4, 0, 'A', 4), (4, 2, 'C', 5), (4, 4, 'E', 5), (4, 6, 'F', 5),
    (4, 8, 'E', 5), (4, 10, 'C', 5), (4, 12, 'A', 4), (4, 14, 'G#', 4),
    (5, 0, 'A', 4), (5, 2, 'C', 5), (5, 4, 'F', 5), (5, 6, 'G', 5),
    (5, 8, 'F', 5), (5, 10, 'C', 5), (5, 12, 'A', 4), (5, 14, 'F', 4),
    (6, 0, 'F', 4), (6, 2, 'A', 4), (6, 4, 'D', 5), (6, 6, 'E', 5),
    (6, 8, 'D', 5), (6, 10, 'A', 4), (6, 12, 'F', 4), (6, 14, 'E', 4),
    # bar 8: the grind breaks upward into the same chromatic seam lift
    (7, 0, 'G', 4), (7, 2, 'B', 4), (7, 4, 'E', 5), (7, 8, 'E', 5),
    (7, 10, 'F', 5), (7, 12, 'G', 5), (7, 14, 'G#', 5),
]


def lead_events(tune, first_bar, lead_channel, lead_inst):
    """Events for bars [first_bar, first_bar+4) mapped into one pattern."""
    events = {}
    for bar, row_in_bar, name, octave in tune:
        if not first_bar <= bar < first_bar + 4:
            continue
        row = (bar - first_bar) * 16 + row_in_bar
        if name == 'OFF':
            events[(row, lead_channel)] = (KEY_OFF, None)
        else:
            events[(row, lead_channel)] = (note_num(name, octave), lead_inst)
    return events


# ------------------------------------------------------------------- main --

def main(out_dir=None):
    if out_dir is None:
        out_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                               'assets', 'audio')
    os.makedirs(out_dir, exist_ok=True)

    kick = delta_encode(gen_kick())
    snare = delta_encode(gen_snare())
    hihat = delta_encode(gen_hihat())
    bass = delta_encode(gen_pulse_cycle(0.25))
    lead = delta_encode(gen_pulse_cycle(0.50))

    backing_instruments = [
        instrument_block('kick', sample_header(len(kick), 0, 0, 56, 0x00, 'kick'), kick),
        instrument_block('snare', sample_header(len(snare), 0, 0, 52, 0x00, 'snare'), snare),
        instrument_block('hihat', sample_header(len(hihat), 0, 0, 32, 0x00, 'hihat'), hihat),
        instrument_block('bass_pulse25',
                         sample_header(len(bass), 0, len(bass), 48, 0x01, 'bass_pulse25'), bass),
    ]
    lead_instrument = instrument_block(
        'lead_square50',
        sample_header(len(lead), 0, len(lead), 44, 0x01, 'lead_square50'), lead)

    def write(filename, xm):
        path = os.path.join(out_dir, filename)
        with open(path, 'wb') as f:
            f.write(xm)
        print('wrote', path, len(xm), 'bytes')

    # Templates: backing only, one 4-bar pattern, 4 channels.
    for filename, module_name, bass in [
        ('stage1_drums_bass.xm', 'stage1 drums+bass', BASS_MAJOR),
        ('boss1_drums_bass.xm', 'boss1 drums+bass', BASS_MINOR),
    ]:
        xm = build_xm(module_name, 4,
                      [build_pattern(64, 4, backing_events(bass))],
                      backing_instruments)
        write(filename, xm)

    # Themes: backing + lead, two 4-bar patterns (8-bar tune), 6 channels
    # (XM channel counts must be even; channel 6 stays empty).
    LEAD_CH, LEAD_INST = 4, 5
    for filename, module_name, bass, tune in [
        ('stage1_theme_a.xm', 'stage1 theme A anthem', BASS_MAJOR, TUNE_A),
        ('stage1_theme_b.xm', 'stage1 theme B driver', BASS_MAJOR, TUNE_B),
        ('boss1_theme_a.xm', 'boss1 theme A siren', BASS_MINOR, TUNE_BOSS_A),
        ('boss1_theme_b.xm', 'boss1 theme B hammer', BASS_MINOR, TUNE_BOSS_B),
    ]:
        patterns = []
        for first_bar in (0, 4):
            events = dict(backing_events(bass))
            events.update(lead_events(tune, first_bar, LEAD_CH, LEAD_INST))
            patterns.append(build_pattern(64, 6, events))
        xm = build_xm(module_name, 6, patterns,
                      backing_instruments + [lead_instrument])
        write(filename, xm)


if __name__ == '__main__':
    import sys
    main(sys.argv[1] if len(sys.argv) > 1 else None)
