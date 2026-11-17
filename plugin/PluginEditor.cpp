#include "PluginEditor.h"

#include "BinaryData.h"
#include "HelpText.h"

#include <cmath>
#include <cstring>

using namespace layout;

namespace {

constexpr double kFrameRate = 30.0;

juce::Image load(const void* data, int size) { return juce::ImageFileFormat::loadFrom(data, size_t(size)); }

// A round knob's strip: colour from a JPEG (premultiplied on black), outline from one mask every frame shares.
juce::Image roundStrip(const void* jpg, int jpgSize, const void* mask, int maskSize) {
    const juce::Image rgb = load(jpg, jpgSize), m = load(mask, maskSize);
    if (!rgb.isValid() || !m.isValid()) return {};
    const int w = rgb.getWidth(), h = rgb.getHeight(), mw = m.getWidth();
    juce::Image out(juce::Image::ARGB, w, h, true);
    const juce::Image::BitmapData src(rgb, juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData msk(m, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst(out, juce::Image::BitmapData::writeOnly);
    std::vector<uint8_t> alpha(size_t(mw * mw));
    for (int y = 0; y < mw; ++y)
        for (int x = 0; x < mw; ++x) alpha[size_t(y * mw + x)] = msk.getPixelColour(x, y).getRed();
    for (int y = 0; y < h; ++y) {
        const int my = y % mw;
        for (int x = 0; x < w && x < mw; ++x) {
            const auto a = alpha[size_t(my * mw + x)];
            if (a == 0) continue;
            const auto c = src.getPixelColour(x, y);
            auto* px = reinterpret_cast<juce::PixelARGB*>(dst.getPixelPointer(x, y));
            px->setARGB(a, std::min(c.getRed(), a), std::min(c.getGreen(), a), std::min(c.getBlue(), a));
        }
    }
    return out;
}

float knobBox(int kind) { return 2 * kKnobRadius[kind] + (kind == kSel ? 20.0f : 12.0f); }

int indexOf(const char* id) {
    for (int i = 0; i < kNumControls; ++i)
        if (std::strcmp(kControls[i].id, id) == 0) return i;
    return -1;
}
bool is(const Control& c, const char* id) { return std::strcmp(c.id, id) == 0; }

// Which parameter each control works.
struct Map { const char* control; const char* key; };
const Map kMap[] = {
    {"o1_wave", "osc1_wave"}, {"o2_wave", "osc2_wave"}, {"o1_tune", "osc1_tune"}, {"o1_fine", "osc1_fine"}, {"o2_tune", "osc2_tune"}, {"o2_fine", "osc2_fine"},
    {"o_sync", "sync"}, {"octave", "octave"}, {"m_osc", "mod_osc"}, {"m_wsrc", "wave_src"}, {"m_psrc", "pitch_src"}, {"m_detune", "detune"},
    {"m_wave", "wave_mod"}, {"m_pitch", "pitch_mod"}, {"f_cutoff", "cutoff"}, {"f_res", "resonance"}, {"f_kt", "keytrack"}, {"f_restype", "res_type"},
    {"f_mod", "vcf_mod"}, {"f_msrc", "vcf_src"}, {"f_delta", "delta"}, {"f_route", "route"}, {"x_osc1", "osc1_level"}, {"x_osc2", "osc2_level"},
    {"x_ext", "ext_level"}, {"l_mode", "lfo1_mode"}, {"l_rate", "lfo1_rate"}, {"l_delay", "lfo1_delay"}, {"l_wave", "lfo1_wave"},
    {"e1_mode", "e1_mode"}, {"e1_h", "e1_hold"}, {"e1_a", "e1_attack"}, {"e1_d", "e1_decay"}, {"e1_s", "e1_sustain"}, {"e1_r", "e1_release"},
    {"e2_a", "e2_attack"}, {"e2_d", "e2_decay"}, {"e2_s", "e2_sustain"}, {"e2_r", "e2_release"}, {"vca_src", "vca_src"},
    {"e1_vel", "vel_vcf"}, {"e2_vel", "vel_vca"}, {"level", "level"}, {"pan", "pan"}, {"fx_send", "fx_send"}, {"glide", "glide"}, {"volume", "volume"},
};

const help::Entry* helpFor(const juce::String& id) {
    for (int i = 0; i < help::kNumEntries; ++i)
        if (id == help::kEntries[i].id) return &help::kEntries[i];
    return nullptr;
}

// The display's phosphor.
const juce::Colour kGlow(0xffa6ffc9), kGlowDim(0x9a8de0ad), kGlowFaint(0x2a7dffb0);

}  // namespace

// ---------------------------------------------------------------------------
const std::vector<DisplayPage>& TandemEditor::pages(int menu) {
    static const std::vector<DisplayPage> voice = {
        {"VOICE", {"voice_mode", "unison", "voices"}, {"MODE", "UNISON", "VOICES"}},
        {"VOICE VARIATION", {"drift", "pan_spread", nullptr}, {"DRIFT", "PAN", nullptr}},
        {"VCF PATH", {"route", "<type>", "res_type"}, {"PATH", "TYPE", "RES"}},
        {"OSC 1 WAVES 1-3", {"osc1_alt1", "osc1_alt2", "osc1_alt3"}, {"SLOT 1", "SLOT 2", "SLOT 3"}},
        {"OSC 1 WAVES 4-6", {"osc1_alt4", "osc1_alt5", "osc1_alt6"}, {"SLOT 4", "SLOT 5", "SLOT 6"}},
        {"OSC 2 WAVES", {"osc2_alt1", "osc2_alt2", "osc2_alt3"}, {"SLOT 1", "SLOT 2", "SLOT 3"}},
        {"LFO 1", {"lfo1_sync", "lfo1_phase", nullptr}, {"CLOCK", "PHASE", nullptr}},
        {"LFO 1 USER WAVES", {"lfo_alt1", "lfo_alt2", nullptr}, {"USER 1", "USER 2", nullptr}},
        {"PORTAMENTO", {"glide", "octave", nullptr}, {"RATE", "OCTAVE", nullptr}},
    };
    static const std::vector<DisplayPage> mod = [] {
        std::vector<DisplayPage> v;
        static const char* titles[7] = {"MATRIX 1", "MATRIX 2", "MATRIX 3", "MATRIX 4", "MATRIX 5", "MATRIX 6", "MATRIX 7"};
        static const char* src[7] = {"m1_src", "m2_src", "m3_src", "m4_src", "m5_src", "m6_src", "m7_src"};
        static const char* dst[7] = {"m1_dst", "m2_dst", "m3_dst", "m4_dst", "m5_dst", "m6_dst", "m7_dst"};
        static const char* amt[7] = {"m1_amt", "m2_amt", "m3_amt", "m4_amt", "m5_amt", "m6_amt", "m7_amt"};
        for (int i = 0; i < 7; ++i) v.push_back({titles[i], {src[i], dst[i], amt[i]}, {"SOURCE", "DEST", "AMOUNT"}});
        v.push_back({"LFO 2", {"lfo2_trig", "lfo2_wave", "lfo2_rate"}, {"TRIGGER", "WAVE", "RATE"}});
        v.push_back({"LFO 2 DEPTH", {"lfo2_pitch", "lfo2_vcf", "lfo2_vca"}, {"PITCH", "VCF", "VCA"}});
        v.push_back({"LFO 2 RATE", {"lfo2_ratemod", "lfo2_delay", nullptr}, {"RATE MOD", "DELAY", nullptr}});
        v.push_back({"ENV 3", {"e3_mode", "e3_hold", "e3_attack"}, {"MODE", "HOLD", "ATTACK"}});
        v.push_back({"ENV 3 (2)", {"e3_decay", "e3_sustain", "e3_release"}, {"DECAY", "SUSTAIN", "RELEASE"}});
        v.push_back({"VELOCITY", {"vel_vca", "vel_vcf", nullptr}, {"VCA", "ENV 1", nullptr}});
        v.push_back({"BENDER", {"bend_range", "bend_vcf", nullptr}, {"PITCH", "VCF", nullptr}});
        return v;
    }();
    static const std::vector<DisplayPage> fx = {
        {"DRIVE", {"drv_mode", "drv_tone", "drv_amount"}, {"MODE", "TONE", "DRIVE"}},
        {"DRIVE MIX", {"drv_mix", nullptr, nullptr}, {"WET/DRY", nullptr, nullptr}},
        {"CHORUS", {"ch_on", "ch_rate", "ch_depth"}, {"ON", "RATE", "DEPTH"}},
        {"CHORUS (2)", {"ch_delay", "ch_mix", nullptr}, {"DELAY", "MIX", nullptr}},
        {"EQUALIZER", {"eq_low", "eq_mid", "eq_high"}, {"LOW", "MID", "HIGH"}},
        {"EQ MID WIDTH", {"eq_q", nullptr, nullptr}, {"WIDTH", nullptr, nullptr}},
        {"DELAY", {"dly_time", "dly_fb", "fx_balance"}, {"TIME", "FEEDBACK", "DLY/REV"}},
        {"DELAY (2)", {"dly_mode", "dly_damp", "freeze"}, {"MODE", "DAMPING", "FREEZE"}},
        {"REVERB", {"rv_size", "rv_damp", "fx_balance"}, {"SIZE", "DAMPING", "DLY/REV"}},
        {"REVERB (2)", {"rv_lowcut", "rv_predelay", "rv_width"}, {"LOW CUT", "PRE-DLY", "WIDTH"}},
    };
    return menu == 0 ? voice : (menu == 1 ? mod : fx);
}

TandemArt::TandemArt() {
    namespace B = BinaryData;
    panel = load(B::panel_jpg, B::panel_jpgSize);
    strip[kLarge] = roundStrip(B::knob_large_jpg, B::knob_large_jpgSize, B::knob_large_mask_png, B::knob_large_mask_pngSize);
    strip[kMed] = roundStrip(B::knob_med_jpg, B::knob_med_jpgSize, B::knob_med_mask_png, B::knob_med_mask_pngSize);
    strip[kSmall] = roundStrip(B::knob_small_jpg, B::knob_small_jpgSize, B::knob_small_mask_png, B::knob_small_mask_pngSize);
    strip[kSel] = load(B::knob_sel_png, B::knob_sel_pngSize);
    for (int k = 0; k < 4; ++k) stripFrames[k] = strip[k].isValid() ? strip[k].getHeight() / strip[k].getWidth() : 0;
    toggle[0] = load(B::toggle_l_png, B::toggle_l_pngSize);
    toggle[1] = load(B::toggle_c_png, B::toggle_c_pngSize);
    toggle[2] = load(B::toggle_r_png, B::toggle_r_pngSize);
    faderCap = load(B::fader_cap_png, B::fader_cap_pngSize);
    lit[0] = load(B::lit_menu_png, B::lit_menu_pngSize);
    lit[1] = load(B::lit_odrive_png, B::lit_odrive_pngSize);
    lit[2] = load(B::lit_power_png, B::lit_power_pngSize);
    lit[3] = load(B::lit_ab_png, B::lit_ab_pngSize);
    marker = juce::Typeface::createSystemTypefaceFor(B::CaveatBrushRegular_ttf, B::CaveatBrushRegular_ttfSize);
    legend = juce::Typeface::createSystemTypefaceFor(B::BarlowSemiCondensedSemiBold_ttf, B::BarlowSemiCondensedSemiBold_ttfSize);
}

// ---------------------------------------------------------------------------
TandemEditor::TandemEditor(TandemProcessor& p) : AudioProcessorEditor(p), proc_(p) {
    key_.assign(size_t(kNumControls), {});
    painted_.resize(size_t(kNumControls));
    drawn_.assign(size_t(kNumControls), -1e9f);
    for (const auto& m : kMap) {
        const int ci = indexOf(m.control);
        if (ci >= 0) key_[size_t(ci)] = m.key;
    }
    setOpaque(true);
    setWantsKeyboardFocus(false);
    setScale(proc_.uiScale);
    startTimerHz(int(kFrameRate));
}

void TandemEditor::setScale(float s) {
    s_ = juce::jlimit(0.5f, 1.5f, s);
    proc_.uiScale = s_;
    setSize(int(std::round(W * s_)), int(std::round((H + kHelpH) * s_)));
    repaint();
}

void TandemEditor::showMenu(int menu, int page) {
    proc_.menu = juce::jlimit(0, 2, menu);
    proc_.page[proc_.menu] = juce::jlimit(0, int(pages(proc_.menu).size()) - 1, page);
    touchedUntil_ = 0;
    repaint();
}

void TandemEditor::touch(const juce::String& key) {
    touchedKey_ = key;
    touchedUntil_ = clock_ + 1.6;
    repaintPts({kDispX - 2, kDispY - 2, kDispW + 4, kDispH + 4});
}

juce::Font TandemEditor::font(float h) const { return juce::Font(juce::FontOptions(art_->legend).withHeight(h * s_)); }

// ---------------------------------------------------------------------------
// Controls
juce::String TandemEditor::keyOf(int ci) const { return ci >= 0 && ci < kNumControls ? key_[size_t(ci)] : juce::String(); }

juce::RangedAudioParameter* TandemEditor::param(int ci) const {
    const auto k = keyOf(ci);
    return k.isEmpty() ? nullptr : proc_.param(k);
}

float TandemEditor::norm(int ci) const { auto* p = param(ci); return p ? p->getValue() : 0.0f; }

int TandemEditor::selIndex(int ci) const {
    auto* p = param(ci);
    return p ? juce::jlimit(0, kControls[ci].nSteps - 1, juce::roundToInt(p->convertFrom0to1(p->getValue()))) : 0;
}

// Toggles: position on the panel (left to right) to the parameter's value. ENV 1's mode is
// printed INV, LOOP, NORM but stored normal, inverted, loop.
int TandemEditor::togglePos(int ci) const {
    auto* p = param(ci);
    if (!p) return 0;
    const int v = juce::roundToInt(p->convertFrom0to1(p->getValue()));
    if (is(kControls[ci], "e1_mode")) return v == 1 ? 0 : (v == 2 ? 1 : 2);
    return juce::jlimit(0, kControls[ci].nPos - 1, v);
}

void TandemEditor::setTogglePos(int ci, int pos) {
    auto* p = param(ci);
    if (!p) return;
    int v = pos;
    if (is(kControls[ci], "e1_mode")) v = pos == 0 ? 1 : (pos == 1 ? 2 : 0);
    p->beginChangeGesture();
    TandemProcessor::set(p, float(v));
    p->endChangeGesture();
    touch(keyOf(ci));
}

bool TandemEditor::lit(int ci) const {
    const auto& c = kControls[ci];
    if (is(c, "power")) return true;
    if (is(c, "odrive")) return clock_ < odUntil_;
    if (is(c, "menu_voice")) return proc_.menu == 0;
    if (is(c, "menu_mod")) return proc_.menu == 1;
    if (is(c, "menu_fx")) return proc_.menu == 2;
    if (is(c, "ab_a")) return proc_.abSlot() == 0;
    if (is(c, "ab_b")) return proc_.abSlot() == 1;
    return false;
}

juce::Rectangle<float> TandemEditor::boundsOf(int ci) const {
    const auto& c = kControls[ci];
    switch (c.type) {
        case Knob: { const float b = knobBox(c.kind); return {c.x - b / 2, c.y - b / 2, b, b}; }
        case Toggle: return {c.x - kToggleBox / 2, c.y - kToggleBox / 2, kToggleBox, kToggleBox};
        case Fader: return {c.x - kFaderBoxW / 2 - 1, c.y - c.travel / 2 - kFaderBoxH / 2 - 1, kFaderBoxW + 2, c.travel + kFaderBoxH + 2};
        case Button:
        case Lamp: return c.litGroup >= 0 ? juce::Rectangle<float>(c.lx, c.ly, c.lw, c.lh) : juce::Rectangle<float>(c.x - c.w / 2 - 4, c.y - c.h / 2 - 4, c.w + 8, c.h + 8);
    }
    return {};
}

void TandemEditor::repaintPts(juce::Rectangle<float> r) {
    if (!r.isEmpty()) repaint((r * s_).getSmallestIntegerContainer().expanded(2));
}

int TandemEditor::hit(juce::Point<float> pt) const {
    for (int i = 0; i < kNumControls; ++i) {
        const auto& c = kControls[i];
        switch (c.type) {
            case Knob: if (pt.getDistanceFrom({c.x, c.y}) < kKnobRadius[c.kind] + 4) return i; break;
            case Toggle: if (std::abs(pt.x - c.x) < 24 && std::abs(pt.y - c.y) < 14) return i; break;
            case Fader: if (std::abs(pt.x - c.x) < 11 && std::abs(pt.y - c.y) < c.travel / 2 + kFaderCapH / 2 + 2) return i; break;
            case Button: if (juce::Rectangle<float>(c.x - c.w / 2 - 2, c.y - c.h / 2 - 3, c.w + 4, c.h + 6).contains(pt)) return i; break;
            case Lamp: if (pt.getDistanceFrom({c.x, c.y}) < c.w / 2 + 4) return i; break;
        }
    }
    if (juce::Rectangle<float>(kDispX, kDispY, kDispW, kDispH).contains(pt)) return kDisplay;
    if (juce::Rectangle<float>(kTypeX, kTypeY, kTypeW, kTypeH).contains(pt)) return kTypeWin;
    if (juce::Rectangle<float>(kPresetTapeX - kPresetTapeW / 2, kPresetTapeY - kPresetTapeH / 2, kPresetTapeW, kPresetTapeH).contains(pt)) return kTape;
    return kNone;
}

void TandemEditor::stepParam(juce::RangedAudioParameter* p, int dir, bool wrap) {
    if (!p) return;
    const int n = p->getNumSteps();
    const bool discrete = n > 1 && n < 200;
    p->beginChangeGesture();
    if (discrete) {
        const int cur = juce::roundToInt(p->getValue() * float(n - 1));
        int next = cur + dir;
        next = wrap ? (next % n + n) % n : juce::jlimit(0, n - 1, next);
        p->setValueNotifyingHost(float(next) / float(n - 1));
    } else {
        p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + 0.01f * float(dir)));
    }
    p->endChangeGesture();
}

