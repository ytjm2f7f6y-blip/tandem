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
    juce::Image panel;
    juce::Image strip[4];          // large, med, small (the encoders, all the way round), selector
    int stripFrames[4] = {0, 0, 0, 0};
    juce::Image toggle[3];         // lever left, centre, right
    juce::Image faderCap;
    juce::Image lit[layout::kNumLitGroups];
    juce::Typeface::Ptr marker, legend;
};

// The display's pages: a title and up to three columns, set by encoders B, C and D.
struct DisplayPage {
    const char* title;
    const char* key[3];            // parameter keys; "<type>" is the filter type for the routing in use
    const char* label[3];
};

class TandemEditor final : public juce::AudioProcessorEditor, private juce::Timer {
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

    void setScale(float s);
    // For the snapshot tool.
    void showHelp(const char* id) { hoverId_ = id; }
    void showMenu(int menu, int page);
    void touch(const juce::String& key);          // as if that control had just been moved

    static constexpr float kHelpH = 72.0f;
    static const std::vector<DisplayPage>& pages(int menu);

private:
    enum { kNone = -1, kDisplay = 2000, kTypeWin = 2001, kTape = 2002 };

    void timerCallback() override;

    // ---- controls
    juce::RangedAudioParameter* param(int ci) const;
    juce::String keyOf(int ci) const;
    float norm(int ci) const;
    int selIndex(int ci) const;
    int togglePos(int ci) const;
    void setTogglePos(int ci, int pos);
    bool lit(int ci) const;
    int hit(juce::Point<float> pt) const;
    juce::Rectangle<float> boundsOf(int ci) const;
    void repaintPts(juce::Rectangle<float> r);
    void click(int ci, const juce::MouseEvent&);
    void stepParam(juce::RangedAudioParameter* p, int dir, bool wrap);
    void showSetMenu();
    void showPresetMenu();
    void showTypeMenu();
    void askPresetName();
    std::unique_ptr<juce::AlertWindow> saveDialog_;
    juce::String drawnPreset_;
    int drawnAb_ = -1, drawnMenu_ = -1;

    // ---- the display
    const DisplayPage& currentPage() const;
    juce::RangedAudioParameter* pageParam(int col) const;      // col 0..2 = encoders B..D
    int displayColumnAt(juce::Point<float> pt) const;          // -1 title (page), 0..2 columns
    juce::String typeKey() const;
    void drawDisplay(juce::Graphics&);
    void drawTypeWindow(juce::Graphics&);
    void phosphor(juce::Graphics&, const juce::String&, juce::Rectangle<float>, float size, juce::Justification, float alpha = 1.0f);

    void drawKnob(juce::Graphics&, int ci);
    void drawLit(juce::Graphics&, int ci, const juce::Image& from);
    void drawPresetName(juce::Graphics&);
    void drawHelp(juce::Graphics&);
    void drawSprite(juce::Graphics&, const juce::Image&, float cx, float cy, float wPt, float hPt, int frame = 0, int frames = 1);
    juce::Font font(float h) const;

    TandemProcessor& proc_;
    juce::SharedResourcePointer<TandemArt> art_;
    float s_ = 1.0f;

    struct Painted { int frame = -1; juce::Image image; };
    std::vector<Painted> painted_;
    const juce::Image& paintedFrame(int ci, int frame);

    std::vector<juce::String> key_;     // the parameter key each control works, by control index
    std::vector<float> drawn_;
    float encAngle_[4] = {36, 144, 252, 90};
    double clock_ = 0;
    double odUntil_ = 0;
    bool odDrawn_ = false;

    // The last control moved, shown on the display for a moment.
    juce::String touchedKey_;
    double touchedUntil_ = 0;
    std::vector<float> lastValues_;     // every parameter's last seen value, to spot moves from the host too

    int drag_ = kNone, dragCol_ = -1, pressed_ = kNone;
    float dragStartNorm_ = 0, dragAccum_ = 0, dragSteps_ = 0;
    juce::Point<float> dragLast_;
    bool dragMoved_ = false;
    juce::String hoverId_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TandemEditor)
};
