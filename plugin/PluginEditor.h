#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "Layout.h"
#include "PluginProcessor.h"
#include "SoundBrowser.h"

#include <functional>
#include <memory>
#include <vector>

// The rendered art, loaded once per process and shared by every open window.
struct TandemArt {
    TandemArt();
    juce::Image panel, drawer, leds;
    juce::Image faderCap[layout::kNumFaderCaps];
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

class ControlProxy;

class TandemEditor final : public juce::AudioProcessorEditor, public juce::TooltipClient, private juce::Timer {
public:
    explicit TandemEditor(TandemProcessor&);
    ~TandemEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override {}
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
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
    void selectLampFor(const char* controlId, int lamp);     // as if that lamp of a lamp key or ring were clicked
    void modulateFrom(const char* controlId, int source);    // the right-click menu's "Modulate from"
    void clearModulationOf(const char* controlId);
    void showTagFor(const char* controlId);                  // the value tag by a control, as while it is turned
    bool doUndo();
    bool doRedo();
    void focusControl(const char* controlId);                // keyboard focus, as Tab would give it
    juce::String accessibleReading(const char* controlId);   // what VoiceOver is told of a control: "role: title, value"
    void selectLayer(int layer);                             // LAYER 1 / LAYER 2: which layer the panel edits
    void swapLayers();                                       // SHIFT + LAYER 1
    void copyLayer();                                        // SHIFT + LAYER 2: the edited layer onto the other
    void tap(double ms);                                     // TAP, at this time (milliseconds)
    void pressControl(const char* controlId);               // as if that key were clicked
    void copySlotToOtherLayer(int slot);                     // the overview's right-click: "Copy it to layer N"
    void askPresetName(int what = 2);                        // WRITE's name dialog. 0, 1: that layer as a patch; 2: both, a performance
    juce::AlertWindow* dialog() { return dialog_.get(); }
    void openBrowser(int layer);                             // a layer's sound browser, under its name window
    void closeBrowser();
    SoundBrowser* browser() { return browser_.get(); }
    // For tests: one frame of the window's timer, and every area it asks to be repainted.
    void tick() { timerCallback(); }
    std::function<void(juce::Rectangle<int>)> repaintHook;
    static constexpr int kMatrixFirst = 1;                   // the MOD menu: the overview, then a page a slot

    static const std::vector<DisplayPage>& pages(int menu);
    static const char* const kMenuNames[TandemProcessor::kMenus];

private:
    friend class ControlProxy;
    enum { kNone = -1, kDisplay = 2000, kTypeWin = 2001, kPatchWin1 = 2002, kPatchWin2 = 2003, kArpWin = 2004, kLayerNum1 = 2005, kLayerNum2 = 2006 };
    // The arpeggiator's window: its three parts (mode, range, division) for the edited layer.
    static juce::Rectangle<float> arpWin();
    static int arpPartAt(juce::Point<float> pt);
    static const char* arpPartKey(int part);
    void drawArpWindow(juce::Graphics&);
    void showArpPartMenu(int part);
    void drawLayerMarks(juce::Graphics&);              // "LAYER n" on the header of each box that shows the edited layer
    bool extInert() const;                             // EXT IN does nothing just now (no side chain; layer 2 outside CASCADE)
    int layerSeen_ = -1;
    std::unique_ptr<SoundBrowser> browser_;
    void placeBrowser();

    void timerCallback() override;