juce::String TandemEditor::typeKey() const {
    const int r = juce::roundToInt(proc_.value("route"));
    return r == 0 ? "type_st" : (r == 1 ? "type_par" : "type_ser");
}

void TandemEditor::click(int ci, const juce::MouseEvent& e) {
    const auto& c = kControls[ci];
    const bool back = e.mods.isPopupMenu() || e.mods.isAltDown();
    if (is(c, "menu_voice") || is(c, "menu_mod") || is(c, "menu_fx")) {
        const int m = is(c, "menu_voice") ? 0 : (is(c, "menu_mod") ? 1 : 2);
        if (proc_.menu == m) showMenu(m, (proc_.page[m] + 1) % int(pages(m).size()));   // pressed again: the next page
        else showMenu(m, proc_.page[m]);
        return;
    }
    if (is(c, "f_prev") || is(c, "f_next")) { stepParam(proc_.param(typeKey()), is(c, "f_prev") ? -1 : 1, true); touch(typeKey()); repaint(); return; }
    if (is(c, "menu")) { showSetMenu(); return; }
    if (is(c, "preset_prev")) { proc_.stepPreset(-1); repaint(); return; }
    if (is(c, "preset_next")) { proc_.stepPreset(1); repaint(); return; }
    if (is(c, "ab_a")) { proc_.selectAB(0); repaint(); return; }
    if (is(c, "ab_b")) { proc_.selectAB(1); repaint(); return; }
    if (c.type == Knob && c.kind == kSel) {
        stepParam(param(ci), back ? -1 : 1, true);
        touch(keyOf(ci));
        return;
    }
    if (c.type == Toggle) {
        const float dx = e.position.x / s_ - c.x;
        int pos;
        if (c.nPos == 3) pos = dx < -5 ? 0 : (dx > 5 ? 2 : 1);
        else pos = std::abs(dx) < 4 ? 1 - togglePos(ci) : (dx < 0 ? 0 : 1);
        setTogglePos(ci, pos);
        repaintPts(boundsOf(ci));
    }
}

