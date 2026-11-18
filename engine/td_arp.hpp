// Tandem: what stands between the keys a layer is given and its voices: the arpeggiator, and HOLD.
//
// The arpeggiator steps through the notes it holds on a grid of beats the engine keeps for both
// layers (the host's, while it plays and HOST is on). With the arpeggiator off, notes pass straight
// through; HOLD then keeps them sounding after the keys come up, until the next chord.
//
// It writes to its layer through three calls: noteOn(note, vel), noteOff(note), sustain(on).
#pragma once

#include "td_dsp.hpp"
#include "td_params.hpp"

#include <array>

namespace td {

class Arp {
public:
    static constexpr int kMaxNotes = 32;
    static constexpr double kNever = 1e300;

    struct Clock { double beat = 0.0, origin = 0.0; bool host = false; };   // now, where the grid starts, the host's grid

    void reset() { count_ = 0; physical_ = 0; nOff_ = 0; running_ = false; pos_ = 0; lastJ_ = -1; pedal_ = false; }
    void setSeed(uint32_t s) { rng_ = Rng(s * 2246822519u + 7u); }

    bool on() const { return p_.arpOn; }
    bool running() const { return running_; }
    int held() const { return count_; }                 // the notes it holds (pressed or kept)
    bool sounding() const { return nOff_ > 0; }

    // ---- keys
    template <class Out> void keyOn(Out& out, int note, float vel, const Clock& clk) {
        if (physical_ == 0 && keeps()) dropKept(out);      // a new chord after letting go: the kept one goes
        const int i = find(note);
        if (i >= 0) {
            if (!e_[size_t(i)].physical) { e_[size_t(i)].physical = true; ++physical_; }
            e_[size_t(i)].vel = vel;
        } else {
            if (count_ >= kMaxNotes) removeAt(0);
            e_[size_t(count_)] = {note, vel, true};
            ++count_;
            ++physical_;
        }
        if (!p_.arpOn) { out.noteOn(note, vel); return; }
        if (!running_) start(clk);
    }
    template <class Out> void keyOff(Out& out, int note) {
        const int i = find(note);
        if (i < 0) return;
        if (e_[size_t(i)].physical) { e_[size_t(i)].physical = false; --physical_; }
        if (keeps()) return;                                // HOLD (or the pedal, arpeggiating): it stays
        removeAt(i);
        if (!p_.arpOn) out.noteOff(note);
    }
    template <class Out> void pedal(Out& out, bool down) {
        pedal_ = down;
        out.sustain(down && !p_.arpOn);                     // arpeggiating, the pedal holds the chord instead
        if (!down && p_.arpOn && !p_.arpHold) dropKept(out);
    }
    template <class Out> void allOff(Out& out) {
        for (int k = 0; k < nOff_; ++k) out.noteOff(off_[size_t(k)].note);
        reset();
    }

    // The layer's settings, every block: notices the arpeggiator or HOLD going on or off.
    template <class Out> void update(Out& out, const LayerParams& p, const Clock& clk) {
        const bool wasOn = p_.arpOn, wasHold = p_.arpHold;
        p_ = p;
        if (wasOn != p.arpOn) {
            if (p.arpOn) {                                  // the notes sounding stop; the arpeggiator takes them
                for (int i = 0; i < count_; ++i) out.noteOff(e_[size_t(i)].note);
                out.sustain(false);
                if (count_ > 0) start(clk);
            } else {                                        // the arpeggio stops; what is held sounds as it is
                flushOffs(out);
                running_ = false;
                out.sustain(pedal_);
                for (int i = 0; i < count_; ++i) out.noteOn(e_[size_t(i)].note, e_[size_t(i)].vel);
            }
        }
        if (wasHold && !p.arpHold && !(p.arpOn && pedal_)) dropKept(out);
    }

    // ---- time
    double nextBeat() const {
        double t = running_ ? next_ : kNever;
        for (int k = 0; k < nOff_; ++k) t = std::min(t, off_[size_t(k)].beat);
        return t;
    }
    // Plays whatever is due at clk.beat.
    template <class Out> void fire(Out& out, const Clock& clk) {
        const double now = clk.beat + 1e-9;
        for (int k = 0; k < nOff_;) {                       // ends first, so a note can start again on the same beat
            if (off_[size_t(k)].beat <= now) { out.noteOff(off_[size_t(k)].note); off_[size_t(k)] = off_[size_t(--nOff_)]; }
            else ++k;
        }
        if (running_ && next_ <= now) step(out, clk);
    }
    // The host jumped (a loop, a locate): what sounds stops, and the next step waits for the grid.
    template <class Out> void jumped(Out& out, const Clock& clk) {
        flushOffs(out);
        if (running_) {
            const double st = stepBeats();
            const double k = std::ceil((clk.beat - clk.origin) / st - 1e-9);
            next_ = at(clk.origin, k, st);
            k_ = k;
        }
    }

private:
    struct Entry { int note = 60; float vel = 0.8f; bool physical = false; };
    struct Off { int note; double beat; };

