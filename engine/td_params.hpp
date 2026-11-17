// Tandem: everything one layer (one synthesizer module) can be set to, in plain units.
// The plugin fills this from its parameters; the engine reads it once per block.
#pragma once

namespace td {

// Oscillator wave positions on the rotary switches.
enum OscWave { kWSine = 0, kWSaw, kWSquare, kWPulse, kWTri, kWNoise, kWAlt1 };   // OSC 1: kWAlt1 + 0..5
constexpr int kOsc1Waves = 12;                                                     // OSC 2: kWAlt1 + 0..2, then the drones
enum { kWDroneSquare = 9, kWDroneSaw = 10, kWDroneSine = 11 };
constexpr int kOsc2Waves = 12;

// LFO 1 waves: ten low-frequency shapes, then the two high-frequency modes.
enum LfoWave { kLSine = 0, kLSaw, kLSampleHold, kLSquare, kLRamp, kLNoise, kLTri, kLNoisySine, kLUser1, kLUser2, kLHF, kLHFTrack };
constexpr int kLfo1Waves = 12;
// LFO 2 waves.
enum Lfo2Wave { kL2Tri = 0, kL2Saw, kL2SampleHold, kL2Square, kL2Ramp, kL2Noise };

// Filter routings and the types each offers (the hardware's curated sets).
enum Route { kRouteStereo = 0, kRouteParallel, kRouteSeries };
enum StereoType { kS4LP = 0, kS2LP, kS1LP, kS3HP, kS2HP, kS1HP, kS4BP, kS2BP, kS4AP, kS4PS, kS1LP3HP, kS3LP1HP, kNumStereoTypes };
enum ParallelType { kP2LP2LP = 0, kP1LP1LP, kP2BP2BP, kP4BP4BP, kNumParallelTypes };
enum SeriesType { kQ4PS1LP = 0, kQ4PS2LP, kQ4PS1HP, kQ4PS4PS, kQ1LP1LP, kQ1LP1HP, kNumSeriesTypes };

enum VoiceMode { kPoly1 = 0, kSolo, kLegato, kUnison, kPoly2 };
enum UnisonMode { kUniLevel1 = 0, kUniLevel2, kUniOctave, kUniFifth, kUniMajor };

// Modulation matrix.
enum ModSource { kMsNone = 0, kMsOsc2, kMsLfo1, kMsLfo2, kMsEnv1, kMsEnv2, kMsVelocity, kMsAftertouch, kMsExpression, kMsEnvFollower,
                 kMsNote, kNumModSources };
enum ModDest { kMdNone = 0, kMdLfo1Rate, kMdCutoff, kMdDeltaCutoff, kMdEnv1Cutoff, kMdEnv1Depth, kMdLfo1Depth, kMdOsc2Osc1, kMdOsc1Wave,
               kMdOsc1Mix, kMdOsc2Mix, kMdResonance, kMdEnv1Decay, kMdDelayTime, kMdVcaLevel, kMdEnv1Attack, kNumModDests };
constexpr int kMatrixSlots = 7;

enum DriveMode { kDriveOff = 0, kDriveHarmonic, kDriveOverdrive, kDriveFuzz };
enum DelayMode { kDelayStereo = 0, kDelayPingPong, kDelayMono };
constexpr int kNumDelayDivs = 16;
// The delay's clock divisions, in beats (quarter notes): 1/32 .. whole.
constexpr float kDelayBeats[kNumDelayDivs] = {0.125f, 0.1875f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f,
                                              2.0f / 3.0f, 1.0f, 1.5f, 4.0f / 3.0f, 2.0f, 3.0f, 8.0f / 3.0f, 4.0f};
constexpr int kNumLfoDivs = 18;
// LFO 1 clock sync: longest first, so the RATE fader still runs slow to fast.
constexpr float kLfoBeats[kNumLfoDivs] = {16.0f, 8.0f, 6.0f, 4.0f, 3.0f, 8.0f / 3.0f, 2.0f, 1.5f, 4.0f / 3.0f,
                                          1.0f, 0.75f, 2.0f / 3.0f, 0.5f, 0.375f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f};

struct LayerParams {
    // ---- oscillators
    int osc1Wave = kWSaw, osc2Wave = kWSaw;
    float osc1Tune = 0, osc1Fine = 0, osc2Tune = 0, osc2Fine = 0;   // semitones (-7..7), cents (-50..50)
    int sync = 0;                                                    // 0 off, 1 OSC 2 to OSC 1 (2-1), 2 OSC 1 to OSC 2 (1-2)
    int modOsc = 1;                                                  // the MOD section works on: 0 OSC 1, 1 both, 2 OSC 2
    float detune = 0;                                                // 0..1
    int waveSrc = 0;  float waveMod = 0;                             // 0 manual, 1 ENV 1, 2 LFO 1; 0..1
    int pitchSrc = 0; float pitchMod = 0;                            // 0 ENV 1, 1 LFO 1, 2 cross; 0..1
    int osc1Alt[6] = {0, 1, 2, 3, 4, 5}, osc2Alt[3] = {6, 7, 8}, lfoAlt[2] = {20, 15};
    // ---- mixer into the filter
    float osc1Level = 0.55f, osc2Level = 0.0f, extLevel = 0.0f;      // 0..1 (above about 0.6 they drive the filter)
    // ---- filter
    float cutoff = 1.0f, resonance = 0.0f;                           // 0..1
    int keytrack = 0;                                                // 0 off, 1 half, 2 on
    int vcfSrc = 0; float vcfMod = 0.0f;                             // 0 ENV 1, 1 LFO 1, 2 OSC 2; 0..1
    float deltaCutoff = 0.0f;                                        // -1..1
    int route = kRouteStereo;
    int stereoType = kS4LP, parallelType = kP2LP2LP, seriesType = kQ4PS1LP;
    int resType = 0;                                                 // 0: type 1 (low end kept), 1: type 2 (vintage)
    // ---- LFO 1
    int lfo1Wave = kLTri, lfo1Mode = 0;                              // mode: free / once / reset (in HF: free / to OSC 1 / to OSC 2)
    float lfo1Rate = 0.5f, lfo1Delay = 0.0f, lfo1Phase = 0.0f;       // 0..1
    bool lfo1Sync = false;
    // ---- envelopes (faders 0..1)
    float e1Hold = 0, e1A = 0, e1D = 0.5f, e1S = 0.0f, e1R = 0.3f;  int e1Mode = 0;   // normal, inverted, loop
    float e2A = 0, e2D = 0.4f, e2S = 1.0f, e2R = 0.25f;
    int vcaSrc = 0;                                                  // ENV 2, ENV 3, on
    float e3Hold = 0, e3A = 0.2f, e3D = 0.5f, e3S = 0.5f, e3R = 0.4f; int e3Mode = 0;
    float velVca = 0.25f, velVcf = 0.0f;                             // -1..1
    // ---- voices
    int voiceMode = kPoly1, unisonMode = kUniLevel2, voices = 4;     // voices: 2..16 (the hardware: 4)
    float drift = 0.15f, panSpread = 0.0f, portamento = 0.0f;        // 0..1
    int octave = 0;                                                  // -2..2
    // ---- performance
    float bendRange = 2.0f, bendVcf = 0.0f;                          // semitones 0..12; -1..1
    int lfo2Wave = kL2Tri, lfo2Trig = 0;                             // MW, AT & MW, on
    float lfo2Rate = 0.45f, lfo2Delay = 0.0f, lfo2RateMod = 0.0f, lfo2Pitch = 0.15f, lfo2Vcf = 0.0f, lfo2Vca = 0.0f;
    // ---- modulation matrix
    int mSrc[kMatrixSlots] = {}, mDst[kMatrixSlots] = {};
    float mAmt[kMatrixSlots] = {};
    // ---- insert effects
    int drvMode = kDriveOff; float drvTone = 0.5f, drvAmt = 0.3f, drvMix = 1.0f;
    bool chOn = false; float chRate = 0.3f, chDepth = 0.4f, chDelay = 0.3f, chMix = 0.5f;
    float eqLow = 0.0f, eqMid = 0.0f, eqHigh = 0.0f, eqQ = 0.7f;     // dB, dB, dB, Q
    // ---- send effects
    int dlyDiv = 7, dlyMode = kDelayStereo; float dlyFb = 0.35f, dlyDamp = 0.3f;
    float fxBal = 0.5f;                                              // 0: delay only .. 1: reverb only
    bool freeze = false;
    float rvSize = 0.5f, rvDamp = 0.4f, rvLowCut = 0.2f, rvPreDly = 0.1f, rvWidth = 0.8f;
    // ---- mixer
    float level = 0.75f, pan = 0.0f, fxSend = 0.0f;                  // pan -1..1
};

}  // namespace td