void TandemEditor::showSetMenu() {
    juce::PopupMenu m;
    m.addSectionHeader("Window size");
    for (float s : {0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.25f})
        m.addItem(juce::String(juce::roundToInt(s * 100)) + " %", true, std::abs(s_ - s) < 0.01f, [this, s] { setScale(s); });
    m.addSeparator();
    m.addItem("Tandem " JucePlugin_VersionString " (stage 1: layer 1)", false, false, [] {});
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::showPresetMenu() {
    juce::PopupMenu m;
    const auto list = proc_.presetList();
    m.addSectionHeader("Factory");
    bool userHeader = false;
    for (int i = 0; i < int(list.size()); ++i) {
        if (!list[size_t(i)].factory && !userHeader) { m.addSectionHeader("Yours"); userHeader = true; }
        m.addItem(list[size_t(i)].name, true, list[size_t(i)].name == proc_.presetName(), [this, i] { proc_.loadPreset(i); repaint(); });
    }
    m.addSeparator();
    m.addItem("Save preset...", [this] { askPresetName(); });
    juce::PopupMenu del;
    for (int i = 0; i < int(list.size()); ++i)
        if (!list[size_t(i)].factory) del.addItem(list[size_t(i)].name, [this, i] { proc_.deleteUserPreset(i); repaint(); });
    if (del.getNumItems() > 0) m.addSubMenu("Delete", del);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::showTypeMenu() {
    juce::PopupMenu m;
    auto* p = proc_.param(typeKey());
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(p);
    if (!choice) return;
    m.addSectionHeader(proc_.param("route")->getCurrentValueAsText() + " filter types");
    for (int i = 0; i < choice->choices.size(); ++i)
        m.addItem(choice->choices[i], true, i == choice->getIndex(), [this, p, i] {
            p->beginChangeGesture(); TandemProcessor::set(p, float(i)); p->endChangeGesture(); touch(typeKey()); repaint();
        });
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::askPresetName() {
    saveDialog_ = std::make_unique<juce::AlertWindow>("Save preset", "Name this sound:", juce::MessageBoxIconType::NoIcon, this);
    saveDialog_->addTextEditor("name", proc_.presetName().upToFirstOccurrenceOf(" (", false, false));
    saveDialog_->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    saveDialog_->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TandemEditor> safe(this);
    saveDialog_->enterModalState(true, juce::ModalCallbackFunction::create([safe](int result) {
        if (safe == nullptr || safe->saveDialog_ == nullptr) return;
        const auto name = safe->saveDialog_->getTextEditorContents("name");
        if (result == 1) {
            const auto err = safe->proc_.saveUserPreset(name);
            if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Couldn't save the preset", err);
        }
        juce::MessageManager::callAsync([safe] { if (safe != nullptr) { safe->saveDialog_.reset(); safe->repaint(); } });
    }), false);
}

// ---------------------------------------------------------------------------
// The display
const DisplayPage& TandemEditor::currentPage() const {
    const auto& ps = pages(proc_.menu);
    return ps[size_t(juce::jlimit(0, int(ps.size()) - 1, proc_.page[proc_.menu]))];
}

juce::RangedAudioParameter* TandemEditor::pageParam(int col) const {
    const auto& pg = currentPage();
    if (col < 0 || col > 2 || pg.key[col] == nullptr) return nullptr;
    if (std::strcmp(pg.key[col], "<type>") == 0) return proc_.param(typeKey());
    return proc_.param(pg.key[col]);
}

int TandemEditor::displayColumnAt(juce::Point<float> pt) const {
    if (pt.y < kDispY + 16) return -1;
    return juce::jlimit(0, 2, int((pt.x - kDispX) / (kDispW / 3)));
}

void TandemEditor::phosphor(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float size, juce::Justification j, float alpha) {
    const auto rr = r * s_;
    g.setFont(font(size));
    g.setColour(kGlowFaint.withMultipliedAlpha(alpha));
    for (auto off : {juce::Point<float>(-0.7f, 0), juce::Point<float>(0.7f, 0), juce::Point<float>(0, -0.7f), juce::Point<float>(0, 0.7f)})
        g.drawText(text, rr.translated(off.x * s_, off.y * s_), j, false);
    g.setColour(kGlow.withMultipliedAlpha(alpha));
    g.drawText(text, rr, j, false);
}

namespace {
// A value as the display shows it: the LFO 1 rate depends on its wave and clock sync.
juce::String valueText(TandemProcessor& proc, juce::RangedAudioParameter* p) {
    if (!p) return {};
    if (p == proc.param("lfo1_rate")) {
        const int w = juce::roundToInt(proc.value("lfo1_wave"));
        const float v = p->getValue();
        if (w == td::kLHF) return tp::hz(20.0f * std::pow(1000.0f, v));
        if (w == td::kLHFTrack) return tp::semis((v - 0.5f) * 24.0f, 0);
        if (proc.value("lfo1_sync") >= 0.5f) {
            static const char* divs[td::kNumLfoDivs] = {"4 bars", "2 bars", "1/1.", "1/1", "1/2.", "1/1T", "1/2", "1/4.", "1/2T", "1/4", "1/8.", "1/4T", "1/8", "1/16.", "1/8T", "1/16", "1/16T", "1/32"};
            return divs[juce::jlimit(0, td::kNumLfoDivs - 1, int(v * (td::kNumLfoDivs - 1) + 0.5f))];
        }
        return tp::hz(0.03f * std::pow(40.0f / 0.03f, v));
    }
    if (p == proc.param("lfo1_mode") && juce::roundToInt(proc.value("lfo1_wave")) >= td::kLHF) {
        static const char* hf[3] = {"Free", "To OSC 1", "To OSC 2"};
        return hf[juce::jlimit(0, 2, juce::roundToInt(proc.value("lfo1_mode")))];
    }
    if (p->getNumSteps() == 2) return p->getValue() >= 0.5f ? "On" : "Off";
    return p->getCurrentValueAsText();
}
}  // namespace

void TandemEditor::drawDisplay(juce::Graphics& g) {
    juce::Graphics::ScopedSaveState ss(g);
    const auto face = juce::Rectangle<float>(kDispX, kDispY, kDispW, kDispH);
    g.reduceClipRegion((face * s_).toNearestInt());
    // The phosphor's faint glow behind the glass.
    juce::ColourGradient glow(juce::Colour(0x1a5dff9a), face.getCentreX() * s_, face.getCentreY() * s_, juce::Colour(0x005dff9a), face.getX() * s_, face.getY() * s_, true);
    g.setGradientFill(glow);
    g.fillRect(face * s_);

    auto bar = [&](juce::Rectangle<float> r, float v, bool bipolar, float alpha) {
        const int cells = 12;
        const float cw = r.getWidth() / cells;
        for (int k = 0; k < cells; ++k) {
            const float c0 = float(k) / cells, c1 = float(k + 1) / cells;
            bool on = bipolar ? (v >= 0.5f ? (c1 > 0.5f && c0 < v) : (c0 < 0.5f && c1 > v)) : (c0 < v);
            const auto cell = juce::Rectangle<float>(r.getX() + k * cw + 0.6f, r.getY(), cw - 1.2f, r.getHeight()) * s_;
            g.setColour(on ? kGlow.withMultipliedAlpha(alpha) : kGlowFaint.withMultipliedAlpha(alpha));
            g.fillRect(cell);
        }
    };
    auto dots = [&](juce::Rectangle<float> r, int n, int sel, float alpha) {
        if (n > 16) {          // too many for dots: a position line
            bar(r, (float(sel) + 0.5f) / float(n), false, alpha * 0.6f);
            return;
        }
        const float sp = std::min(7.0f, r.getWidth() / float(n));
        const float x0 = r.getCentreX() - sp * float(n - 1) / 2;
        for (int k = 0; k < n; ++k) {
            const float d = k == sel ? 2.4f : 1.4f;
            g.setColour(k == sel ? kGlow.withMultipliedAlpha(alpha) : kGlowDim.withMultipliedAlpha(alpha * 0.7f));
            g.fillEllipse(juce::Rectangle<float>(x0 + k * sp - d, r.getCentreY() - d, 2 * d, 2 * d) * s_);
        }
    };
    auto drawParam = [&](juce::RangedAudioParameter* p, juce::Rectangle<float> r, float alpha) {
        if (!p) return;
        const int n = p->getNumSteps();
        const bool discrete = n > 1 && n < 200 && dynamic_cast<juce::AudioParameterFloat*>(p) == nullptr;
        if (discrete) dots(r, n, juce::roundToInt(p->getValue() * float(n - 1)), alpha);
        else {
            const auto range = p->getNormalisableRange();
            bar(r, p->getValue(), range.start < 0 && range.end > 0, alpha);
        }
    };

    const bool touched = clock_ < touchedUntil_ && touchedKey_.isNotEmpty();
    if (touched) {
        auto* p = proc_.param(touchedKey_);
        if (p) {
            phosphor(g, p->getName(40).toUpperCase(), {kDispX + 6, kDispY + 3, kDispW - 12, 14}, 10.5f, juce::Justification::centredLeft);
            phosphor(g, valueText(proc_, p), {kDispX + 6, kDispY + 19, kDispW - 12, 26}, 22.0f, juce::Justification::centred);
            drawParam(p, {kDispX + 14, kDispY + kDispH - 15, kDispW - 28, 7}, 1.0f);
        }
        return;
    }
    static const char* menus[3] = {"VOICE", "MOD", "FX"};
    const auto& pg = currentPage();
    const int np = int(pages(proc_.menu).size());
    phosphor(g, juce::String(menus[proc_.menu]) + "  " + pg.title, {kDispX + 6, kDispY + 2, kDispW - 40, 14}, 10.0f, juce::Justification::centredLeft);
    phosphor(g, juce::String(proc_.page[proc_.menu] + 1) + "/" + juce::String(np), {kDispX + kDispW - 40, kDispY + 2, 34, 14}, 9.0f, juce::Justification::centredRight, 0.75f);
    g.setColour(kGlowFaint);
    g.fillRect(juce::Rectangle<float>(kDispX + 6, kDispY + 16.5f, kDispW - 12, 0.7f) * s_);
    const float cw = kDispW / 3;
    for (int col = 0; col < 3; ++col) {
        auto* p = pageParam(col);
        if (!p) continue;
        const float x = kDispX + col * cw;
        phosphor(g, pg.label[col], {x + 2, kDispY + 19, cw - 4, 11}, 8.2f, juce::Justification::centred, 0.7f);
        juce::String v = valueText(proc_, p);
        float size = 12.5f;
        while (size > 8.0f && juce::GlyphArrangement::getStringWidth(font(size), v) > (cw - 4) * s_) size -= 0.5f;
        phosphor(g, v, {x + 2, kDispY + 31, cw - 4, 16}, size, juce::Justification::centred);
        drawParam(p, {x + 7, kDispY + kDispH - 13, cw - 14, 5}, 1.0f);
    }
}

void TandemEditor::drawTypeWindow(juce::Graphics& g) {
    juce::Graphics::ScopedSaveState ss(g);
    const auto face = juce::Rectangle<float>(kTypeX, kTypeY, kTypeW, kTypeH);
    g.reduceClipRegion((face * s_).toNearestInt());
    g.setColour(juce::Colour(0x125dff9a));
    g.fillRect(face * s_);
    auto* p = proc_.param(typeKey());
    if (p) phosphor(g, p->getCurrentValueAsText(), face, 12.5f, juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Painting
void TandemEditor::drawSprite(juce::Graphics& g, const juce::Image& im, float cx, float cy, float wPt, float hPt, int frame, int frames) {
    if (!im.isValid()) return;
    const int fh = im.getHeight() / std::max(1, frames);
    g.drawImage(im, int(std::round((cx - wPt / 2) * s_)), int(std::round((cy - hPt / 2) * s_)), int(std::round(wPt * s_)), int(std::round(hPt * s_)),
                0, frame * fh, im.getWidth(), fh);
}

void TandemEditor::drawLit(juce::Graphics& g, int ci, const juce::Image& from) {
    const auto& c = kControls[ci];
    if (c.litGroup < 0 || !from.isValid()) return;
    const float* gr = kLitGroupRect[c.litGroup];
    const int S = kArtScale;
    g.drawImage(from, int(std::round(c.lx * s_)), int(std::round(c.ly * s_)), int(std::round(c.lw * s_)), int(std::round(c.lh * s_)),
                int((c.lx - gr[0]) * S), int((c.ly - gr[1]) * S), int(c.lw * S), int(c.lh * S));
}

void TandemEditor::paint(juce::Graphics& g) {
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    const auto clip = g.getClipBounds().toFloat() / s_;
    if (art_->panel.isValid()) g.drawImage(art_->panel, juce::Rectangle<float>(0, 0, W, H) * s_);
    else g.fillAll(juce::Colour(0xff8a8a86));

    for (int i = 0; i < kNumControls; ++i) {
        const auto& c = kControls[i];
        if (!boundsOf(i).expanded(2).intersects(clip)) continue;
        switch (c.type) {
            case Knob: drawKnob(g, i); break;
            case Toggle: {
                const int pos = togglePos(i);
                const int lever = c.nPos == 2 ? (pos == 0 ? 0 : 2) : pos;
                drawSprite(g, art_->toggle[lever], c.x, c.y, kToggleBox, kToggleBox);
                break;
            }
            case Fader: {
                const float cy = c.y + c.travel / 2 - c.travel * norm(i);
                drawSprite(g, art_->faderCap, c.x, cy, kFaderBoxW, kFaderBoxH);
                break;
            }
            case Button:
            case Lamp:
                if (lit(i)) drawLit(g, i, art_->lit[std::max(0, c.litGroup)]);
                if (pressed_ == i) {
                    g.setColour(juce::Colours::black.withAlpha(0.2f));
                    g.fillRoundedRectangle(juce::Rectangle<float>(c.x - c.w / 2 + 2, c.y - c.h / 2 + 2, c.w - 4, c.h - 4) * s_, 2 * s_);
                }
                break;
        }
        drawn_[size_t(i)] = param(i) ? norm(i) : (lit(i) ? 1.0f : 0.0f);
    }
    if (clip.intersects(juce::Rectangle<float>(kDispX, kDispY, kDispW, kDispH))) drawDisplay(g);
    if (clip.intersects(juce::Rectangle<float>(kTypeX, kTypeY, kTypeW, kTypeH))) drawTypeWindow(g);
    drawPresetName(g);
    if (clip.getBottom() > H) drawHelp(g);
    drawnMenu_ = proc_.menu * 100 + proc_.page[proc_.menu];
}

void TandemEditor::drawKnob(juce::Graphics& g, int ci) {
    const auto& c = kControls[ci];
    const float box = knobBox(c.kind);
    if (c.encoder) {
        const int n = std::max(1, art_->stripFrames[kSmall]);
        const int k = ci - indexOf("enc_a");
        const float ang = encAngle_[juce::jlimit(0, 3, k)];
        const int frame = ((int(std::lround(ang / (360.0f / float(n)))) % n) + n) % n;
        drawSprite(g, art_->strip[kSmall], c.x, c.y, box, box, frame, n);
        return;
    }
    if (c.kind == kSel) {
        const float ang = c.steps[selIndex(ci)];
        int best = 0;
        for (int k = 1; k < kNumSelAngles; ++k)
            if (std::abs(kSelAngles[k] - ang) < std::abs(kSelAngles[best] - ang)) best = k;
        drawSprite(g, art_->strip[kSel], c.x, c.y, box, box, best, art_->stripFrames[kSel]);
        return;
    }
    const int n = art_->stripFrames[c.kind];
    const int frame = juce::jlimit(0, std::max(0, n - 1), int(std::lround(norm(ci) * float(n - 1))));
    if (c.pointer >= 0) drawSprite(g, paintedFrame(ci, frame), c.x, c.y, box, box);
    else drawSprite(g, art_->strip[c.kind], c.x, c.y, box, box, frame, n);
}

// A knob whose pointer is painted a colour: the ivory index line, tinted.
const juce::Image& TandemEditor::paintedFrame(int ci, int frame) {
    auto& p = painted_[size_t(ci)];
    if (p.frame == frame && p.image.isValid()) return p.image;
    const auto& c = kControls[ci];
    juce::Image strip = art_->strip[c.kind];
    const int fw = strip.getWidth(), n = art_->stripFrames[c.kind];
    p.image = juce::Image(juce::Image::ARGB, std::max(1, fw), std::max(1, fw), true);
    p.frame = frame;
    if (!strip.isValid() || n < 2) return p.image;
    const float* col = kPointerColours[c.pointer];
    const float a = juce::degreesToRadians(-kSweep + 2 * kSweep * float(frame) / float(n - 1));
    const float ux = std::sin(a), uy = -std::cos(a);
    const float S = float(kArtScale), r = kKnobRadius[c.kind];
    const float inner = (kCapR * r + 0.3f) * S, outer = (r + 0.5f) * S, half = 1.15f * S;
    const float c0 = float(fw) / 2;
    const juce::Image::BitmapData src(strip, 0, frame * fw, fw, fw, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst(p.image, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < fw; ++y)
        for (int x = 0; x < fw; ++x) {
            const auto* sp = reinterpret_cast<const juce::PixelARGB*>(src.getPixelPointer(x, y));
            auto* dp = reinterpret_cast<juce::PixelARGB*>(dst.getPixelPointer(x, y));
            *dp = *sp;
            const float vx = float(x) + 0.5f - c0, vy = float(y) + 0.5f - c0;
            const float along = vx * ux + vy * uy, perp = std::abs(vx * uy - vy * ux);
            if (along < inner - 1 || along > outer + 1 || perp > half + 1) continue;
            const float geo = juce::jlimit(0.0f, 1.0f, half + 1 - perp) * juce::jlimit(0.0f, 1.0f, along - inner + 1) * juce::jlimit(0.0f, 1.0f, outer + 1 - along);
            const float al = float(sp->getAlpha());
            if (al < 1) continue;
            const float lo = float(std::min({sp->getRed(), sp->getGreen(), sp->getBlue()})) / al;
            const float t = geo * juce::jlimit(0.0f, 1.0f, (lo - 0.32f) / 0.3f);
            if (t <= 0) continue;
            auto mixc = [&](uint8_t v, float k) { return uint8_t(juce::jlimit(0.0f, al, float(v) * (1 - t + t * k * 1.1f)) + 0.5f); };
            dp->setARGB(sp->getAlpha(), mixc(sp->getRed(), col[0]), mixc(sp->getGreen(), col[1]), mixc(sp->getBlue(), col[2]));
        }
    return p.image;
}

void TandemEditor::drawPresetName(juce::Graphics& g) {
    if (!art_->marker) return;
    juce::Graphics::ScopedSaveState ss(g);
    const float cx = kPresetTapeX * s_, cy = kPresetTapeY * s_;
    g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(kPresetTapeAngle), cx, cy));
    const float maxW = (kPresetTapeW - 30) * s_;
    const auto r = juce::Rectangle<float>(cx - maxW / 2, cy - kPresetTapeH * 0.5f * s_, maxW, kPresetTapeH * s_);
    juce::String text = proc_.presetName();
    if (proc_.presetEdited()) text << " *";
    drawnPreset_ = text;
    float size = 25.0f;
    while (juce::GlyphArrangement::getStringWidth(juce::Font(juce::FontOptions(art_->marker).withHeight(size * s_)), text) > maxW && size > 12) size -= 1;
    g.setFont(juce::Font(juce::FontOptions(art_->marker).withHeight(size * s_)));
    g.setColour(juce::Colour(0x1a141a2a));
    g.drawText(text, r.translated(0.5f * s_, 0.6f * s_), juce::Justification::centred, false);
    g.setColour(juce::Colour(0xe0181a26));
    g.drawText(text, r, juce::Justification::centred, false);
}

void TandemEditor::drawHelp(juce::Graphics& g) {
    const auto strip = juce::Rectangle<float>(0, H, W, kHelpH) * s_;
    g.setColour(juce::Colour(0xff1c1d1f));
    g.fillRect(strip);
    juce::String name = "point at a control", range,
                 text = "Move the pointer over any knob, fader, switch or button and this strip says what it does. Drag knobs and faders (Shift for fine, "
                        "double-click for the default); click a lever on the side you want; click a rotary switch to step it round.";
    if (hoverId_ == "preset") {
        name = "preset";
        range = proc_.presetName();
        text = "Click the tape for the list of presets, and to save your own (they go to Music / Rumpus Room Records / Tandem / Presets).";
    } else if (hoverId_.startsWith("enc_") && hoverId_ != "enc_a") {
        const int col = hoverId_.getLastCharacter() - 'b';
        if (auto* p = pageParam(col)) {
            name = juce::String::charToString(juce::juce_wchar('B' + col)) + "  " + currentPage().label[col];
            range = p->getName(60).toLowerCase() + ". now " + valueText(proc_, p).toLowerCase();
            text = "Drag up or down (or scroll) to change it; click to step a list; double-click for the default. The column above it on the display shows the same.";
        } else {
            name = juce::String::charToString(juce::juce_wchar('B' + col));
            text = "Nothing on this page for this encoder.";
        }
    } else if (const auto* e = helpFor(hoverId_)) {
        name = juce::String::fromUTF8(e->name);
        range = juce::String::fromUTF8(e->range);
        text = juce::String::fromUTF8(e->text);
        juce::RangedAudioParameter* p = nullptr;
        if (hoverId_ == "f_type") p = proc_.param(typeKey());
        else { const int ci = indexOf(hoverId_.toRawUTF8()); p = param(ci); }
        if (p) range << ". now " << valueText(proc_, p).toLowerCase();
        else if (range.isNotEmpty()) range << ".";
    }
    const float pad = 24 * s_;
    g.setColour(juce::Colour(0xfff1ead2));
    g.setFont(font(17.0f));
    g.drawText(name.toUpperCase(), juce::Rectangle<float>(pad, strip.getY() + 10 * s_, 230 * s_, 20 * s_), juce::Justification::centredLeft, false);
    g.setColour(juce::Colour(0xffa39e8b));
    g.setFont(font(12.5f));
    g.drawFittedText(range, juce::Rectangle<float>(pad, strip.getY() + 32 * s_, 230 * s_, 34 * s_).toNearestInt(), juce::Justification::topLeft, 2);
    g.setColour(juce::Colour(0xffd8d2bd));
    g.setFont(font(15.0f));
    g.drawFittedText(text, juce::Rectangle<float>(pad + 246 * s_, strip.getY() + 8 * s_, W * s_ - 2 * pad - 246 * s_, 58 * s_).toNearestInt(), juce::Justification::centredLeft, 3);
}

// ---------------------------------------------------------------------------
void TandemEditor::timerCallback() {
    clock_ += 1.0 / kFrameRate;
    if (proc_.overdriven()) odUntil_ = clock_ + 0.15;
    const bool od = clock_ < odUntil_;
    if (od != odDrawn_) { odDrawn_ = od; const int ci = indexOf("odrive"); if (ci >= 0) repaintPts(boundsOf(ci)); }
    // The display: always (it is small), so values moved by the host show too.
    repaintPts({kDispX - 2, kDispY - 2, kDispW + 4, kDispH + 4});
    repaintPts({kTypeX - 2, kTypeY - 2, kTypeW + 4, kTypeH + 4});
    if (proc_.abSlot() != drawnAb_ || proc_.menu * 100 + proc_.page[proc_.menu] != drawnMenu_) { drawnAb_ = proc_.abSlot(); repaint(); return; }
    {
        juce::String text = proc_.presetName();
        if (proc_.presetEdited()) text << " *";
        if (text != drawnPreset_) repaintPts({kPresetTapeX - kPresetTapeW / 2 - 4, kPresetTapeY - kPresetTapeH / 2 - 4, kPresetTapeW + 8, kPresetTapeH + 8});
    }
    for (int i = 0; i < kNumControls; ++i) {
        if (kControls[i].encoder) continue;
        const float v = param(i) ? norm(i) : (lit(i) ? 1.0f : 0.0f);
        if (std::abs(v - drawn_[size_t(i)]) > 1e-6f) repaintPts(boundsOf(i));
    }
    if (hoverId_.isNotEmpty()) repaintPts({0, H, W, kHelpH});
}

// ---------------------------------------------------------------------------
// The mouse
void TandemEditor::mouseMove(const juce::MouseEvent& e) {
    const auto pt = e.position / s_;
    const int h = hit(pt);
    juce::String id;
    if (h == kDisplay) id = "disp";
    else if (h == kTypeWin) id = "f_type";
    else if (h == kTape) id = "preset";
    else if (h >= 0) {
        const auto& c = kControls[h];
        if (is(c, "preset_prev") || is(c, "preset_next")) id = "preset";
        else id = c.id;
    }
    if (id != hoverId_) { hoverId_ = id; repaintPts({0, H, W, kHelpH}); }
    const bool dragType = h >= 0 && h < kNumControls && (kControls[h].type == Knob || kControls[h].type == Fader) && !(kControls[h].kind == kSel && kControls[h].type == Knob);
    setMouseCursor(dragType || h == kDisplay ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void TandemEditor::mouseExit(const juce::MouseEvent&) {
    hoverId_ = {};
    repaintPts({0, H, W, kHelpH});
}

void TandemEditor::mouseDown(const juce::MouseEvent& e) {
    const auto pt = e.position / s_;
    const int h = hit(pt);
    drag_ = kNone; dragCol_ = -1; dragMoved_ = false; dragAccum_ = 0; dragSteps_ = 0; dragLast_ = e.position;
    if (h == kTape) { showPresetMenu(); return; }
    if (h == kTypeWin) { showTypeMenu(); return; }
    if (h == kDisplay) {
        drag_ = kDisplay;
        dragCol_ = displayColumnAt(pt);
        if (auto* p = pageParam(dragCol_)) { dragStartNorm_ = p->getValue(); p->beginChangeGesture(); }
        return;
    }
    if (h < 0 || h >= kNumControls) return;
    const auto& c = kControls[h];
    if (c.encoder) {
        drag_ = h;
        dragCol_ = h - indexOf("enc_b");          // -1 for A (pages)
        if (auto* p = pageParam(dragCol_)) { dragStartNorm_ = p->getValue(); p->beginChangeGesture(); }
        return;
    }
    if ((c.type == Knob && c.kind != kSel) || c.type == Fader) {
        if (auto* p = param(h)) {
            drag_ = h; dragStartNorm_ = p->getValue();
            p->beginChangeGesture();
        }
        return;
    }
    if (c.type == Button) { pressed_ = h; repaintPts(boundsOf(h)); return; }
    click(h, e);
}

void TandemEditor::mouseDrag(const juce::MouseEvent& e) {
    const float dx = e.position.x - dragLast_.x, dy = e.position.y - dragLast_.y;
    dragLast_ = e.position;
    const bool fine = e.mods.isShiftDown();
    auto columnDrag = [&](int col) {
        dragMoved_ = true;
        if (col < 0) {                      // A: pages
            dragSteps_ += -dy / (16.0f * s_);
            while (std::abs(dragSteps_) >= 1.0f) {
                const int dir = dragSteps_ > 0 ? 1 : -1;
                dragSteps_ -= float(dir);
                const int n = int(pages(proc_.menu).size());
                showMenu(proc_.menu, (proc_.page[proc_.menu] + dir + n) % n);
            }
            return;
        }
        auto* p = pageParam(col);
        if (!p) return;
        const int n = p->getNumSteps();
        if (n > 1 && n < 200) {
            dragSteps_ += -dy / (12.0f * s_);
            while (std::abs(dragSteps_) >= 1.0f) {
                const int dir = dragSteps_ > 0 ? 1 : -1;
                dragSteps_ -= float(dir);
                const int cur = juce::roundToInt(p->getValue() * float(n - 1));
                p->setValueNotifyingHost(float(juce::jlimit(0, n - 1, cur + dir)) / float(n - 1));
            }
        } else {
            dragAccum_ += (dx - dy) / (220.0f * s_) * (fine ? 0.15f : 1.0f);
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartNorm_ + dragAccum_));
        }
    };
    if (drag_ == kDisplay) { columnDrag(dragCol_); return; }
    if (drag_ < 0 || drag_ >= kNumControls) return;
    const auto& c = kControls[drag_];
    if (c.encoder) {
        const int k = drag_ - indexOf("enc_a");
        encAngle_[k] = std::fmod(encAngle_[k] - dy * 1.6f / s_ + 3600.0f, 360.0f);
        repaintPts(boundsOf(drag_));
        columnDrag(dragCol_);
        return;
    }
    if (auto* p = param(drag_)) {
        dragMoved_ = true;
        if (c.type == Fader) dragAccum_ += -dy / (c.travel * s_) * (fine ? 0.2f : 1.0f);
        else dragAccum_ += (dx - dy) / (220.0f * s_) * (fine ? 0.15f : 1.0f);
        p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartNorm_ + dragAccum_));
        touch(keyOf(drag_));
    }
}

void TandemEditor::mouseUp(const juce::MouseEvent& e) {
    const bool wasMoved = dragMoved_;
    if (drag_ == kDisplay || (drag_ >= 0 && drag_ < kNumControls && kControls[drag_].encoder)) {
        if (auto* p = pageParam(dragCol_)) p->endChangeGesture();
        if (!wasMoved) {                     // a click: A turns the page; B-D step a list or flip a switch
            if (dragCol_ < 0) {
                const int n = int(pages(proc_.menu).size());
                const int dir = e.mods.isPopupMenu() || e.mods.isAltDown() ? -1 : 1;
                showMenu(proc_.menu, (proc_.page[proc_.menu] + dir + n) % n);
            } else if (auto* p = pageParam(dragCol_)) {
                const int n = p->getNumSteps();
                if (n > 1 && n < 200) stepParam(p, e.mods.isPopupMenu() || e.mods.isAltDown() ? -1 : 1, true);
            }
        }
    } else if (drag_ >= 0 && drag_ < kNumControls) {
        if (auto* p = param(drag_)) p->endChangeGesture();
    }
    drag_ = kNone; dragCol_ = -1;
    if (pressed_ >= 0) {
        const int was = pressed_;
        pressed_ = kNone;
        repaintPts(boundsOf(was));
        if (hit(e.position / s_) == was) click(was, e);
    }
}

void TandemEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto pt = e.position / s_;
    const int h = hit(pt);
    juce::RangedAudioParameter* p = nullptr;
    if (h == kDisplay) p = pageParam(displayColumnAt(pt));
    else if (h >= 0 && h < kNumControls) {
        const auto& c = kControls[h];
        if (c.encoder) p = pageParam(h - indexOf("enc_b"));
        else if ((c.type == Knob && c.kind != kSel) || c.type == Fader) p = param(h);
    }
    if (p) { p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture(); }
}

void TandemEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    const auto pt = e.position / s_;
    const int h = hit(pt);
    const float d = wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f);
    if (d == 0.0f) return;
    const int dir = d > 0 ? 1 : -1;
    if (h == kDisplay) {
        const int col = displayColumnAt(pt);
        if (col < 0) { const int n = int(pages(proc_.menu).size()); showMenu(proc_.menu, (proc_.page[proc_.menu] + dir + n) % n); }
        else if (auto* p = pageParam(col)) {
            const int n = p->getNumSteps();
            if (n > 1 && n < 200) stepParam(p, dir, false);
            else p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + d * (e.mods.isShiftDown() ? 0.03f : 0.2f)));
        }
        return;
    }
    if (h < 0 || h >= kNumControls) return;
    const auto& c = kControls[h];
    if (c.encoder) {
        const int col = h - indexOf("enc_b");
        const int k = h - indexOf("enc_a");
        encAngle_[k] = std::fmod(encAngle_[k] + float(dir) * 15.0f + 3600.0f, 360.0f);
        repaintPts(boundsOf(h));
        if (col < 0) { const int n = int(pages(proc_.menu).size()); showMenu(proc_.menu, (proc_.page[proc_.menu] + dir + n) % n); }
        else if (auto* p = pageParam(col)) {
            const int n = p->getNumSteps();
            if (n > 1 && n < 200) stepParam(p, dir, false);
            else p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + d * (e.mods.isShiftDown() ? 0.03f : 0.2f)));
        }
        return;
    }
    if (c.type == Knob && c.kind == kSel) { stepParam(param(h), dir, false); touch(keyOf(h)); return; }
    if (auto* p = param(h)) {
        p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + d * (e.mods.isShiftDown() ? 0.03f : 0.2f)));
        touch(keyOf(h));
    }
}
