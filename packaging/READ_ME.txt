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
Tandem is a two-layer synthesizer. This build (stage 1) plays layer 1:
two super oscillators, the dual filter, two LFOs, three envelopes, the
voice modes and the effects. The layer keys, play mode, arpeggiator and
layer 2's mixer are on the panel already; they say "stage 2" for now.

Top row: the sound. Bottom row: playing and programming.

Knobs and faders: drag (Shift for fine), scroll, double-click for the
default. Keys with lamps: click to step through; the lamps show which.
Lamp rings (waveforms, octave): click or drag.

The display: PERF, VOICE, MOD, ARP and FX each have pages (press again,
or use the PAGE arrows). The four encoders under the display change the
four columns above them; you can also drag a column. Moving any control
shows its value for a moment.

SHIFT: click it, then touch any control, for that control's page (SHIFT
+ CUTOFF: routing, type, resonance type, keytrack). Right-click does the
same. Red-orange legends are their own jobs: SHIFT + a patch key picks
the bank; SHIFT + COMPARE starts from a blank patch.

The mod matrix: twelve slots, one per MOD page (SOURCE, DEST, AMOUNT,
and VIA, a second source that scales the amount). SHIFT turns AMOUNT
into the slot's curve. ASSIGN: click it, then turn the knob you want to
modulate; a free slot opens for it. Modulated knobs carry an amber dot;
touch one (or light ASSIGN) to see an arc for its range.

VOICE MODE: a key per layer steps the mode; the lamps show it. The voice
lamps in LAYER MIX light as each voice plays.

DICE and VARY: DICE rolls a pattern of small note-to-note differences;
VARY sets how much you hear (none at 0, the odd octave past 7).

Patches: eight keys, eight banks. Right-click a patch key (or click the
top line of the display) for the list. WRITE saves the sound as one of
yours (its lamp blinks when the patch has changed). COMPARE flips
between the patch as edited and as stored.

SETUP opens the drawer: master tune, transpose, velocity and pressure
response, MIDI channel, sustain pedal polarity, AGE and panel size.

Hover over any control for a note on what it does.