    bool keeps() const { return p_.arpHold || (p_.arpOn && pedal_); }
    double stepBeats() const { return double(kArpBeats[std::clamp(p_.arpDiv, 0, kNumArpDivs - 1)]); }
    // Step k of the grid: the off beats come late by the swing (at full swing, a triplet lilt).
    double at(double origin, double k, double st) const {
        const bool odd = (long long)(k) & 1;
        return origin + k * st + (odd ? double(clampf(p_.arpSwing, 0.0f, 1.0f)) * st / 3.0 : 0.0);
    }

    void start(const Clock& clk) {
        running_ = true;
        pos_ = 0;
        lastJ_ = -1;
        const double st = stepBeats();
        const double k = std::floor((clk.beat - clk.origin) / st + 1e-9);
        // On the host's grid, a key a little late for a step still plays at once; later, it waits for the next.
        if (!clk.host || clk.beat - at(clk.origin, k, st) < 0.3 * st) { next_ = clk.beat; k_ = k; }
        else { k_ = k + 1; next_ = at(clk.origin, k_, st); }
    }

    template <class Out> void step(Out& out, const Clock& clk) {
        if (count_ == 0) { running_ = false; return; }
        const double st = stepBeats();
        // The notes in order: by pitch, or as played.
        int base[kMaxNotes]; float vel[kMaxNotes];
        const int nb = count_;
        for (int i = 0; i < nb; ++i) { base[i] = e_[size_t(i)].note; vel[i] = e_[size_t(i)].vel; }
        if (p_.arpMode != kArpOrder)
            for (int i = 1; i < nb; ++i)
                for (int j = i; j > 0 && base[j - 1] > base[j]; --j) { std::swap(base[j - 1], base[j]); std::swap(vel[j - 1], vel[j]); }
        const int range = std::clamp(p_.arpRange, 1, 4);
        const int len = nb * range;
        const float gate = clampf(p_.arpGate, 0.05f, 1.0f);
        const double end = clk.beat + st * double(gate) * (gate >= 0.999f ? 1.02 : 1.0);   // full gate: a touch over, to tie
        auto play = [&](int note, float v) {
            note = std::clamp(note, 0, 127);
            for (int k = 0; k < nOff_; ++k)                 // the same note still sounding: it ends first
                if (off_[size_t(k)].note == note) { out.noteOff(note); off_[size_t(k)] = off_[size_t(--nOff_)]; break; }
            out.noteOn(note, v);
            if (nOff_ < int(off_.size())) off_[size_t(nOff_++)] = {note, end};
        };
        if (p_.arpMode == kArpChord) {
            const int oct = 12 * (pos_ % range);
            for (int i = 0; i < nb; ++i) play(base[i] + oct, vel[i]);
        } else {
            int j = 0;
            const int q2 = 2 * len - 2;
            switch (p_.arpMode) {
                case kArpDown: j = len - 1 - pos_ % len; break;
                case kArpUpDown: { if (len > 1) { const int q = pos_ % q2; j = q < len ? q : q2 - q; } break; }
                case kArpUpDown2: { const int q = pos_ % (2 * len); j = q < len ? q : 2 * len - 1 - q; break; }
                case kArpRandom: {
                    j = int(rng_.next() % uint32_t(len));
                    if (len > 1 && j == lastJ_) j = (j + 1 + int(rng_.next() % uint32_t(len - 1))) % len;
                    break;
                }
                default: j = pos_ % len; break;             // UP, and AS PLAYED (unsorted)
            }
            lastJ_ = j;
            play(base[j % nb] + 12 * (j / nb), vel[j % nb]);
        }
        ++pos_;
        if (pos_ >= 1 << 20) pos_ = 0;
        // The next step: on the grid, from now (so a new division takes over at once).
        k_ = std::floor((clk.beat - clk.origin) / st + 1e-6) + 1.0;
        next_ = at(clk.origin, k_, st);
        if (next_ <= clk.beat + 1e-9) { k_ += 1.0; next_ = at(clk.origin, k_, st); }
    }

    template <class Out> void dropKept(Out& out) {
        for (int i = 0; i < count_;)
            if (!e_[size_t(i)].physical) { const int n = e_[size_t(i)].note; removeAt(i); if (!p_.arpOn) out.noteOff(n); }
            else ++i;
    }
    template <class Out> void flushOffs(Out& out) {
        for (int k = 0; k < nOff_; ++k) out.noteOff(off_[size_t(k)].note);
        nOff_ = 0;
    }
    int find(int note) const { for (int i = 0; i < count_; ++i) if (e_[size_t(i)].note == note) return i; return -1; }
    void removeAt(int i) {
        if (e_[size_t(i)].physical) --physical_;
        for (int j = i + 1; j < count_; ++j) e_[size_t(j - 1)] = e_[size_t(j)];
        --count_;
    }

    LayerParams p_;
    std::array<Entry, kMaxNotes> e_{};
    int count_ = 0, physical_ = 0;
    std::array<Off, 96> off_{};
    int nOff_ = 0;
    bool running_ = false, pedal_ = false;
    double next_ = 0.0, k_ = 0.0;
    int pos_ = 0, lastJ_ = -1;
    Rng rng_{0x2545F491u};
};

}  // namespace td
