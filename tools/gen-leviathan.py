#!/usr/bin/env python3
"""Generates the Leviathan boss sprite set into assets/sprites/:

  leviathan_hull.png          200x120 breached broadside hull, bow left -
                              the "wreck-to-be" base every part sits on:
                              amber waterline wash, dark plate deck with
                              seams and a raised spine ridge, and a jagged
                              breach amidships the core is revealed under
  leviathan_pod.png           20x20 AA pod: air-family violet dome with a
                              magenta energy ring and pale core (gun-weak)
  leviathan_hull_section.png  40x24 armored waterline casemate plate with
                              two dark gun ports facing the player
                              (torpedo-weak)
  leviathan_mortar.png        24x24 bare-steel mortar dome - deliberately
                              carries NO faction color: bare steel is the
                              visual grammar for "no weapon works on this"
  leviathan_core.png          32x24 white-hot exposed core, jagged breach
                              opening glowing from white-hot center to
                              orange rim (weak to both weapons)

See README "Stage 1 boss design" for the fight these serve. Mortar
shell / shadow / blast are code-drawn VFX per the game's convention,
not sprites. The grids were drafted offline with a small rasterizer and
curated by eye; this script holds the baked result so the assets are
exactly reproducible (same convention as the ocean tile).

Run from anywhere:  python3 tools/gen-leviathan.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import genlib
import gridpng

HULL_PALETTE = {
    '.': (0, 0, 0, 0), # transparent
    'o': (18, 14, 10, 255), # outline dark warm
    'a': (232, 148, 44, 255), # amber waterline #e8942c
    'h': (46, 38, 30, 255), # hull dark warm
    's': (34, 28, 22, 255), # plate shade / seams
    'l': (110, 86, 56, 255), # lit plating (upper-left)
    'r': (96, 78, 50, 255), # raised spine ridge
    'k': (8, 7, 6, 255), # breach void
}

HULL_GRID = [
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '..........................................aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.........................',
    '.........................................aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.....................',
    '........................................aaoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooaaaaaa...................',
    '.......................................aaooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooaaa..................',
    '......................................aaoollllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllsllllllllllllllllllooooooaa.................',
    '.....................................aaoolllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllloooaa................',
    '....................................aaoollllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllsllllllllllllllllllllllllooaa...............',
    '...................................aaoolllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllooaa..............',
    '..................................aaoollllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllsooaa.............',
    '.................................aaoosssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssooa.............',
    '................................aaoollllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllsllooa............',
    '...............................aaoolllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllsllloaa...........',
    '..............................aaoollllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllloa...........',
    '.............................aaoolllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslhhhooa..........',
    '............................aaoollllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllsllllllllllhhhhhhhhhhhhhhhshhhhhoa..........',
    '..........................aaaoolllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhho..........',
    '.........................aaaoollllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllslllhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhoa.........',
    '........................aaooolllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllllllsllllllllllllhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhha.........',
    '.......................aaooollllllllllllllllllllllllslllllllllllllllllllllllllslllllllllllllllllllllhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhho.........',
    '......................aaooslllllllllllllllllllllllllslllllllllllllllllllllllllslllllhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhoa........',
    '.....................aaoolslllllllllllllllllllllllllsllllllllllllllhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhha........',
    '....................aaoollslllllllllllllllllllllllhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhho........',
    '...................aaoolllslllllllhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhho........',
    '..................aaoossssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssa.......',
    '.................aaoohhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhooooohhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhha.......',
    '................aaoohhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhoookkooohhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhho.......',
    '...............aaoohhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhooooooooooookkkkkkoohhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhho.......',
    '..............aaoohhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhoookkkkkooookkkkkkkoohhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhh.......',
    '.............aaoohhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhoookkkkkkkkkkkkkkkkkkoohhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhha......',
    '............aaoohhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhoookkkkkkkkkkkkkkkkkkkoohhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhha......',
    '...........aaoohhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhookkkkkkkkkkkkkkkkkkkkoooohhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhho......',
    '..........aaoohhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhoookkkkkkkkkkkkkkkkkkkkkkooooohhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhho......',
    '.........aaoohhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhoookkkkkkkkkkkkkkkkkkkkkkkkkkooooohhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhh......',
    '........aaoohhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhoookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkoooohshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhh......',
    '......aaaoorrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrroookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkooosrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrr......',
    '......aaaoorrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrroookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkooorrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrr......',
    '......aaaoorrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrroookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkooorrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrr......',
    '......aaaoorrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrroookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkoorrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrr......',
    '......aaaoorrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkooorrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrrrrrrrrrrrrrrrrsrrrrrrrrrrr......',
    '........aaoohhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhoookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkooohhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhh......',
    '.........aaoohhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhoookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkooohhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhh......',
    '..........aaoohhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkooohhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhho......',
    '...........aaoohhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhoookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkooooshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhho......',
    '............aaoohhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhoookkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkooooohhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhha......',
    '.............aaoohhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhoookkkkkkkkkkkooookkkkkkkkoooooooooohhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhha......',
    '..............aaoohhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhoookkkkkkkooooshookkkkkkoooohhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhh.......',
    '...............aaoohhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhoooooooooohhhshhoookkooohhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhho.......',
    '................aaoohhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhooooohhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhho.......',
    '.................aaoohhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhha.......',
    '..................aaoossssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssa.......',
    '...................aaoohhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhho........',
    '....................aaoohhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshssssssso........',
    '.....................aaoohshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhssssssssssssssssssssssssa........',
    '......................aaooshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhsssssssssssssssssssssssssssssssssssssssoa........',
    '.......................aaooohhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhsssssssssssssssssssssssssssssssssssssssssssssssssssssssso.........',
    '........................aaooohhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhsssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssa.........',
    '.........................aaaoohhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssoa.........',
    '..........................aaaoohhhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhhhhhhhhhhhhshhhhhssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssso..........',
    '............................aaoohhhhhhhhhhhhhhhhhhhhshhhhhhhhhhhhhhsssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssoa..........',
    '.............................aaoohhhhhhhhhhhhhhhhhhssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssooa..........',
    '..............................aaoosssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssoa...........',
    '...............................aaoosssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssoaa...........',
    '................................aaoosssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssooa............',
    '.................................aaoosssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssooa.............',
    '..................................aaoosssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssooaa.............',
    '...................................aaoosssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssooaa..............',
    '....................................aaoosssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssooaa...............',
    '.....................................aaoossssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssoooaa................',
    '......................................aaoosssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssooooooaa.................',
    '.......................................aaooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooaaa..................',
    '........................................aaoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooaaaaaa...................',
    '.........................................aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.....................',
    '..........................................aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.........................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
    '........................................................................................................................................................................................................',
]

POD_PALETTE = {
    '.': (0, 0, 0, 0), # transparent
    'O': (20, 16, 32, 255), # outline dark violet-navy
    'H': (58, 52, 72, 255), # dome gunmetal violet
    'L': (74, 66, 90, 255), # dome lit
    'M': (216, 72, 192, 255), # magenta energy #d848c0
    'W': (255, 216, 248, 255), # core near-white pink
}

POD_GRID = [
    '....................',
    '........OOOO........',
    '.....OOOLLLLOOO.....',
    '....OOLLLMMLLHOO....',
    '...OLLMMMMMMMMHHO...',
    '..OOLMMLLLLHHMMHOO..',
    '..OLMMLLLLHHHHMMHO..',
    '..OLMLLLHHHHHHHMHO..',
    '.OLLMLLHHWWHHHHMHHO.',
    '.OLMMLLHWWWWHHHMMHO.',
    '.OLMMLHHWWWWHHHMMHO.',
    '.OLLMHHHHWWHHHHMHHO.',
    '..OLMHHHHHHHHHHMHO..',
    '..OHMMHHHHHHHHMMHO..',
    '..OOHMMHHHHHHMMHOO..',
    '...OHHMMMMMMMMHHO...',
    '....OOHHHMMHHHOO....',
    '.....OOOHHHHOOO.....',
    '........OOOO........',
    '....................',
]

SECTION_PALETTE = {
    '.': (0, 0, 0, 0), # transparent
    'o': (18, 14, 10, 255), # outline dark warm
    'a': (232, 148, 44, 255), # amber glow edge #e8942c
    'h': (46, 38, 30, 255), # plate dark warm
    's': (34, 28, 22, 255), # plate shade / seam
    'l': (110, 86, 56, 255), # lit plate (upper-left)
    'k': (8, 7, 6, 255), # gun port void
}

SECTION_GRID = [
    '........................................',
    '........................................',
    '...aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa...',
    '..aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa..',
    '.aaooooooooooooooooooooooooooooooooooaa.',
    '.aaooooooooooooooooooooooooooooooooooaa.',
    '.aaoolllllllllllllhhhhhhhhhhhhhhhhhooaa.',
    '.aaookkklllllllllhhhhhhhhhhhhhhhhhhooaa.',
    '.aaookkkllllllllhhhhhhhhhhhhhhhhhhhooaa.',
    '.aaookkklllllllhhhhhhhhhhhhhhhhhhhhooaa.',
    '.aaoolllllllllhhhhhhhhhhhhhhhhhhhhhooaa.',
    '.aaoossssssssssssssssssssssssssssssooaa.',
    '.aaoossssssssssssssssssssssssssssssooaa.',
    '.aaoollllllhhhhhhhhhhhhhhhhhhhhhhhhooaa.',
    '.aaookkkllhhhhhhhhhhhhhhhhhhhhhhhhhooaa.',
    '.aaookkklhhhhhhhhhhhhhhhhhhhhhhhhhhooaa.',
    '.aaookkkhhhhhhhhhhhhhhhhhhhhhhhhhhhooaa.',
    '.aaoollhhhhhhhhhhhhhhhhhhhhhhhhhhhhooaa.',
    '.aaooooooooooooooooooooooooooooooooooaa.',
    '.aaooooooooooooooooooooooooooooooooooaa.',
    '..aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa..',
    '...aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa...',
    '........................................',
    '........................................',
]

MORTAR_PALETTE = {
    '.': (0, 0, 0, 0), # transparent
    'Q': (16, 17, 20, 255), # steel rim / tube rim
    'g': (74, 78, 86, 255), # steel dome
    'G': (108, 112, 120, 255), # lit steel (upper-left)
    'd': (40, 42, 48, 255), # tube throat
    'k': (10, 10, 12, 255), # bore void
}

MORTAR_GRID = [
    '........................',
    '........................',
    '........QQQQQQQQ........',
    '......QQGGGGGGGGQQ......',
    '.....QQGGGGGGGGggQQ.....',
    '....QGGGGGGGGGgggggQ....',
    '...QQGGGGGGGGggggggQQ...',
    '...QGGGGGGGGggggggggQ...',
    '..QGGGGGGGQQQQQggggggQ..',
    '..QGGGGGGQddddQQgggggQ..',
    '..QGGGGGQddkkddQgggggQ..',
    '..QGGGGGQdkkkkdQgggggQ..',
    '..QGGGGgQdkkkkdQgggggQ..',
    '..QGGGggQddkkddQgggggQ..',
    '..QGGgggQQddddQQgggggQ..',
    '..QGgggggQQQQQQggggggQ..',
    '...QggggggggggggggggQ...',
    '...QQggggggggggggggQQ...',
    '....QggggggggggggggQ....',
    '.....QQggggggggggQQ.....',
    '......QQggggggggQQ......',
    '........QQQQQQQQ........',
    '........................',
    '........................',
]

CORE_PALETTE = {
    '.': (0, 0, 0, 0), # transparent
    'o': (18, 14, 10, 255), # breach rim
    'a': (232, 148, 44, 255), # orange glow edge #e8942c
    'c': (255, 200, 120, 255), # hot amber
    'w': (255, 246, 216, 255), # white-hot center
}

CORE_GRID = [
    '................................',
    '...........oo...................',
    '..........oaao..................',
    '.........oaaaao......ooo........',
    '.........oaaaaao...oaaaoo.......',
    '........ooaacccaoooaaaaao.......',
    '......oooaaacccaaaaccaaaoo......',
    '...oooaaaaaaccccaaccccaaaoo.....',
    '..ooaaaaaaacccwcacccccaaaaoo....',
    '.ooaaaacccccccwwcwwcccaaaaaaoo..',
    '.ooaaaccccccccwwcwcccccccaaaaao.',
    '.ooaaaaccccwwwwwwwwwwwcccccaaaao',
    '..ooaaaacccccccwcwwwwwwcccccaaao',
    '....ooaaaaacccwwwcccwwccccccaaao',
    '.....oooaaaccwwcwccccccccccaaaao',
    '.......oaaaccwcccccaaaaaaaaaaaoo',
    '.......oaacccccccccaaaoooooooo..',
    '.......oaaccccaaaccaao..........',
    '.......oaacccaaaaaaaao..........',
    '.......oaaaaaaooaaaao...........',
    '.......ooaaaoo..ooaoo...........',
    '........oooo......oo............',
    '................................',
    '................................',
]

SPRITES = [
    (200, 120, HULL_PALETTE, HULL_GRID, 'leviathan_hull.png'),
    (20, 20, POD_PALETTE, POD_GRID, 'leviathan_pod.png'),
    (40, 24, SECTION_PALETTE, SECTION_GRID, 'leviathan_hull_section.png'),
    (24, 24, MORTAR_PALETTE, MORTAR_GRID, 'leviathan_mortar.png'),
    (32, 24, CORE_PALETTE, CORE_GRID, 'leviathan_core.png'),
]


def main(out_dir=None):
    out_dir = genlib.resolve_out_dir(out_dir, 'assets', 'sprites')
    for width, height, palette, grid, name in SPRITES:
        gridpng.write_grid_png(width, height, palette, grid,
                               os.path.join(out_dir, name))


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
