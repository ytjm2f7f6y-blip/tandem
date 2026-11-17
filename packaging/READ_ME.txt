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
two super oscillators, the dual filter, LFO 1, three envelopes, the
voice modes and the effects.

Knobs and faders: drag (Shift for fine), double-click for the default.
Levers: click the side you want. Rotary switches: click to step round
(Alt-click to step back).

The display: VOICE, MOD and FX each have pages. Turn A (or click it)
for the next page; B, C and D change the three columns. You can also
drag the columns on the display itself. Moving any control shows its
value on the display for a moment.

FILTER TYPE: the arrows step through the types for the routing
(STEREO, PARALLEL, SERIES); click the window for the list.

Point at any control and the strip under the faceplate says what it
does. The preset's name is on the tape at the bottom: click it for
the list (factory, then your own) and to save. A and B hold two
whole sounds to flip between. SET (bottom rail) sizes the window.
