#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Layout.h"
#include "PluginProcessor.h"

#include <memory>
#include <vector>

// The rendered art, loaded once per process and shared by every open window.
struct TandemArt {
    TandemArt();
    juce::Image panel, drawer, leds, faderCap;
    juce::Image strip[layout::kNumStrips];
    int stripFrames[layout::kNumStrips] = {};
    juce::Typeface::Ptr legend;
};

// A display page: a title and up to four columns, one over each encoder.
struct DisplayPage {
    const char* title;
    const char* key[4];            // parameter keys; "<type>" is the filter type for the routing in use
    const char* label[4];
};

class TandemEditor final : public juce::AudioProcessorEditor, public juce::TooltipClient, private juce::Timer {
public:
    explicit TandemEditor(TandemProcessor&);
    ~TandemEditor() override { stopTimer(); }

    void paint(juce::Graphics&) override;
    void resized() override {}
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    juce::String getTooltip() override;

    void setScale(float s);
    void setDrawer(bool open);
    // For the snapshot tool and the self test.
    void showMenu(int menu, int page);
    void touch(const juce::String& key);           // as if that control had just been moved
    void openPageFor(const char* controlId);       // as if SHIFT and that control
    void latchShift() { shift_ = true; repaint(); }
    void latchAssign() { assign_ = true; repaint(); }
    void rollDice();                               // DICE: a new variation
    bool assignFor(const char* controlId, float amount);     // as if ASSIGN, then that knob turned to this amount (-1..1)
    int assignedSlot() const { return assignSlot_; }

    static const std::vector<DisplayPage>& pages(int menu);
    static const char* const kMenuNames[TandemProcessor::kMenus];

private:
    enum { kNone = -1, kDisplay = 2000, kTypeWin = 2001 };

    void timerCallback() override;

    // ---- controls
    juce::RangedAudioParameter* param(int ci) const;
    juce::String keyOf(int ci) const;
    float norm(int ci) const;            // where a knob or fader sits, 0..1 (inert ones: their resting place)
    int choice(int ci) const;            // a stepped control's position
    bool ledOn(int ci, int k) const;
    bool inert(int ci) const;            // a stage 2 control: drawn, but not yet working
    int hit(juce::Point<float> pt) const;
    juce::Rectangle<float> boundsOf(int ci) const;
    void repaintPts(juce::Rectangle<float> r);
    void press(int ci, const juce::MouseEvent&);    // a key let go over itself, or a ring clicked
    void stepParam(juce::RangedAudioParameter* p, int dir, bool wrap);
    void notice(const juce::String& text);
    void loadSlot(int slot);
    void showPresetMenu();
    void showTypeMenu();
    void showSizeMenu();
    void askPresetName();
    std::unique_ptr<juce::AlertWindow> saveDialog_;

    // ---- the display
    const DisplayPage& currentPage() const;
    juce::RangedAudioParameter* pageParam(int col) const;
    int displayColumnAt(juce::Point<float> pt) const;      // -1 the title line, 0..3 the columns
    juce::String typeKey() const;
    juce::String patchLabel() const;
    void drawDisplay(juce::Graphics&);
    void drawTypeWindow(juce::Graphics&);
    void phosphor(juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, juce::Justification, float alpha = 1.0f);

    void drawKnob(juce::Graphics&, int ci);
    void drawModMarks(juce::Graphics&, int ci);    // a modulated control's dot, and its range while touched or assigning

    // ---- the matrix, from the panel
    int destOf(int ci) const;                      // the matrix destination a control stands for (0: none)
    float destScale(int ci) const;                 // how far one unit of modulation moves it, for the arcs
    bool matrixPage() const;                       // the display shows a matrix slot (its page index is the slot)
    juce::RangedAudioParameter* slotParam(int slot, int field) const;     // field: 0 source, 1 dest, 2 amount, 3 via, 4 curve
    float slotValue(int slot, int field) const;    // in the parameter's own units
    int freeSlotFor(int dest) const;
    bool startAssign(int ci);                      // ASSIGN, then a knob: a slot for it; the drag sets its amount
    float lampAlpha(int ci) const;                 // the voice lamps: how bright
    void refreshDepths();
    void drawLed(juce::Graphics&, int li);
    void drawSprite(juce::Graphics&, const juce::Image&, float cx, float cy, float wPt, float hPt, int frame = 0, int frames = 1);
    juce::Font font(float h) const;
    float height() const;

    TandemProcessor& proc_;
    juce::SharedResourcePointer<TandemArt> art_;
    juce::TooltipWindow tips_{this, 900};
    float s_ = 1.0f;

    std::vector<juce::String> key_;      // the parameter each control works, by control index
    std::vector<uint64_t> drawn_;        // what each control looked like when last painted
    uint64_t look(int ci) const;
    float encAngle_[4] = {36, 144, 252, 90};
    double clock_ = 0;
    double odUntil_ = 0;
    juce::String drawnTitle_;

    juce::String touchedKey_, noticeText_;
    double touchedUntil_ = 0, noticeUntil_ = 0;
    bool shift_ = false;
    bool assign_ = false, assigning_ = false;     // ASSIGN latched; a knob being turned to set a new slot's amount
    int assignSlot_ = -1;
    double diceUntil_ = 0;
    float depth_[td::kNumModDests] = {};          // how much each destination is modulated (the sum of its slots' amounts)
    juce::RangedAudioParameter* slot_[td::kMatrixSlots][5] = {};

    int drag_ = kNone, dragCol_ = -1, pressed_ = kNone;
    float dragStartNorm_ = 0, dragAccum_ = 0, dragSteps_ = 0;
    juce::Point<float> dragLast_;
    bool dragMoved_ = false;
    juce::String hoverId_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TandemEditor)
};