    // ---- controls
    juce::RangedAudioParameter* param(int ci) const;
    juce::String keyOf(int ci) const;
    float norm(int ci) const;            // where a knob or fader sits, 0..1
    int choice(int ci) const;            // a stepped control's position
    bool ledOn(int ci, int k) const;
    int hit(juce::Point<float> pt) const;
    juce::Rectangle<float> boundsOf(int ci) const;
    void repaintPts(juce::Rectangle<float> r);
    void press(int ci, bool back = false);           // a key let go over itself, or a ring clicked: the next setting
    int lampAt(int ci, juce::Point<float> pt) const;   // which of a lamp key's or ring's lamps (or their legends) a point is on
    void selectLamp(int ci, int k);                    // straight to the setting that lamp stands for
    void showControlMenu(int ci);                      // right-click: its page, its default, modulation
    void showPageMenu();                               // the display's title: every page of this menu
    void drawValueTag(juce::Graphics&);
    juce::Rectangle<float> tagRect(int ci, const juce::String& text) const;
    juce::String tagText(int ci) const;
    void drawOverview(juce::Graphics&);                // the MOD menu's first page: every slot in use
    int overviewSlotAt(juce::Point<float> pt) const;
    void showSlotMenu(int slot);                       // right-click on a line of the overview
    int ovSlot_ = -1;                                  // the overview line being dragged (its amount)
    void accessibleNudge(int ci, int dir, bool fine);
    void accessibleActivate(int ci);
    void focusChanged(int ci);
    void stepParam(juce::RangedAudioParameter* p, int dir, bool wrap);
    void notice(const juce::String& text);
    void loadSlot(int slot);
    void showPresetMenu();
    void showTypeMenu();
    void showSizeMenu();
    void showWriteMenu();
    // ---- each layer's patch window
    void drawPatchWindow(juce::Graphics&, int layer);
    void showLayerMenu(int layer);
    // ---- your own waves: imported into one of 8 slots, chosen in any wave slot's list
    void showWaveMenu(juce::RangedAudioParameter* slot);
    void importWaveInto(juce::RangedAudioParameter* slot);
    std::unique_ptr<juce::FileChooser> chooser_;
    void loadLayer(int index, int layer, int part);
    juce::Rectangle<float> patchWin(int layer) const;
    bool patchEdited_[2] = {false, false};
    juce::String patchSeen_;
    // ---- the drawer slides up over the bottom of the panel: its controls sit this far above where the layout puts them
    float dy(int ci) const { return ci >= 0 && ci < layout::kNumControls && layout::kControls[ci].drawer ? -layout::kDrawerH : 0.0f; }
    std::unique_ptr<juce::AlertWindow> dialog_;                    // a dialog, inside the window
    juce::PopupMenu::Options menuOptions();
    void showDialog(std::unique_ptr<juce::AlertWindow>, std::function<void(int)> done);
    void showAlert(const juce::String& title, const juce::String& text);

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
    bool matrixPage() const;                       // the display shows a matrix slot
    int shownSlot() const;                         // which (or -1)
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
    std::vector<uint64_t> drawn_;        // what each control looked like when the timer last had it repainted
    uint64_t look(int ci) const;
    float encAngle_[4] = {36, 144, 252, 90};
    double clock_ = 0;
    double odUntil_ = 0;

    juce::String touchedKey_, noticeText_;
    double touchedUntil_ = 0, noticeUntil_ = 0;
    bool shift_ = false;
    bool assign_ = false, assigning_ = false;     // ASSIGN latched; a knob being turned to set a new slot's amount
    int assignSlot_ = -1;
    double diceUntil_ = 0;
    float depth_[td::kNumModDests] = {};          // how much each destination is modulated (the sum of its slots' amounts)
    juce::RangedAudioParameter* slot_[2][td::kMatrixSlots][5] = {};     // per layer
    std::vector<double> taps_;                    // TAP: when it was last pressed (ms)
    double learnUntil_ = 0;                       // ZONES: the next key played sets the split, till then
    bool layerKeyLike(const juce::String& key) const;  // a layer's key (not the instrument's)
    int pressLamp_ = -1;                          // the lamp a key press began on
    int tagCi_ = -1;                              // the control whose value tag shows, till tagUntil_
    double tagUntil_ = 0;
    juce::Rectangle<float> tagShown_;
    int focusCi_ = -1;                            // the control with keyboard focus (kDisplay: the display)
    std::vector<std::unique_ptr<ControlProxy>> proxies_;    // one per control, for the keyboard and VoiceOver

    int drag_ = kNone, dragCol_ = -1, pressed_ = kNone;
    float dragStartNorm_ = 0, dragAccum_ = 0, dragSteps_ = 0;
    juce::Point<float> dragLast_;
    bool dragMoved_ = false;
    juce::String hoverId_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TandemEditor)
};
