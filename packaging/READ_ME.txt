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

The two windows at the top name each layer's sound: click a name for the
list and put any patch on either layer; the arrows step through them.

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

The side chain input is switched off in this build, so layer 1's EXT IN
knob does nothing yet. Layer 2's EXT IN knob sets CASCADE's level.

Knobs and faders: drag (Shift for fine), scroll, double-click for the
default; a tag beside them shows the value as you turn. UNDO and REDO
keys (or Cmd-Z) take edits back and forth. Keys with lamps:
click to step through, or click a lamp to go straight to it. Lamp rings
(waveforms, octave): click a lamp, or drag. Right-click any control for
its menu. Tab and the arrow
keys work the panel from the keyboard; VoiceOver reads every control.

The display: PERF, VOICE, MOD, ARP and FX each have pages (press again,
or use the PAGE arrows). Drag a column up or down (or scroll over it)
to change it. Moving any control shows its value for a moment.

SHIFT: click it, then touch any control, for that control's page (SHIFT
+ CUTOFF: routing, type, resonance type, keytrack). Red legends are its
own jobs: SWAP and COPY on the layer keys, ZONES, DIVISION, INIT on
COMPARE (a blank patch), NAME on WRITE.

The mod matrix: twelve slots, one per MOD page (SOURCE, DEST, AMOUNT,
and VIA, a second source that scales the amount). SHIFT turns AMOUNT
into the slot's curve. ASSIGN: click it, then turn the knob you want to
modulate; a free slot opens for it. Modulated knobs carry an amber dot;
touch one (or light ASSIGN) to see an arc for its range.

VOICE MODE: a key per layer steps the mode; the lamps show it. The voice
lamps in MIX light as each voice plays.

DICE and VARY: DICE rolls a pattern of small note-to-note differences;
VARY sets how much you hear (none at 0, the odd octave past 7).

Whole sounds: click the name on the right of the display's top line.
WRITE saves one layer as a patch, or both as a performance (its lamp
blinks when something has changed). COMPARE flips between the sound
as edited and as chosen.

SETUP slides the drawer up: master tune, transpose, velocity and pressure
response, MIDI channel per layer, sustain pedal polarity, AGE and size.

Hover over any control for a note on what it does.
