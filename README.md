# Tandem

A two-layer synthesizer by Rumpus Room Records for Logic, worked out from the owner's
manual of a modern bi-timbral hybrid synth (two independent 4-voice modules, super
oscillators, a dual-core stereo filter and a binaural signal path) and built in the
family of Sam's Desk, George's Hand and Kontur: a 1960s German broadcast-desk faceplate
(TYPE DS 8), faders, levers and a green phosphor display with four encoders.

![Tandem's faceplate](docs/faceplate.jpg)

## Where the build is

| Stage | What | State |
| --- | --- | --- |
| 1 | One complete layer: oscillators, filter, LFO 1, envelopes, voice modes, the matrix, effects; the window; presets | **this build** |
| 2 | The second layer, the eight play modes (single, dual, split, one-two, rotate, random, chaos, cascade), an arpeggiator per layer, the side chain as EXT IN with the envelope follower | to do |
| 3 | Importing your own single-cycle waves, matrix polish, more presets, final-quality render | to do |

## Installing

Get `tandem-<version>-mac` from the newest green run on the Actions page, open the disk
image, quit Logic, and in Terminal run:

    bash "$(ls -td /Volumes/tandem*/ | head -1)install.sh"

Then, on a Software Instrument track: AU Instruments > Rumpus Room Records > Tandem.

## The sound engine

Everything below is measured in `tests/engine_tests.cpp`.

**Oscillators.** Each oscillator is a centre oscillator and six sister oscillators
(the super waveform). DETUNE spreads the sisters in pitch (up to 70 cents) and, in
Binaural mode, between the ears. Phases are 32-bit accumulators, as in direct digital
synthesis, so tuning is exact (A4 measures 440.0000 Hz at 44.1, 48 and 96 kHz). Waves
are read from band-limited tables, one per octave, at twice the host rate below 70 kHz.
What would fold over lands above 26 kHz, and a 95-tap halfband filter (108 dB down)
removes it on the way back down. Measured aliasing: saw at C7 -99 dB, square at C7
-102 dB, saw at E7 -98 dB.

Hard sync (2-1 or 1-2) runs the synced saw, square or pulse as its exact shape and
smooths every jump with a 16-tap band-limited step at its exact time: -98 dB. Synced
sines and alternative waves smooth the reset step only: -63 to -90 dB. Cross
modulation is linear FM through zero. WAVE/PWM narrows a pulse or morphs a wave into
the next (sine to saw, saw to square, triangle to saw, an alternative wave to the
next slot). OSC 2's last three positions are drones on C2.

There are 32 alternative waves of Tandem's own (organs, vowels, synced and FM shapes,
folded and clipped sines, and more). The hardware's own waves are not in its manual,
so none of them are copied.

**Filter.** Each voice has two filter cores. Each core is four OTA one-pole stages in a
loop, solved without a unit delay, with a saturating input stage. Low, high and band
pass, all pass and phaser responses of 1 to 4 poles come from mixing the input and
the four stage taps. The routings and their types:

- STEREO (Binaural): one core per ear, 12 types including 1LP+3HP and 3LP+1HP
- PARALLEL: the two cores side by side, 4 types
- SERIES: one into the other (phaser into low pass and so on), 6 types

That makes 22 in all. Δ-CUTOFF pulls the two cores apart. Small-signal responses
match theory: 4-pole low pass -12.04 dB at cutoff, all pass flat within 0.001 dB.

Resonance type 1 feeds back through a 2-pole high pass at a quarter of the cutoff,
so the low end stays full (60 Hz within 2 dB near the top of the resonance). Type 2
loses it, like vintage 4-pole filters (-13 dB). Both self-oscillate in tune: with
KEYTRACK on FULL, within 0.3 cents from C3 to C6.

**Modulation.** LFO 1 has ten shapes, plus HF (20 Hz to 20 kHz) and HF TRK, a third
oscillator that follows the keyboard and can be added into either oscillator's
channel. It runs free, once or reset, with a delayed fade-in, and in Binaural mode a
left/right phase offset. ENV 1 has hold and can invert or loop; ENV 2 drives the VCA;
ENV 3 is on the MOD pages, and the VCA can run from ENV 2, ENV 3 or be held open.
Envelopes are analog RC curves, and a retrigger starts from where the envelope is,
so stolen voices don't click. LFO 2 rides the mod wheel and aftertouch. The matrix
has seven slots, ten sources and fifteen destinations.

**Voices.** POLY 1, POLY 2, SOLO, LEGATO and UNISON (level, detuned, octave, fifth,
major). VOICES sets how many: 4 by default, like each of the hardware's modules.
Binaural mode pairs voices into stereo super voices, so 4 voices make 2. DRIFT gives
each voice its own small pitch, cutoff and envelope offsets plus a slow wander.
Portamento is a time per octave, so small steps are quicker than big leaps.

**Effects.** On the layer: drive (harmonic, overdrive, fuzz; at the internal rate),
chorus and a 3-band equaliser. On FX SEND: a delay locked to Logic's tempo (stereo,
ping-pong, mono; with freeze) and an eight-line reverb.

Cost, one layer at 44.1 kHz with effects on: 4 voices about 8% of one core here;
16 voices with full detune about 45%.

## Layout

- `engine/`: the sound engine, header-only C++17 with no dependencies.
  - `td_dsp.hpp`: helpers, the FFT, the halfband and the band-limited step
  - `td_wave.hpp`: the wave tables and the 32 alternative waves
  - `td_voice.hpp`: envelopes, LFOs, the filter core and the voice
  - `td_layer.hpp`: voice allocation and the layer's signal chain
  - `td_fx.hpp`: the effects
  - `td_engine.hpp`: the instrument
- `plugin/`: the JUCE plugin.
  - `Params.h`: every parameter, and how they become the engine's settings
  - `Presets.h`: the factory presets
  - `PluginProcessor`
  - `PluginEditor`: the window, painted over the rendered faceplate, with the display's pages
  - `Layout.h` and `HelpText.h`: generated by `art/gen_layout_h.py`
- `tests/`:
  - `engine_tests.cpp`: the engine's measurements
  - `plugin_selftest.cpp`: drives the real processor as a host would
  - `ui_snapshot.cpp`: renders the window to PNGs
- `art/`: the faceplate pipeline (Blender via `bpy`), forked from Kontur.
  - `layout.py`: the single source of control positions and legends
  - `make_textures.py`: draws the printed legends
  - `render.py`: renders the panel and parts
  - `render_all.sh`: does the lot
