Tandem: install
===============

0. Quit Logic. If an older "tandem" disk is still open, eject it
   (Finder sidebar, eject button) so only this one is open.
1. Open Terminal (press Cmd+Space, type Terminal, press Return).
2. Paste this line and press Return:

   bash "$(ls -td /Volumes/tandem*/ | head -1)install.sh"

3. Open Logic. Make a Software Instrument track, then in its
   instrument slot: AU Instruments > Rumpus Room Records > Tandem


Using it
--------
Tandem is a two-layer synthesizer: two complete synths, each with two
super oscillators, the dual filter, two LFOs, three envelopes, voice
modes, an arpeggiator and its own effects.

Top row: the sound of the layer you are editing. Bottom row: playing
and programming.

LAYER 1 / LAYER 2: which layer the panel edits (the display's title says
L1 or L2). SHIFT + LAYER 1 swaps them; SHIFT + LAYER 2 copies the one you
are editing onto the other.

PLAY MODE: how the layers share the keys. SINGLE (the layer you edit),
DUAL, SPLIT, ONE-TWO (first key layer 1, the rest layer 2), ROTATE,
RANDOM, CHAOS (random, with fifths and octaves) and CASCADE (layer 1
plays into layer 2's filter, at layer 2's EXT IN knob). SHIFT + PLAY
MODE: ZONES; the next key you play sets the split.

ARPEGGIATOR: ON, HOLD, MODE, RANGE for the layer you edit (each layer
has its own); division, swing and gate on the ARP pages. HOST follows
Logic's tempo and beat; turn TEMPO or TAP for Tandem's own clock.

Side chain: pick a track in the Side Chain menu of Tandem's header.
Layer 1's EXT IN knob feeds it into the filter; its envelope follower is
a matrix source, and with the VCA on GATE it opens the VCA.

Knobs and faders: drag (Shift for fine), scroll, double-click for the
default; a tag beside them shows the value as you turn. Keys with lamps:
click to step through, or click a lamp to go straight to it. Lamp rings
(waveforms, octave): click a lamp, or drag. Right-click any control for
its menu. Undo: SHIFT + the page arrows, or Cmd-Z. Tab and the arrow
keys work the panel from the keyboard; VoiceOver reads every control.

The display: PERF, VOICE, MOD, ARP and FX each have pages (press again,
or use the PAGE arrows). The four encoders under the display change the
four columns above them; you can also drag a column. Moving any control
shows its value for a moment.

SHIFT: click it, then touch any control, for that control's page (SHIFT
+ CUTOFF: routing, type, resonance type, keytrack). Red legends are its
own jobs: SHIFT + a patch key picks the bank, SHIFT + COMPARE starts from
a blank patch, SHIFT + the page arrows undo and redo.

The mod matrix: twelve slots, one per MOD page (SOURCE, DEST, AMOUNT,
and VIA, a second source that scales the amount). SHIFT turns AMOUNT
into the slot's curve. ASSIGN: click it, then turn the knob you want to
modulate; a free slot opens for it. Modulated knobs carry an amber dot;
touch one (or light ASSIGN) to see an arc for its range.

VOICE MODE: a key per layer steps the mode; the lamps show it. The voice
lamps in LAYER MIX light as each voice plays.

DICE and VARY: DICE rolls a pattern of small note-to-note differences;
VARY sets how much you hear (none at 0, the odd octave past 7).

Patches: eight keys, eight banks (G and H use both layers; SHIFT + a
bank key again reaches your own patches). Right-click a patch key (or click the
top line of the display) for the list. WRITE saves the sound as one of
yours (its lamp blinks when the patch has changed). COMPARE flips
between the patch as edited and as stored.

SETUP opens the drawer: master tune, transpose, velocity and pressure
response, MIDI channel per layer, sustain pedal polarity, AGE and size.

Hover over any control for a note on what it does.
