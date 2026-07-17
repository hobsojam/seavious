#!/usr/bin/env python3
"""Generate the game's music .xm files into assets/audio/:

  stage1_drums_bass.xm  - the shared drum+bass template (no lead)
  stage1_theme_a.xm     - stage 1 gameplay theme: full song form over
                          "anthem" (A) and "spray" (B)
  stage1_theme_b.xm     - title/menu theme ("driver"), tight 8-bar loop
  boss1_drums_bass.xm   - boss backing: same drums, bass in parallel minor
  boss1_theme_a.xm      - boss backing + boss tune A ("siren"; game-over lament)
  boss1_theme_b.xm      - boss backing + boss tune B ("hammer"; stage 1 boss)
  stage2_theme_a.xm     - stage 2 gameplay theme: full song form over
                          "strait" (A) and "undertow" (B), D Dorian
  boss2_theme_a.xm      - D-Dorian fortress-atoll boss theme ("gate")

152 BPM, 4/4, linear frequency table. 16 rows/bar (speed=6 ticks/row gives
exactly 4 rows/beat at this BPM). Each 64-row pattern = 4 bars:
  - Drums (channels 1-3): fixed 2-bar loop (rows 0-31), repeated once more
    (rows 32-63) to fill the pattern, with a snare fill on every 2nd bar;
    the form's break section swaps in a half-time variant (kick, beat-3
    snare, no hats).
  - Bass (channel 4): 4-bar walking bass, one chord per bar, quarter-note
    motion connecting each chord's root/3rd/5th with a passing tone into
    the next root. Stage: I-vi-IV-V in A major (A - F#m - D - E). Boss:
    the parallel-minor reharmonization i-VI-iv-v (Am - F - Dm - Em) with
    the identical rhythm and contour - only the color-note pitches move
    (C#->C, F#->F, G#->G), which is the design's whole point: the boss
    switch darkens the same music rather than replacing it.
  - Lead (channel 5) and harmony (channel 6): 8-bar tunes over two passes
    of the bass loop; the harmony channel carries a 2-row same-pitch echo
    of the lead on a softer 25% pulse in the form's A'/B' sections.

Song form: the stage gameplay themes - the tracks that loop under four
minutes of play - are compiled as   intro | A | A' | B | break | B' | A'
(44 bars). The XM restart position points at A, so the drums+bass intro
plays exactly once and the ~69s body loops with its arrangement rising
and falling (echo joins, breaks strip back down). Boss and menu tracks
deliberately stay 8-bar loops: repetition reads as intensity in a fight
and nobody lingers on the title screen.

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
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib

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


def build_xm(module_name, n_channels, patterns, instruments, order, restart):
    """order: pattern indices played in sequence; restart: order index the
    player loops back to at the song end (jar_xm honors it natively, so an
    intro before the restart plays exactly once)."""
    assert len(patterns) <= 256 and len(order) <= 256
    assert all(0 <= o < len(patterns) for o in order)
    assert 0 <= restart < len(order)
    header_name = module_name.encode('ascii').ljust(20, b'\x00')[:20]
    tracker_name = b'seavious-gen'.ljust(20, b'\x00')[:20]
    order_table = bytes(order) + b'\x00' * (256 - len(order))

    header_after_size = struct.pack('<HHHHHHHH',
                                     len(order),     # song length
                                     restart,        # restart position
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

# D Dorian: Dm - C - G - Am. The B natural in the G chord is the modal
# lift: less settled than natural minor, fitting a dangerous archipelago
# that still has forward motion rather than Stage 1's open-water anthem.
BASS_D_DORIAN = [
    (0, ('D', 3)), (4, ('F', 3)), (8, ('A', 3)), (12, ('C', 3)),
    (16, ('C', 3)), (20, ('E', 3)), (24, ('G', 3)), (28, ('F#', 3)),
    (32, ('G', 3)), (36, ('B', 3)), (40, ('D', 4)), (44, ('G#', 3)),
    (48, ('A', 3)), (52, ('C', 4)), (56, ('E', 4)), (60, ('C#', 4)),
]


def backing_events(bass_notes, drums='normal'):
    """Drum + bass events for one 64-row (4-bar) pattern.

    drums: 'normal' is the driving loop (fill every 2nd bar); 'break' is
    the half-time breakdown - kick on the downbeat, snare on beat 3, no
    hats - used by the form's break section so the bass carries alone.
    """
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

    if drums == 'break':
        for bar_row in (0, 16, 32, 48):
            events[(bar_row + 0, KICK)] = (49, 1)
            events[(bar_row + 8, SNARE)] = (49, 2)
    else:
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

# D-Dorian stage: a salt-air climbing line. The B natural is held over the
# G bar each pass so the mode reads clearly without turning the whole tune
# into a scale exercise.
TUNE_STAGE2_A = [
    (0, 0, 'D', 5), (0, 4, 'F', 5), (0, 8, 'A', 5), (0, 12, 'C', 5),
    (1, 0, 'E', 5), (1, 6, 'G', 5), (1, 12, 'A', 5),
    (2, 0, 'B', 5), (2, 8, 'A', 5), (2, 12, 'G', 5),
    (3, 0, 'E', 5), (3, 8, 'C', 5), (3, 12, 'D', 5),
    (4, 0, 'F', 5), (4, 4, 'A', 5), (4, 8, 'C', 6), (4, 12, 'A', 5),
    (5, 0, 'G', 5), (5, 8, 'E', 5), (5, 12, 'D', 5),
    (6, 0, 'B', 5), (6, 4, 'D', 6), (6, 8, 'A', 5), (6, 12, 'G', 5),
    (7, 0, 'E', 5), (7, 6, 'F', 5), (7, 10, 'C', 5), (7, 14, 'C#', 5),
]

# Stage 1 B section ("spray"): call-and-response over the same changes -
# a rising three-note call answered lower, offbeat pushes, a breath (OFF)
# closing each half. Mid-register so the anthem keeps the top. Ends
# stable on E (no leading tone): the break follows, and the bass resolves.
TUNE_STAGE1_B = [
    (0, 0, 'C#', 5), (0, 2, 'E', 5), (0, 4, 'A', 5), (0, 10, 'G#', 5), (0, 12, 'E', 5),
    (1, 0, 'F#', 5), (1, 4, 'C#', 5), (1, 8, 'A', 4), (1, 12, 'B', 4), (1, 14, 'C#', 5),
    (2, 0, 'D', 5), (2, 2, 'F#', 5), (2, 4, 'A', 5), (2, 8, 'F#', 5), (2, 12, 'D', 5),
    (3, 0, 'B', 4), (3, 4, 'G#', 4), (3, 8, 'B', 4), (3, 10, 'C#', 5), (3, 12, 'D', 5),
    (3, 14, 'OFF', 0),
    (4, 0, 'C#', 5), (4, 2, 'E', 5), (4, 4, 'A', 5), (4, 10, 'B', 5), (4, 12, 'A', 5),
    (5, 0, 'F#', 5), (5, 4, 'E', 5), (5, 8, 'C#', 5), (5, 12, 'A', 4),
    (6, 0, 'F#', 5), (6, 4, 'D', 5), (6, 8, 'A', 4), (6, 10, 'B', 4), (6, 12, 'C#', 5),
    (6, 14, 'D', 5),
    (7, 0, 'E', 5), (7, 4, 'D', 5), (7, 8, 'B', 4), (7, 12, 'E', 5), (7, 14, 'OFF', 0),
]

# Stage 2 B section ("undertow"): a low syncopated riff (dotted-8th feel:
# rows 0-3-6-8) under the strait's high register, surfacing to the D-
# Dorian color notes (B natural in bars 3/6) at each bar's tail. The same
# chromatic C# pull as the stage's other tunes closes it into D.
TUNE_STAGE2_B = [
    (0, 0, 'A', 4), (0, 3, 'F', 4), (0, 6, 'D', 4), (0, 8, 'F', 4), (0, 12, 'A', 4),
    (0, 14, 'C', 5),
    (1, 0, 'G', 4), (1, 3, 'E', 4), (1, 6, 'C', 4), (1, 8, 'E', 4), (1, 12, 'G', 4),
    (1, 14, 'A', 4),
    (2, 0, 'B', 4), (2, 3, 'G', 4), (2, 6, 'D', 4), (2, 8, 'G', 4), (2, 12, 'B', 4),
    (2, 14, 'D', 5),
    (3, 0, 'C', 5), (3, 4, 'A', 4), (3, 8, 'E', 4), (3, 12, 'A', 4),
    (4, 0, 'A', 4), (4, 3, 'F', 4), (4, 6, 'D', 4), (4, 8, 'F', 4), (4, 12, 'A', 4),
    (4, 14, 'C', 5),
    (5, 0, 'G', 4), (5, 3, 'E', 4), (5, 6, 'C', 4), (5, 8, 'E', 4), (5, 12, 'G', 4),
    (5, 14, 'B', 4),
    (6, 0, 'D', 5), (6, 3, 'B', 4), (6, 6, 'G', 4), (6, 8, 'B', 4), (6, 12, 'D', 5),
    (6, 14, 'E', 5),
    (7, 0, 'C', 5), (7, 4, 'E', 5), (7, 8, 'D', 5), (7, 12, 'C#', 5), (7, 14, 'OFF', 0),
]

# Fortress atoll: the same modal colors, but a lower two-note ostinato
# answers itself against the bass. The final C# is a brief gate-opening
# pull into the loop's D, not a change of key.
TUNE_BOSS2_A = [
    (0, 0, 'D', 4), (0, 4, 'A', 4), (0, 8, 'F', 4), (0, 12, 'A', 4),
    (1, 0, 'C', 4), (1, 4, 'G', 4), (1, 8, 'E', 4), (1, 12, 'G', 4),
    (2, 0, 'G', 4), (2, 4, 'D', 5), (2, 8, 'B', 4), (2, 12, 'D', 5),
    (3, 0, 'A', 4), (3, 4, 'E', 5), (3, 8, 'C', 5), (3, 12, 'E', 5),
    (4, 0, 'D', 5), (4, 4, 'A', 4), (4, 8, 'F', 5), (4, 12, 'A', 4),
    (5, 0, 'C', 5), (5, 4, 'G', 4), (5, 8, 'E', 5), (5, 12, 'G', 4),
    (6, 0, 'B', 4), (6, 4, 'D', 5), (6, 8, 'G', 5), (6, 12, 'D', 5),
    (7, 0, 'A', 4), (7, 4, 'C', 5), (7, 8, 'E', 5), (7, 12, 'C#', 5),
]


LEAD_CH, HARMONY_CH = 4, 5
LEAD_INST, HARMONY_INST = 5, 6
# The harmony voice is a 2-row (8th-note) same-pitch echo of the lead on
# the softer pulse - the classic single-channel tracker delay illusion,
# which thickens A'/B' sections without a second composed line.
ECHO_DELAY_ROWS = 2


def tune_events(tune, channel, inst, n_rows, delay=0):
    """Map a tune's (bar, row_in_bar, name, octave) events onto a channel;
    delayed copies that would spill past the section are dropped."""
    events = {}
    for bar, row_in_bar, name, octave in tune:
        row = bar * 16 + row_in_bar + delay
        if row >= n_rows:
            continue
        if name == 'OFF':
            events[(row, channel)] = (KEY_OFF, None)
        else:
            events[(row, channel)] = (note_num(name, octave), inst)
    return events


# ---------------------------------------------------------------- sections --
# A section is a contiguous run of bars with one arrangement; a song is a
# list of sections compiled to patterns + an order table. Repeated sections
# dedup to the same pattern indices, so recapitulations cost order bytes
# only. One section index is the loop restart: everything before it is an
# intro that plays exactly once (jar_xm honors the XM restart position).

def section_events(bass, n_bars, tune=None, harmony=False, drums='normal'):
    """One section: backing repeated per 4-bar block, plus the optional
    lead tune and its echo. Returns (events, n_bars); n_bars % 4 == 0."""
    assert n_bars % 4 == 0
    n_rows = n_bars * 16
    events = {}
    for block in range(n_bars // 4):
        for (row, ch), ev in backing_events(bass, drums).items():
            events[(block * 64 + row, ch)] = ev
    if tune is not None:
        events.update(tune_events(tune, LEAD_CH, LEAD_INST, n_rows))
        if harmony:
            events.update(tune_events(tune, HARMONY_CH, HARMONY_INST, n_rows,
                                      delay=ECHO_DELAY_ROWS))
    return events, n_bars


def stage_form(bass, tune_a, tune_b):
    """The shared stage-theme song form (see module docstring):

        intro | A | A' | B | break | B' | A'   (44 bars, ~69s loop body)

    A' recurs verbatim at the end, so its patterns dedup to order-table
    references. Pair with restart_section=1: the intro plays once, the
    loop returns to A with the arrangement thinned back down.
    """
    intro = section_events(bass, 4)
    a = section_events(bass, 8, tune_a)
    a2 = section_events(bass, 8, tune_a, harmony=True)
    b = section_events(bass, 8, tune_b)
    brk = section_events(bass, 4, drums='break')
    b2 = section_events(bass, 8, tune_b, harmony=True)
    return [intro, a, a2, b, brk, b2, a2]


def build_song(module_name, n_channels, sections, instruments, restart_section=0):
    """Compile sections to deduped patterns + an order table and build the
    module. restart_section: index in `sections` the loop returns to."""
    patterns = []
    index_of = {}
    order = []
    section_starts = []
    for events, n_bars in sections:
        section_starts.append(len(order))
        for block in range(n_bars // 4):
            block_events = {(row - block * 64, ch): ev
                            for (row, ch), ev in events.items()
                            if block * 64 <= row < (block + 1) * 64}
            pat = build_pattern(64, n_channels, block_events)
            if pat not in index_of:
                index_of[pat] = len(patterns)
                patterns.append(pat)
            order.append(index_of[pat])
    return build_xm(module_name, n_channels, patterns, instruments,
                    order, section_starts[restart_section])


# ------------------------------------------------------------------- main --

def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'audio')
    os.makedirs(out_dir, exist_ok=True)

    kick = delta_encode(gen_kick())
    snare = delta_encode(gen_snare())
    hihat = delta_encode(gen_hihat())
    # Bass: was a 25% pulse at volume 48, but its thin fundamental vanished
    # under the 50% square lead in playtests ("can't hear the bass at all")
    # - a wider 40% pulse carries far more fundamental while staying
    # distinct from the lead's timbre, and it runs at full volume.
    bass = delta_encode(gen_pulse_cycle(0.40))
    lead = delta_encode(gen_pulse_cycle(0.50))

    backing_instruments = [
        instrument_block('kick', sample_header(len(kick), 0, 0, 56, 0x00, 'kick'), kick),
        instrument_block('snare', sample_header(len(snare), 0, 0, 52, 0x00, 'snare'), snare),
        instrument_block('hihat', sample_header(len(hihat), 0, 0, 32, 0x00, 'hihat'), hihat),
        instrument_block('bass_pulse40',
                         sample_header(len(bass), 0, len(bass), 64, 0x01, 'bass_pulse40'), bass),
    ]
    # Lead level: started at 44, dropped to 34 after the first Windows
    # playtest found the tune overpowering the drums+bass backing, then to
    # 22 after the second still heard the tune dominating - a 50% square is
    # perceptually much louder than its sample volume suggests.
    lead_instrument = instrument_block(
        'lead_square50',
        sample_header(len(lead), 0, len(lead), 22, 0x01, 'lead_square50'), lead)
    # Harmony echo voice: a thin 25% pulse well under the lead (12 vs 22),
    # so the delayed copy reads as space behind the tune, not a second
    # tune fighting it.
    harmony = delta_encode(gen_pulse_cycle(0.25))
    harmony_instrument = instrument_block(
        'harmony_pulse25',
        sample_header(len(harmony), 0, len(harmony), 12, 0x01, 'harmony_pulse25'), harmony)

    def write(filename, xm):
        path = os.path.join(out_dir, filename)
        with open(path, 'wb') as f:
            f.write(xm)
        print('wrote', path, len(xm), 'bytes')

    # Templates: backing only, one 4-bar section, 4 channels.
    for filename, module_name, bass in [
        ('stage1_drums_bass.xm', 'stage1 drums+bass', BASS_MAJOR),
        ('boss1_drums_bass.xm', 'boss1 drums+bass', BASS_MINOR),
    ]:
        xm = build_song(module_name, 4, [section_events(bass, 4)],
                        backing_instruments)
        write(filename, xm)

    # Single-loop themes: backing + one 8-bar lead section, 6 channels
    # (XM channel counts must be even; channel 6 is the harmony voice,
    # silent in these tracks). Boss themes stay deliberately short -
    # repetition reads as intensity in a fight - and "driver" loops
    # tight under the title screen, where nobody lingers.
    for filename, module_name, bass, tune in [
        ('stage1_theme_b.xm', 'stage1 theme B driver', BASS_MAJOR, TUNE_B),
        ('boss1_theme_a.xm', 'boss1 theme A siren', BASS_MINOR, TUNE_BOSS_A),
        ('boss1_theme_b.xm', 'boss1 theme B hammer', BASS_MINOR, TUNE_BOSS_B),
        ('boss2_theme_a.xm', 'boss2 theme A gate', BASS_D_DORIAN, TUNE_BOSS2_A),
    ]:
        xm = build_song(module_name, 6, [section_events(bass, 8, tune)],
                        backing_instruments + [lead_instrument])
        write(filename, xm)

    # Stage gameplay themes: the full song form (see module docstring) -
    # these are the tracks that loop under 4 minutes of play.
    for filename, module_name, bass, tune_a, tune_b in [
        ('stage1_theme_a.xm', 'stage1 anthem+spray', BASS_MAJOR, TUNE_A, TUNE_STAGE1_B),
        ('stage2_theme_a.xm', 'stage2 strait+undertow', BASS_D_DORIAN, TUNE_STAGE2_A,
         TUNE_STAGE2_B),
    ]:
        xm = build_song(module_name, 6, stage_form(bass, tune_a, tune_b),
                        backing_instruments + [lead_instrument, harmony_instrument],
                        restart_section=1)
        write(filename, xm)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
