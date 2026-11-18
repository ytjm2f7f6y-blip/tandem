#include "PluginEditor.h"

#include "BinaryData.h"
#include "HelpText.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

using namespace layout;

namespace {

constexpr double kFrameRate = 30.0;
constexpr float kStepDragPx = 14.0f;      // drag this far (at 100 %) to move a stepped control one step

juce::Image load(const void* data, int size) { return data ? juce::ImageFileFormat::loadFrom(data, size_t(size)) : juce::Image(); }
juce::Image named(const juce::String& resource) {
    int size = 0;
    const char* data = BinaryData::getNamedResource(resource.toRawUTF8(), size);
    return load(data, size);
}

// A knob strip: colour from a JPEG (premultiplied on black), outline from one mask every frame shares.
juce::Image roundStrip(const juce::Image& rgb, const juce::Image& m) {
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

float knobBox(int kind) { return 2 * kKnobRadius[kind] + 2 * kShadowMargin; }

int indexOf(const char* id) {
    for (int i = 0; i < kNumControls; ++i)
        if (std::strcmp(kControls[i].id, id) == 0) return i;
    return -1;
}
bool is(const Control& c, const char* id) { return std::strcmp(c.id, id) == 0; }
bool startsWith(const Control& c, const char* pre) { return std::strncmp(c.id, pre, std::strlen(pre)) == 0; }

// Which parameter each control works.
struct Map { const char* control; const char* key; };
const Map kMap[] = {
    {"o1_wave", "osc1_wave"}, {"o2_wave", "osc2_wave"}, {"o1_tune", "osc1_tune"}, {"o1_fine", "osc1_fine"}, {"o2_tune", "osc2_tune"}, {"o2_fine", "osc2_fine"},
    {"o_sync", "sync"}, {"octave", "octave"}, {"m_osc", "mod_osc"}, {"m_wsrc", "wave_src"}, {"m_psrc", "pitch_src"}, {"m_detune", "detune"},
    {"m_wave", "wave_mod"}, {"m_pitch", "pitch_mod"}, {"x_osc1", "osc1_level"}, {"x_osc2", "osc2_level"}, {"x_ext", "ext_level"},
    {"f_cutoff", "cutoff"}, {"f_res", "resonance"}, {"f_kt", "keytrack"}, {"f_restype", "res_type"}, {"f_mod", "vcf_mod"}, {"f_msrc", "vcf_src"},
    {"f_delta", "delta"}, {"f_route", "route"}, {"l_rate", "lfo1_rate"}, {"l_delay", "lfo1_delay"}, {"l_wave", "lfo1_wave"},
    {"lfo2_rate", "lfo2_rate"}, {"lfo2_trig", "lfo2_trig"},
    {"e1_mode", "e1_mode"}, {"e1_h", "e1_hold"}, {"e1_a", "e1_attack"}, {"e1_d", "e1_decay"}, {"e1_s", "e1_sustain"}, {"e1_r", "e1_release"},
    {"e2_a", "e2_attack"}, {"e2_d", "e2_decay"}, {"e2_s", "e2_sustain"}, {"e2_r", "e2_release"}, {"vca_src", "vca_src"},
    {"e1_vel", "vel_vcf"}, {"e2_vel", "vel_vca"}, {"glide", "glide"}, {"vmode_1", "1:voice_mode"}, {"vmode_2", "2:voice_mode"}, {"vary", "vary"},
    {"fx_drive", "drv_amount"}, {"fx_chorus", "ch_on"}, {"fx_bal", "fx_balance"}, {"fx_time", "dly_time"}, {"fx_fb", "dly_fb"}, {"fx_size", "rv_size"},
    {"fx_freeze", "freeze"}, {"level1", "1:level"}, {"pan1", "1:pan"}, {"send1", "1:fx_send"}, {"volume", "volume"},
    {"level2", "2:level"}, {"pan2", "2:pan"}, {"send2", "2:fx_send"},
    {"play_mode", "perf_mode"}, {"arp_on", "arp_on"}, {"arp_hold", "arp_hold"}, {"arp_mode", "arp_mode"}, {"arp_range", "arp_range"},
    {"clock", "perf_tempo"}, {"arp_sync", "perf_host"},
    {"d_tune", "setup_tune"}, {"d_trans", "setup_transpose"}, {"d_vel", "setup_velocity"}, {"d_press", "setup_pressure"},
    {"d_ch1", "setup_ch1"}, {"d_ch2", "setup_ch2"}, {"d_age", "setup_age"}, {"d_pedal", "setup_pedal"},
};

// SHIFT + a control: the page it opens (menu, page title).
struct ShiftPage { const char* control; int menu; const char* title; };
const ShiftPage kShift[] = {
    {"f_cutoff", 1, "FILTER"}, {"f_res", 1, "FILTER"}, {"f_route", 1, "FILTER"}, {"f_restype", 1, "FILTER"}, {"f_kt", 1, "FILTER"},
    {"f_prev", 1, "FILTER"}, {"f_next", 1, "FILTER"}, {"f_delta", 1, "FILTER"}, {"f_mod", 1, "FILTER"}, {"f_msrc", 1, "FILTER"},
    {"o1_wave", 1, "OSC 1 WAVES 1-4"}, {"o2_wave", 1, "OSC 2 WAVES"}, {"m_detune", 1, "VOICE"}, {"glide", 1, "VOICE"}, {"octave", 1, "GLIDE"},
    {"l_wave", 1, "LFO 1"}, {"l_rate", 1, "LFO 1"}, {"l_delay", 1, "LFO 1"},
    {"lfo2_rate", 2, "LFO 2"}, {"lfo2_trig", 2, "LFO 2"}, {"vca_src", 2, "ENV 3"}, {"e1_vel", 2, "VELOCITY"}, {"e2_vel", 2, "VELOCITY"},
    {"e1_mode", 2, "ENV 3 MODE"},
    {"fx_drive", 4, "DRIVE"}, {"fx_chorus", 4, "CHORUS"}, {"fx_time", 4, "DELAY"}, {"fx_fb", 4, "DELAY"}, {"fx_size", 4, "REVERB"},
    {"fx_bal", 4, "DELAY / REVERB"}, {"fx_freeze", 4, "DELAY / REVERB"},
    {"level1", 0, "LAYER"}, {"pan1", 0, "LAYER"}, {"send1", 0, "LAYER"}, {"volume", 0, "LAYER"},
    {"level2", 0, "LAYER"}, {"pan2", 0, "LAYER"}, {"send2", 0, "LAYER"},
    {"vmode_1", 1, "VOICE"}, {"vmode_2", 1, "VOICE"}, {"dice", 0, "VARIATION"}, {"vary", 0, "VARIATION"}, {"assign", 2, "MATRIX 1"},
    {"play_mode", 0, "ZONES"}, {"x_ext", 0, "EXT IN FOLLOWER"},
    {"arp_on", 3, "MODE"}, {"arp_mode", 3, "MODE"}, {"arp_range", 3, "MODE"}, {"tap", 3, "MODE"},
    {"arp_hold", 3, "SWING"}, {"clock", 3, "CLOCK"}, {"arp_sync", 3, "CLOCK"},
};

// The controls that stand for a matrix destination: ASSIGN works on them, and they show their modulation.
// k: how far one unit of the matrix's output moves the control (in its travel), roughly as the engine applies it.
struct DestOf { const char* control; int dest; float k; };
const DestOf kDests[] = {
    {"f_cutoff", td::kMdCutoff, 0.58f}, {"f_res", td::kMdResonance, 0.9f}, {"f_delta", td::kMdDeltaCutoff, 0.5f}, {"f_mod", td::kMdEnv1Cutoff, 0.5f},
    {"m_wave", td::kMdOsc1Wave, 1.0f}, {"x_osc1", td::kMdOsc1Mix, 0.5f}, {"x_osc2", td::kMdOsc2Mix, 0.5f}, {"l_rate", td::kMdLfo1Rate, 0.38f},
    {"e1_a", td::kMdEnv1Attack, 0.3f}, {"e1_d", td::kMdEnv1Decay, 0.3f}, {"fx_time", td::kMdDelayTime, 0.5f}, {"level1", td::kMdVcaLevel, 0.5f},
    {"pan1", td::kMdPan, 0.5f}, {"o1_tune", td::kMdPitch, 0.86f}, {"o2_tune", td::kMdPitch, 0.86f},
};
const char* const kSlotFields[5] = {"_src", "_dst", "_amt", "_via", "_crv"};
const juce::Colour kModAmber(0xffffb347);

const help::Entry* helpFor(const juce::String& id) {
    for (int i = 0; i < help::kNumEntries; ++i)
        if (id == help::kEntries[i].id) return &help::kEntries[i];
    const auto family = id.upToLastOccurrenceOf("_", false, false);       // a lamp of a set: vm1_3 -> vm1
    if (family.isNotEmpty() && family != id)
        for (int i = 0; i < help::kNumEntries; ++i)
            if (family == help::kEntries[i].id) return &help::kEntries[i];
    return nullptr;
}

// The display's phosphor: amber.
const juce::Colour kGlow(0xffffc066), kGlowDim(0xa0f0a850), kGlowFaint(0x30ff9a30);

juce::String valueText(TandemProcessor& proc, juce::RangedAudioParameter* p);     // (below, with the display)

}  // namespace

const char* const TandemEditor::kMenuNames[TandemProcessor::kMenus] = {"PERF", "VOICE", "MOD", "ARP", "FX"};

// ---------------------------------------------------------------------------
const std::vector<DisplayPage>& TandemEditor::pages(int menu) {
    static const std::vector<DisplayPage> perf = {
        {"LAYER", {"level", "pan", "fx_send", "volume"}, {"LEVEL", "PAN", "FX SEND", "VOLUME"}},
        {"ZONES", {"perf_mode", "perf_split", nullptr, nullptr}, {"PLAY MODE", "SPLIT", nullptr, nullptr}},
        {"KEY RANGES", {"perf_lo1", "perf_hi1", "perf_lo2", "perf_hi2"}, {"L1 LOWEST", "L1 HIGHEST", "L2 LOWEST", "L2 HIGHEST"}},
        {"RNDMZR!?", {"perf_5up", "perf_5dn", "perf_8up", "perf_8dn"}, {"5TH UP", "5TH DOWN", "OCT UP", "OCT DOWN"}},
        {"RNDMZR LAYERS", {"perf_odds", nullptr, nullptr, nullptr}, {"LAYER 1", nullptr, nullptr, nullptr}},
        {"EXT IN FOLLOWER", {"perf_efatk", "perf_efrel", "perf_efthr", "perf_efgain"}, {"ATTACK", "RELEASE", "THRESHOLD", "GAIN"}},
        {"BENDER", {"bend_range", "bend_vcf", nullptr, nullptr}, {"PITCH", "CUTOFF", nullptr, nullptr}},
        {"VELOCITY", {"vel_vca", "vel_vcf", nullptr, nullptr}, {"VCA", "ENV 1", nullptr, nullptr}},
        {"VARIATION", {"vary", "vary_seed", nullptr, nullptr}, {"VARY", "ROLL", nullptr, nullptr}},
    };
    static const std::vector<DisplayPage> voice = {
        {"VOICE", {"voice_mode", "unison", "voices", "drift"}, {"MODE", "UNISON", "VOICES", "DRIFT"}},
        {"SPREAD", {"pan_spread", "glide", nullptr, nullptr}, {"PAN", "GLIDE", nullptr, nullptr}},
        {"FILTER", {"route", "<type>", "res_type", "keytrack"}, {"ROUTING", "TYPE", "RES TYPE", "KEYTRACK"}},
        {"OSC 1 WAVES 1-4", {"osc1_alt1", "osc1_alt2", "osc1_alt3", "osc1_alt4"}, {"SLOT 1", "SLOT 2", "SLOT 3", "SLOT 4"}},
        {"OSC 1 WAVES 5-6", {"osc1_alt5", "osc1_alt6", nullptr, nullptr}, {"SLOT 5", "SLOT 6", nullptr, nullptr}},
        {"OSC 2 WAVES", {"osc2_alt1", "osc2_alt2", "osc2_alt3", nullptr}, {"SLOT 1", "SLOT 2", "SLOT 3", nullptr}},
        {"LFO 1", {"lfo1_mode", "lfo1_sync", "lfo1_phase", nullptr}, {"MODE", "CLOCK", "PHASE", nullptr}},
        {"LFO 1 USER WAVES", {"lfo_alt1", "lfo_alt2", nullptr, nullptr}, {"USER 1", "USER 2", nullptr, nullptr}},
        {"GLIDE", {"glide", "octave", nullptr, nullptr}, {"RATE", "OCTAVE", nullptr, nullptr}},
    };
    static const std::vector<DisplayPage> mod = [] {
        std::vector<DisplayPage> v;
        // One page a slot, its four settings over the four encoders. (Static strings: the pages point at them.)
        static std::string names[td::kMatrixSlots][5];
        v.push_back({"MATRIX", {nullptr, nullptr, nullptr, nullptr}, {nullptr, nullptr, nullptr, nullptr}});    // the overview
        for (int i = 0; i < td::kMatrixSlots; ++i) {
            const std::string n = std::to_string(i + 1);
            names[i][0] = "MATRIX " + n;
            names[i][1] = "m" + n + "_src"; names[i][2] = "m" + n + "_dst"; names[i][3] = "m" + n + "_amt"; names[i][4] = "m" + n + "_via";
            v.push_back({names[i][0].c_str(), {names[i][1].c_str(), names[i][2].c_str(), names[i][3].c_str(), names[i][4].c_str()},
                         {"SOURCE", "DEST", "AMOUNT", "VIA"}});
        }
        v.push_back({"NOTE RANDOM", {"rnd_count", nullptr, nullptr, nullptr}, {"CYCLE", nullptr, nullptr, nullptr}});
        v.push_back({"LFO 2", {"lfo2_wave", "lfo2_trig", "lfo2_rate", "lfo2_delay"}, {"WAVE", "TRIGGER", "RATE", "DELAY"}});
        v.push_back({"LFO 2 DEPTH", {"lfo2_pitch", "lfo2_vcf", "lfo2_vca", "lfo2_ratemod"}, {"PITCH", "CUTOFF", "VCA", "RATE MOD"}});
        v.push_back({"ENV 3", {"e3_attack", "e3_decay", "e3_sustain", "e3_release"}, {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"}});
        v.push_back({"ENV 3 MODE", {"e3_mode", "e3_hold", nullptr, nullptr}, {"MODE", "HOLD", nullptr, nullptr}});
        v.push_back({"VELOCITY", {"vel_vca", "vel_vcf", nullptr, nullptr}, {"VCA", "ENV 1", nullptr, nullptr}});
        v.push_back({"BENDER", {"bend_range", "bend_vcf", nullptr, nullptr}, {"PITCH", "CUTOFF", nullptr, nullptr}});
        return v;
    }();
    static const std::vector<DisplayPage> arp = {
        {"MODE", {"arp_mode", "arp_range", "arp_div", "arp_on"}, {"MODE", "RANGE", "DIVISION", "ARP"}},
        {"SWING", {"arp_swing", "arp_gate", "arp_hold", nullptr}, {"SWING", "GATE", "HOLD", nullptr}},
        {"CLOCK", {"perf_tempo", "perf_host", nullptr, nullptr}, {"TEMPO", "HOST", nullptr, nullptr}},
    };
    static const std::vector<DisplayPage> fx = {
        {"DRIVE", {"drv_mode", "drv_tone", "drv_amount", "drv_mix"}, {"KIND", "TONE", "AMOUNT", "MIX"}},
        {"CHORUS", {"ch_on", "ch_rate", "ch_depth", "ch_mix"}, {"ON", "RATE", "DEPTH", "MIX"}},
        {"CHORUS DELAY", {"ch_delay", nullptr, nullptr, nullptr}, {"DELAY", nullptr, nullptr, nullptr}},
        {"EQUALIZER", {"eq_low", "eq_mid", "eq_high", "eq_q"}, {"LOW", "MID", "HIGH", "WIDTH"}},
        {"DELAY", {"dly_time", "dly_fb", "dly_damp", "dly_mode"}, {"TIME", "FEEDBACK", "DAMPING", "MODE"}},
        {"DELAY / REVERB", {"fx_balance", "freeze", "fx_send", nullptr}, {"BALANCE", "FREEZE", "SEND", nullptr}},
        {"REVERB", {"rv_size", "rv_damp", "rv_lowcut", "rv_predelay"}, {"SIZE", "DAMPING", "LOW CUT", "PRE-DLY"}},
        {"REVERB WIDTH", {"rv_width", nullptr, nullptr, nullptr}, {"WIDTH", nullptr, nullptr, nullptr}},
    };
    switch (menu) {
        case 0: return perf;
        case 1: return voice;
        case 3: return arp;
        case 4: return fx;
        default: return mod;
    }
}

TandemArt::TandemArt() {
    namespace B = BinaryData;
    panel = load(B::panel_jpg, B::panel_jpgSize);
    drawer = load(B::drawer_jpg, B::drawer_jpgSize);
    leds = load(B::leds_png, B::leds_pngSize);
    for (int k = 0; k < kNumFaderCaps; ++k) faderCap[k] = named(juce::String("fader_cap_") + kFaderCapNames[k] + "_png");
    for (int k = 0; k < kNumStrips; ++k) {
        const juce::String n = juce::String("knob_") + kStripNames[k];
        strip[k] = roundStrip(named(n + "_jpg"), named(n + "_mask_png"));
        stripFrames[k] = strip[k].isValid() ? strip[k].getHeight() / strip[k].getWidth() : 0;
    }
    legend = juce::Typeface::createSystemTypefaceFor(B::BarlowSemiCondensedSemiBold_ttf, B::BarlowSemiCondensedSemiBold_ttfSize);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// One invisible stand-in per control: it takes keyboard focus and tells VoiceOver what the control is.
// (Its workings are at the end of this file.)
class ControlProxy final : public juce::Component {
public:
    ControlProxy(TandemEditor& e, int ci);
    juce::String name() const;
    juce::RangedAudioParameter* param() const;
    juce::String valueString() const;
    std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;
    bool keyPressed(const juce::KeyPress& k) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;
    TandemEditor& ed;
    const int index;
};

TandemEditor::TandemEditor(TandemProcessor& p) : AudioProcessorEditor(p), proc_(p) {
    key_.assign(size_t(kNumControls), {});
    drawn_.assign(size_t(kNumControls), ~uint64_t(0));
    for (const auto& m : kMap) {
        const int ci = indexOf(m.control);
        if (ci >= 0) key_[size_t(ci)] = m.key;
    }
    for (int L = 0; L < 2; ++L)
        for (int i = 0; i < td::kMatrixSlots; ++i)
            for (int f = 0; f < 5; ++f) slot_[L][i][f] = proc_.param("m" + juce::String(i + 1) + kSlotFields[f], L);
    refreshDepths();
    setOpaque(true);
    setWantsKeyboardFocus(true);
    setFocusContainerType(FocusContainerType::keyboardFocusContainer);
    setTitle("Tandem");
    setDescription("Rumpus Room Records Tandem, a two-layer synthesizer. Tab moves through the controls; the arrow keys change the one in focus.");
    for (int i = 0; i < kNumControls; ++i) {           // the keyboard and VoiceOver: one invisible stand-in per control
        proxies_.push_back(std::make_unique<ControlProxy>(*this, i));
        addAndMakeVisible(*proxies_.back());
    }
    proxies_.push_back(std::make_unique<ControlProxy>(*this, kDisplay));
    addAndMakeVisible(*proxies_.back());
    setScale(proc_.uiScale);
    startTimerHz(int(kFrameRate));
}

TandemEditor::~TandemEditor() {
    stopTimer();
    proxies_.clear();
}

float TandemEditor::height() const { return H; }          // the drawer slides up over the panel: the window stays put

void TandemEditor::setScale(float s) {
    s_ = juce::jlimit(0.5f, 1.5f, s);
    proc_.uiScale = s_;
    setSize(int(std::round(W * s_)), int(std::round(height() * s_)));
    for (auto& p : proxies_) {                           // the stand-ins sit over their controls
        if (p->index == kDisplay) { p->setBounds((juce::Rectangle<float>(kDispX, kDispY, kDispW, kDispH) * s_).toNearestInt()); continue; }
        const auto& c = kControls[p->index];
        p->setBounds((boundsOf(p->index) * s_).toNearestInt());
        p->setVisible(!c.drawer || proc_.drawerOpen);
    }
    repaint();
}

void TandemEditor::setDrawer(bool open) {
    proc_.drawerOpen = open;
    setScale(s_);
}

void TandemEditor::showMenu(int menu, int page) {
    proc_.menu = juce::jlimit(0, TandemProcessor::kMenus - 1, menu);
    proc_.page[proc_.menu] = juce::jlimit(0, int(pages(proc_.menu).size()) - 1, page);
    touchedUntil_ = noticeUntil_ = 0;
    if (proc_.learningSplit() && !(proc_.menu == 0 && std::strcmp(currentPage().title, "ZONES") == 0)) proc_.learnSplit(false);
    repaint();
}

void TandemEditor::touch(const juce::String& key) {
    touchedKey_ = key;
    touchedUntil_ = clock_ + 1.6;
    noticeUntil_ = 0;
    repaintPts({kDispX - 2, kDispY - 2, kDispW + 4, kDispH + 4});
}

void TandemEditor::notice(const juce::String& text) {
    noticeText_ = text;
    noticeUntil_ = clock_ + 1.8;
    touchedUntil_ = 0;
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

bool TandemEditor::layerKeyLike(const juce::String& key) const {
    return key.isNotEmpty() && key != tp::kVolumeId && !tp::isPerfId(key) && !tp::isSetupId(key) && !(key.length() > 2 && key[1] == ':');
}

void TandemEditor::pressControl(const char* controlId) { if (const int ci = indexOf(controlId); ci >= 0) press(ci); }

// ---------------------------------------------------------------------------
// The matrix, from the panel
int TandemEditor::destOf(int ci) const {
    if (ci < 0 || ci >= kNumControls) return 0;
    for (const auto& d : kDests) if (is(kControls[ci], d.control)) return d.dest;
    return 0;
}

float TandemEditor::destScale(int ci) const {
    for (const auto& d : kDests) if (is(kControls[ci], d.control)) return d.k;
    return 0.5f;
}

int TandemEditor::shownSlot() const {
    const int s = proc_.page[2] - kMatrixFirst;
    return proc_.menu == 2 && s >= 0 && s < td::kMatrixSlots ? s : -1;
}
bool TandemEditor::matrixPage() const { return shownSlot() >= 0; }

juce::RangedAudioParameter* TandemEditor::slotParam(int slot, int field) const {
    return slot >= 0 && slot < td::kMatrixSlots && field >= 0 && field < 5 ? slot_[proc_.editLayer()][slot][field] : nullptr;
}

float TandemEditor::slotValue(int slot, int field) const {
    auto* p = slotParam(slot, field);
    return p ? p->convertFrom0to1(p->getValue()) : 0.0f;
}

void TandemEditor::refreshDepths() {
    for (auto& d : depth_) d = 0.0f;
    for (int i = 0; i < td::kMatrixSlots; ++i) {
        const int src = juce::roundToInt(slotValue(i, 0)), dst = juce::roundToInt(slotValue(i, 1));
        if (src != td::kMsNone && dst > 0 && dst < td::kNumModDests) depth_[dst] += std::abs(slotValue(i, 2));
    }
}

int TandemEditor::freeSlotFor(int dest) const {
    for (int i = 0; i < td::kMatrixSlots; ++i)          // one already pointed at it but not yet given a source
        if (juce::roundToInt(slotValue(i, 1)) == dest && juce::roundToInt(slotValue(i, 0)) == td::kMsNone) return i;
    for (int i = 0; i < td::kMatrixSlots; ++i)
        if (juce::roundToInt(slotValue(i, 0)) == td::kMsNone && juce::roundToInt(slotValue(i, 1)) == td::kMdNone) return i;
    return -1;
}

bool TandemEditor::startAssign(int ci) {
    const int dest = destOf(ci);
    if (dest == 0) {
        juce::String nm = kControls[ci].id;
        if (const auto* e = helpFor(nm)) nm = juce::String::fromUTF8(e->name);
        notice("NO MATRIX DESTINATION: " + nm.toUpperCase());
        return false;
    }
    const int slot = freeSlotFor(dest);
    if (slot < 0) { notice("ALL 12 MATRIX SLOTS ARE IN USE"); return false; }
    auto setOwn = [](juce::RangedAudioParameter* p, float v) { if (p) { p->beginChangeGesture(); TandemProcessor::set(p, v); p->endChangeGesture(); } };
    setOwn(slotParam(slot, 1), float(dest));
    if (juce::roundToInt(slotValue(slot, 0)) == td::kMsNone) setOwn(slotParam(slot, 0), float(td::kMsLfo1));    // a start: encoder 1 changes it
    assignSlot_ = slot;
    showMenu(2, slot + kMatrixFirst);
    return true;
}

bool TandemEditor::assignFor(const char* controlId, float amount) {
    const int ci = indexOf(controlId);
    if (ci < 0 || !startAssign(ci)) return false;
    if (auto* p = slotParam(assignSlot_, 2)) { p->beginChangeGesture(); TandemProcessor::set(p, amount); p->endChangeGesture(); }
    assign_ = false;
    refreshDepths();
    repaint();
    return true;
}

float TandemEditor::lampAlpha(int ci) const {
    const auto& c = kControls[ci];
    if (!startsWith(c, "v1_") && !startsWith(c, "v2_")) return 1.0f;
    const int L = c.id[1] - '1', v = std::atoi(c.id + 3) - 1;
    if (v >= proc_.voiceCount(L)) return 0.0f;
    const float m = proc_.voiceMeter(L, v);
    return m < 0.003f ? 0.0f : juce::jlimit(0.25f, 1.0f, 0.2f + 1.1f * std::sqrt(m));
}

void TandemEditor::rollDice() {
    auto* p = proc_.param("vary_seed");
    if (!p) return;
    const int was = juce::roundToInt(p->convertFrom0to1(p->getValue()));
    int roll = was;
    while (roll == was) roll = juce::Random::getSystemRandom().nextInt(10000);
    p->beginChangeGesture();
    TandemProcessor::set(p, float(roll));
    p->endChangeGesture();
    diceUntil_ = clock_ + 0.35;
    const juce::String num = "#" + juce::String(roll).paddedLeft('0', 4);
    notice(proc_.value("vary") < 0.005f ? "ROLL " + num + ": TURN UP VARY" : "DICE: ROLL " + num);
}

float TandemEditor::norm(int ci) const {
    if (auto* p = param(ci)) return p->getValue();
    return 0.5f;
}

int TandemEditor::choice(int ci) const {
    auto* p = param(ci);
    if (!p) return 0;
    return juce::roundToInt(p->convertFrom0to1(p->getValue()) - p->convertFrom0to1(0.0f));
}

bool TandemEditor::ledOn(int ci, int k) const {
    const auto& c = kControls[ci];
    if (c.type == Knob && c.kind == kRing) return k == (param(ci) ? choice(ci) : 0);
    if (c.type == Lamp) {
        if (is(c, "odrive")) return clock_ < odUntil_;
        if (startsWith(c, "vm1_") || startsWith(c, "vm2_")) return juce::roundToInt(proc_.value("voice_mode", c.id[2] - '1')) == c.id[4] - '0';
        if (startsWith(c, "v1_") || startsWith(c, "v2_")) return lampAlpha(ci) > 0.0f;
        return true;
    }
    if (is(c, "layer_1") || is(c, "layer_2")) return proc_.editLayer() == c.id[6] - '1';
    if (is(c, "arp_mode")) return juce::roundToInt(proc_.value("arp_mode")) != td::kArpUp;       // lit: not plain UP
    if (is(c, "arp_range")) return juce::roundToInt(proc_.value("arp_range")) > 0;               // lit: more than an octave
    if (is(c, "arp_on")) {                                // on: lit; playing: it beats with each step's half
        if (proc_.value("arp_on") < 0.5f) return false;
        return !proc_.arpPlaying(proc_.editLayer()) || std::fmod(proc_.clockBeat(), 1.0) < 0.5;
    }
    if (is(c, "tap")) return std::fmod(proc_.clockBeat(), 1.0) < 0.14;                           // the clock's lamp: every beat
    if (c.nLeds > 1 || (c.type == Key && param(ci) && c.nLeds > 0 && !param(ci)->isBoolean() && dynamic_cast<juce::AudioParameterChoice*>(param(ci)))) {
        if (is(c, "d_size")) {
            const float sizes[3] = {0.8f, 1.0f, 1.25f};
            return std::abs(s_ - sizes[juce::jlimit(0, 2, k)]) < 0.01f;
        }
        const int v = choice(ci);
        switch (c.lampMode) {
            case 1: return v > 0 && k == v - 1;
            case 2: return v == 1 || k == (v == 0 ? 0 : 1);
            default: return k == v;
        }
    }
    if (auto* p = param(ci)) return p->getValue() >= 0.5f;           // on / off keys
    if (startsWith(c, "pg_")) {
        static const char* ids[5] = {"pg_perf", "pg_voice", "pg_mod", "pg_arp", "pg_fx"};
        return is(c, ids[juce::jlimit(0, 4, proc_.menu)]);
    }
    if (startsWith(c, "patch_")) return proc_.presetIndex() == proc_.bank * 8 + (c.id[6] - '1');
    if (is(c, "compare")) return proc_.abSlot() == 1;
    if (is(c, "write")) return proc_.presetEdited() && std::fmod(clock_, 1.2) < 0.75;
    if (is(c, "assign")) return assign_;
    if (is(c, "dice")) return clock_ < diceUntil_;
    return false;
}

uint64_t TandemEditor::look(int ci) const {
    const auto& c = kControls[ci];
    uint64_t h = 0;
    if (c.type == Knob || c.type == Fader) h = uint64_t(std::lround(norm(ci) * 100000.0f));
    if (c.type == Lamp && (startsWith(c, "v1_") || startsWith(c, "v2_"))) h = uint64_t(std::lround(lampAlpha(ci) * 12.0f));
    if (const int d = destOf(ci)) h = h * 31 + uint64_t(std::lround(depth_[d] * 1000.0f));
    for (int k = 0; k < c.nLeds && k < 40; ++k) h = (h << 1) | (ledOn(ci, k) ? 1u : 0u);
    if (pressed_ == ci) h ^= 0x9e3779b97f4a7c15ull;
    if (c.shiftCap && shift_) h ^= 0x51ed27aull;
    return h;
}

juce::Rectangle<float> TandemEditor::boundsOf(int ci) const {
    const auto& c = kControls[ci];
    juce::Rectangle<float> r;
    switch (c.type) {
        case Knob: { const float b = knobBox(c.kind); r = {c.x - b / 2, c.y - b / 2, b, b}; break; }
        case Fader: r = {c.x - kFaderBoxW / 2 - 1, c.y - c.travel / 2 - kFaderBoxH / 2 - 1, kFaderBoxW + 2, c.travel + kFaderBoxH + 2}; break;
        case Key: r = {c.x - c.w / 2 - 3, c.y - c.h / 2 - 3, c.w + 6, c.h + 6}; break;
        case Lamp: r = {c.x - c.w / 2 - 6, c.y - c.h / 2 - 6, c.w + 12, c.h + 12}; break;
    }
    for (int k = 0; k < c.nLeds; ++k) {
        const auto& L = kLeds[c.led + k];
        r = r.getUnion({L.x - L.half, L.y - L.half, 2 * L.half, 2 * L.half});
    }
    return r.translated(0.0f, dy(ci));
}

juce::Rectangle<float> TandemEditor::patchWin(int layer) const {
    const auto* w = kPatchWin[juce::jlimit(0, 1, layer)];
    return {w[0], w[1], w[2], w[3]};
}

void TandemEditor::repaintPts(juce::Rectangle<float> r) {
    if (r.isEmpty()) return;
    const auto area = (r * s_).getSmallestIntegerContainer().expanded(2);
    if (repaintHook) repaintHook(area);
    repaint(area);
}

int TandemEditor::hit(juce::Point<float> pt0) const {
    const bool inDrawer = proc_.drawerOpen && pt0.y > H - kDrawerH;      // the drawer covers what is under it
    for (int i = 0; i < kNumControls; ++i) {
        const auto& c = kControls[i];
        if (c.drawer != inDrawer) continue;
        const auto pt = pt0.translated(0.0f, -dy(i));
        switch (c.type) {
            case Knob: if (pt.getDistanceFrom({c.x, c.y}) < kKnobRadius[c.kind] + (c.kind == kRing ? 9 : 4)) return i; break;
            case Fader: if (std::abs(pt.x - c.x) < 11 && std::abs(pt.y - c.y) < c.travel / 2 + kFaderCapH / 2 + 2) return i; break;
            case Key: {
                auto r = juce::Rectangle<float>(c.x - c.w / 2 - 2, c.y - c.h / 2 - 3, c.w + 4, c.h + 6);
                if (c.nLeds > 1) r = r.withTop(r.getY() - 16);           // the lamps over it are part of it
                if (r.contains(pt)) return i;
                break;
            }
            case Lamp: if (pt.getDistanceFrom({c.x, c.y}) < c.w / 2 + 4) return i; break;
        }
    }
    if (inDrawer) return kNone;
    if (juce::Rectangle<float>(kDispX, kDispY, kDispW, kDispH).contains(pt0)) return kDisplay;
    if (juce::Rectangle<float>(kTypeX, kTypeY, kTypeW, kTypeH).contains(pt0)) return kTypeWin;
    for (int L = 0; L < 2; ++L)
        if (patchWin(L).expanded(4.0f).contains(pt0)) return L == 0 ? kPatchWin1 : kPatchWin2;
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

// An edit the window makes, kept for UNDO: the sound before it and after.
struct EditScope {
    TandemProcessor& p;
    EditScope(TandemProcessor& proc, const juce::String& what) : p(proc) { p.beginEdit(what); }
    ~EditScope() { p.endEdit(); }
};

juce::String nameOf(juce::RangedAudioParameter* p) { return p ? p->getName(40).toUpperCase() : juce::String("EDIT"); }

void TandemEditor::loadSlot(int slot) {
    const auto list = proc_.presetList();
    if (slot < 0 || slot >= int(list.size())) {
        notice("PATCH " + juce::String::charToString(juce::juce_wchar('A' + slot / 8)) + juce::String(slot % 8 + 1) + " IS EMPTY");
        return;
    }
    {
        EditScope edit(proc_, "PATCH " + list[size_t(slot)].name.toUpperCase());
        proc_.loadPreset(slot);
    }
    touchedUntil_ = noticeUntil_ = 0;
    refreshDepths();
    repaint();
}

bool TandemEditor::doUndo() {
    const auto what = proc_.undoName();
    const bool ok = proc_.undo();
    notice(ok ? "UNDO: " + what : juce::String("NOTHING TO UNDO"));
    refreshDepths();
    repaint();
    return ok;
}

bool TandemEditor::doRedo() {
    const auto what = proc_.redoName();
    const bool ok = proc_.redo();
    notice(ok ? "REDO: " + what : juce::String("NOTHING TO REDO"));
    refreshDepths();
    repaint();
    return ok;
}

// SHIFT and a control: a special function where the panel prints one in red, else its page.
void TandemEditor::openPageFor(const char* controlId) {
    shift_ = false;
    juce::String id(controlId);
    if (id.startsWith("vm1_") || id.startsWith("v1_")) id = "vmode_1";      // the voice lamps: the voice page
    if (id.startsWith("vm2_") || id.startsWith("v2_")) id = "vmode_2";
    const int ci = indexOf(id.toRawUTF8());
    if (id == "layer_1") { swapLayers(); return; }
    if (id == "layer_2") { copyLayer(); return; }
    // A control of one layer's own (its mixer column, its voice mode): that layer, then its page.
    if (id.endsWithChar('1') && (id.startsWith("level") || id.startsWith("pan") || id.startsWith("send") || id == "vmode_1")) selectLayer(0);
    if (id.endsWithChar('2') && (id.startsWith("level") || id.startsWith("pan") || id.startsWith("send") || id == "vmode_2")) selectLayer(1);
    if (id == "play_mode") {                            // ZONES; and the next key played moves the split
        proc_.learnSplit(true);
        learnUntil_ = clock_ + 12.0;
    }
    if (id == "page_prev") { doUndo(); return; }
    if (id == "page_next") { doRedo(); return; }
    if (id == "compare") {
        { EditScope edit(proc_, "INIT"); proc_.loadPreset(0); }
        notice("INIT: A BLANK PATCH");
        refreshDepths();
        repaint();
        return;
    }
    if (id == "write") { askPresetName(2); repaint(); return; }
    if (id.startsWith("patch_")) {               // a bank; the same one again: the bank eight on (your own patches, past H)
        const int k = id.getLastCharacter() - '1', banks = (int(proc_.presetList().size()) + 7) / 8;
        proc_.bank = proc_.bank % 8 == k && proc_.bank + 8 < banks ? proc_.bank + 8 : k;
        notice("BANK " + juce::String::charToString(juce::juce_wchar('A' + proc_.bank)));
        repaint();
        return;
    }
    for (const auto& sp : kShift)
        if (id == juce::String(sp.control)) {
            const auto& ps = pages(sp.menu);
            for (int i = 0; i < int(ps.size()); ++i)
                if (std::strcmp(ps[size_t(i)].title, sp.title) == 0) { showMenu(sp.menu, i); return; }
        }
    if (ci >= 0 && keyOf(ci).isNotEmpty()) touch(keyOf(ci));
    else repaint();
}

void TandemEditor::press(int ci, bool back) {
    const auto& c = kControls[ci];
    if (is(c, "layer_1") || is(c, "layer_2")) { selectLayer(c.id[6] - '1'); return; }
    if (is(c, "tap")) { tap(juce::Time::getMillisecondCounterHiRes()); return; }
    if (is(c, "assign")) { assign_ = !assign_; shift_ = false; repaintPts(boundsOf(ci)); repaint(); return; }
    if (is(c, "dice")) { { EditScope edit(proc_, "DICE"); rollDice(); } repaintPts(boundsOf(ci)); return; }
    if (startsWith(c, "vm1_") || startsWith(c, "vm2_")) {     // a voice mode lamp: straight to that mode, on its layer
        const juce::String key = juce::String::charToString(juce::juce_wchar(c.id[2])) + ":voice_mode";
        if (auto* p = proc_.param(key)) {
            EditScope edit(proc_, nameOf(p));
            p->beginChangeGesture(); TandemProcessor::set(p, float(c.id[4] - '0')); p->endChangeGesture();
        }
        touch(key);
        repaint();
        return;
    }
    if (startsWith(c, "v1_") || startsWith(c, "v2_")) { touch(juce::String::charToString(juce::juce_wchar(c.id[1])) + ":voices"); return; }
    if (is(c, "shift")) { shift_ = !shift_; assign_ = false; repaintPts(boundsOf(ci)); repaintPts({kDispX, kDispY, kDispW, kDispH}); return; }
    if (startsWith(c, "pg_")) {
        static const char* ids[5] = {"pg_perf", "pg_voice", "pg_mod", "pg_arp", "pg_fx"};
        for (int m = 0; m < 5; ++m)
            if (is(c, ids[m])) {
                if (proc_.menu == m) showMenu(m, (proc_.page[m] + 1) % int(pages(m).size()));      // again: the next page
                else showMenu(m, proc_.page[m]);
            }
        return;
    }
    if (is(c, "page_prev") || is(c, "page_next")) {
        const int n = int(pages(proc_.menu).size());
        showMenu(proc_.menu, (proc_.page[proc_.menu] + (is(c, "page_prev") ? -1 : 1) + n) % n);
        return;
    }
    if (startsWith(c, "patch_")) { loadSlot(proc_.bank * 8 + (c.id[6] - '1')); return; }
    if (is(c, "compare")) {
        { EditScope edit(proc_, "COMPARE"); proc_.selectAB(1 - proc_.abSlot()); }
        notice(proc_.abSlot() == 1 ? "COMPARE: AS STORED" : "COMPARE: AS EDITED");
        refreshDepths();
        repaint();
        return;
    }
    if (is(c, "write")) { showWriteMenu(); return; }
    if (is(c, "undo")) { doUndo(); return; }
    if (is(c, "redo")) { doRedo(); return; }
    if (is(c, "f_prev") || is(c, "f_next")) {
        auto* p = proc_.param(typeKey());
        { EditScope edit(proc_, nameOf(p)); stepParam(p, is(c, "f_prev") ? -1 : 1, true); }
        touch(typeKey());
        repaint();
        return;
    }
    if (is(c, "setup")) { setDrawer(!proc_.drawerOpen); return; }
    if (is(c, "d_close")) { setDrawer(false); return; }
    if (is(c, "d_size")) {
        const float sizes[3] = {0.8f, 1.0f, 1.25f};
        int cur = 0;
        for (int i = 0; i < 3; ++i) if (std::abs(s_ - sizes[i]) < 0.01f) cur = i;
        setScale(sizes[(cur + 1) % 3]);
        return;
    }
    if (auto* p = param(ci)) {             // lamp keys, on / off keys and rings: step round
        if (c.type == Knob && c.kind != kRing) return;
        { EditScope edit(proc_, nameOf(p)); stepParam(p, back ? -1 : 1, true); }
        touch(keyOf(ci));
        if (c.type == Knob) showTagFor(c.id);
        repaintPts(boundsOf(ci));
    }
}

// ---------------------------------------------------------------------------
// The two layers
void TandemEditor::selectLayer(int layer) {
    layer = juce::jlimit(0, 1, layer);
    const bool changed = layer != proc_.editLayer();
    proc_.setEditLayer(layer);
    refreshDepths();
    if (changed || clock_ >= noticeUntil_) {
        const bool single = juce::roundToInt(proc_.value("perf_mode")) == td::kPlaySingle;
        notice("EDITING LAYER " + juce::String(layer + 1) + (single ? ": PLAYING IT ALONE" : juce::String()));
    }
    repaint();
}

void TandemEditor::swapLayers() {
    {
        EditScope edit(proc_, "SWAP LAYERS");
        for (const auto& d : tp::layerDefs()) {
            auto* a = proc_.param(d.key, 0);
            auto* b = proc_.param(d.key, 1);
            if (!a || !b) continue;
            const float va = a->getValue(), vb = b->getValue();
            if (va == vb) continue;
            a->beginChangeGesture(); a->setValueNotifyingHost(vb); a->endChangeGesture();
            b->beginChangeGesture(); b->setValueNotifyingHost(va); b->endChangeGesture();
        }
    }
    refreshDepths();
    notice("LAYERS SWAPPED");
    repaint();
}

void TandemEditor::copyLayer() {
    const int from = proc_.editLayer(), to = 1 - from;
    {
        EditScope edit(proc_, "COPY LAYER " + juce::String(from + 1) + " TO " + juce::String(to + 1));
        for (const auto& d : tp::layerDefs()) {
            auto* a = proc_.param(d.key, from);
            auto* b = proc_.param(d.key, to);
            if (!a || !b || a->getValue() == b->getValue()) continue;
            b->beginChangeGesture(); b->setValueNotifyingHost(a->getValue()); b->endChangeGesture();
        }
    }
    notice("LAYER " + juce::String(from + 1) + " COPIED TO LAYER " + juce::String(to + 1));
    repaint();
}

// TAP: two taps or more set the tempo (the last four intervals' average), and take the clock from the host.
void TandemEditor::tap(double ms) {
    if (!taps_.empty() && ms - taps_.back() > 2000.0) taps_.clear();
    taps_.push_back(ms);
    if (taps_.size() > 5) taps_.erase(taps_.begin());
    if (taps_.size() < 2) { notice("TAP: KEEP TAPPING"); return; }
    const double beat = (taps_.back() - taps_.front()) / double(taps_.size() - 1);
    const float bpm = juce::jlimit(40.0f, 240.0f, float(60000.0 / beat));
    {
        EditScope edit(proc_, "TAP TEMPO");
        if (auto* p = proc_.param("perf_tempo")) { p->beginChangeGesture(); TandemProcessor::set(p, bpm); p->endChangeGesture(); }
        if (auto* h = proc_.param("perf_host"); h && h->getValue() >= 0.5f) { h->beginChangeGesture(); h->setValueNotifyingHost(0.0f); h->endChangeGesture(); }
    }
    notice("TEMPO " + juce::String(bpm, bpm < 100 ? 1 : 0) + " BPM");
    repaintPts(boundsOf(indexOf("clock")));
    repaintPts(boundsOf(indexOf("arp_sync")));
}

// A lamp key's or ring's lamps can be clicked (or their legends): straight to that setting.
int TandemEditor::lampAt(int ci, juce::Point<float> pt) const {
    if (ci < 0 || ci >= kNumControls) return -1;
    pt = pt.translated(0.0f, -dy(ci));
    const auto& c = kControls[ci];
    const bool ring = c.type == Knob && c.kind == kRing;
    if (!ring && !(c.type == Key && c.nLeds > 1)) return -1;
    int best = -1;
    float bestD = 1e9f;
    for (int k = 0; k < c.nLeds; ++k) {
        const auto& L = kLeds[c.led + k];
        float d = pt.getDistanceFrom({L.x, L.y});
        if (ring) {                                     // or its number or icon, further out
            const float a = juce::degreesToRadians(c.steps[k]);
            d = std::min(d, pt.getDistanceFrom({c.x + std::sin(a) * 39.0f, c.y - std::cos(a) * 39.0f}));
            if (d < 7.0f && d < bestD) { best = k; bestD = d; }
        } else {
            const bool onLegend = std::abs(pt.x - L.x) < 10.0f && pt.y > L.y - 14.0f && pt.y < L.y + 5.0f;
            if ((d < 6.5f || onLegend) && d < bestD) { best = k; bestD = d; }
        }
    }
    return best;
}

void TandemEditor::selectLamp(int ci, int k) {
    if (ci < 0 || ci >= kNumControls || k < 0) return;
    const auto& c = kControls[ci];
    if (is(c, "d_size")) { const float sizes[3] = {0.8f, 1.0f, 1.25f}; setScale(sizes[juce::jlimit(0, 2, k)]); return; }
    auto* p = param(ci);
    if (!p) { press(ci); return; }
    const int v = choice(ci);
    int next = k;
    if (c.type == Key)
        switch (c.lampMode) {
            case 1: next = v == k + 1 ? 0 : k + 1; break;          // the first setting has no lamp: a lit lamp clicked goes back to it
            case 2: {                                               // OSC 1, both, OSC 2: each lamp in or out, never neither
                bool in[2] = {v != 2, v != 0};
                in[k & 1] = !in[k & 1];
                if (!in[0] && !in[1]) return;
                next = in[0] && in[1] ? 1 : (in[0] ? 0 : 2);
                break;
            }
            default: break;
        }
    {
        EditScope edit(proc_, nameOf(p));
        p->beginChangeGesture();
        TandemProcessor::set(p, p->convertFrom0to1(0.0f) + float(next));
        p->endChangeGesture();
    }
    touch(keyOf(ci));
    if (c.type == Knob) showTagFor(c.id);
    repaintPts(boundsOf(ci));
}

void TandemEditor::selectLampFor(const char* controlId, int lamp) { selectLamp(indexOf(controlId), lamp); }

// Right-click on a control: a small menu.
void TandemEditor::showControlMenu(int ci) {
    const auto& c = kControls[ci];
    juce::String title = c.id;
    if (const auto* e = helpFor(c.id)) title = juce::String::fromUTF8(e->name);
    juce::PopupMenu m;
    m.addSectionHeader(title);
    bool hasPage = false;
    for (const auto& sp : kShift) hasPage = hasPage || is(c, sp.control);
    const juce::String id(c.id);
    m.addItem(hasPage ? "Open its page  (SHIFT + it)" : "Show its value", [this, id] { openPageFor(id.toRawUTF8()); });
    auto* p = param(ci);
    if (p)
        m.addItem("Back to its default", [this, p] {
            EditScope edit(proc_, nameOf(p));
            p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture();
            refreshDepths(); repaint();
        });
    if (const int d = destOf(ci)) {
        juce::PopupMenu from;
        for (int src = 1; src < td::kNumModSources; ++src)
            from.addItem(tp::kModSources[src], [this, id, src] { modulateFrom(id.toRawUTF8(), src); });
        m.addSubMenu("Modulate from", from);
        int used = 0;
        for (int i = 0; i < td::kMatrixSlots; ++i)
            used += juce::roundToInt(slotValue(i, 1)) == d && juce::roundToInt(slotValue(i, 0)) != td::kMsNone;
        m.addItem("Clear its modulation" + (used ? "  (" + juce::String(used) + (used == 1 ? " slot)" : " slots)") : juce::String()), used > 0, false,
                  [this, id] { clearModulationOf(id.toRawUTF8()); });
    }
    if (is(c, "o1_wave") || is(c, "o2_wave") || is(c, "l_wave")) {          // the alternative slots: your own waves
        juce::PopupMenu waves;
        const char* pre = is(c, "o1_wave") ? "osc1_alt" : (is(c, "o2_wave") ? "osc2_alt" : "lfo_alt");
        const int n = is(c, "o1_wave") ? 6 : (is(c, "o2_wave") ? 3 : 2);
        for (int k = 1; k <= n; ++k)
            if (auto* sp = proc_.param(pre + juce::String(k)))
                waves.addItem("Slot " + juce::String(k) + ":  " + valueText(proc_, sp) + "...", [this, sp] { showWaveMenu(sp); });
        m.addSubMenu("Its alternative waves (yours too)", waves);
    }
    if (p && (c.type == Key || c.kind == kRing) && dynamic_cast<juce::AudioParameterChoice*>(p)) {
        auto* ch = dynamic_cast<juce::AudioParameterChoice*>(p);
        juce::PopupMenu set;
        for (int i = 0; i < ch->choices.size(); ++i)
            set.addItem(ch->choices[i], true, i == ch->getIndex(), [this, p, i, ci] {
                EditScope edit(proc_, nameOf(p));
                p->beginChangeGesture(); TandemProcessor::set(p, float(i)); p->endChangeGesture();
                touch(keyOf(ci)); repaint();
            });
        m.addSubMenu("Set to", set);
    }
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::modulateFrom(const char* controlId, int source) {
    const int ci = indexOf(controlId), d = destOf(ci);
    if (d == 0) return;
    const int slot = freeSlotFor(d);
    if (slot < 0) { notice("ALL 12 MATRIX SLOTS ARE IN USE"); return; }
    {
        EditScope edit(proc_, "MODULATE " + juce::String(controlId).toUpperCase());
        auto set = [](juce::RangedAudioParameter* q, float v) { if (q) { q->beginChangeGesture(); TandemProcessor::set(q, v); q->endChangeGesture(); } };
        set(slotParam(slot, 1), float(d));
        set(slotParam(slot, 0), float(source));
        if (std::abs(slotValue(slot, 2)) < 1e-4f) set(slotParam(slot, 2), 0.25f);
    }
    assignSlot_ = slot;
    refreshDepths();
    showMenu(2, slot + kMatrixFirst);
}

void TandemEditor::clearModulationOf(const char* controlId) {
    const int d = destOf(indexOf(controlId));
    if (d == 0) return;
    {
        EditScope edit(proc_, "CLEAR MODULATION");
        for (int i = 0; i < td::kMatrixSlots; ++i)
            if (juce::roundToInt(slotValue(i, 1)) == d)
                for (int f = 0; f < 5; ++f)
                    if (auto* q = slotParam(i, f)) { q->beginChangeGesture(); q->setValueNotifyingHost(q->getDefaultValue()); q->endChangeGesture(); }
    }
    refreshDepths();
    notice("MODULATION CLEARED");
    repaint();
}

// The display's title line: every page of this menu, to jump to.
void TandemEditor::showPageMenu() {
    juce::PopupMenu m;
    const auto& ps = pages(proc_.menu);
    m.addSectionHeader(juce::String(kMenuNames[proc_.menu]) + " pages");
    for (int i = 0; i < int(ps.size()); ++i) {
        juce::String t = ps[size_t(i)].title;
        if (proc_.menu == 2 && i >= kMatrixFirst && i < kMatrixFirst + td::kMatrixSlots) {
            const int slot = i - kMatrixFirst, src = juce::roundToInt(slotValue(slot, 0)), dst = juce::roundToInt(slotValue(slot, 1));
            if (src != td::kMsNone && dst != td::kMdNone) t << "   " << tp::kModSources[src] << " > " << tp::kModDests[dst];
        }
        m.addItem(t, true, i == proc_.page[proc_.menu], [this, i] { showMenu(proc_.menu, i); });
    }
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::showPresetMenu() {
    juce::PopupMenu m;
    const auto list = proc_.presetList();
    m.addSectionHeader("The whole sound");
    auto addCat = [&](const juce::String& title, auto keep) {
        juce::PopupMenu sub;
        bool here = false;
        for (int i = 0; i < int(list.size()); ++i)
            if (keep(list[size_t(i)])) {
                here = here || i == proc_.presetIndex();
                sub.addItem(list[size_t(i)].name, true, i == proc_.presetIndex(), [this, i] { loadSlot(i); });
            }
        if (sub.getNumItems() > 0) m.addSubMenu(title, sub, true, nullptr, here);
    };
    addCat("Two layers (performances)", [](const auto& e) { return e.factory && e.bothLayers; });
    addCat("Your performances", [](const auto& e) { return !e.factory && e.bothLayers; });
    m.addSeparator();
    m.addSectionHeader("A patch on layer 1 (layer 2 cleared)");
    for (const char* cat : {"Bass", "Lead", "Pad", "Keys", "Pluck", "Arp", "Effects", "Yours"})
        addCat(cat, [cat](const auto& e) { return !e.bothLayers && e.category == cat; });
    m.addItem("init  (both layers)", [this] { loadSlot(0); });
    m.addSeparator();
    m.addItem("Write...", [this] { showWriteMenu(); });
    juce::PopupMenu del;
    for (int i = 0; i < int(list.size()); ++i)
        if (!list[size_t(i)].factory) del.addItem(list[size_t(i)].name, [this, i] { proc_.deleteUserPreset(i); repaint(); });
    if (del.getNumItems() > 0) m.addSubMenu("Delete one of yours", del);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::showTypeMenu() {
    juce::PopupMenu m;
    auto* p = proc_.param(typeKey());
    auto* ch = dynamic_cast<juce::AudioParameterChoice*>(p);
    if (!ch) return;
    m.addSectionHeader(proc_.param("route")->getCurrentValueAsText() + " filter types");
    for (int i = 0; i < ch->choices.size(); ++i)
        m.addItem(ch->choices[i], true, i == ch->getIndex(), [this, p, i] {
            p->beginChangeGesture(); TandemProcessor::set(p, float(i)); p->endChangeGesture(); touch(typeKey()); repaint();
        });
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::showSizeMenu() {
    juce::PopupMenu m;
    m.addSectionHeader("Panel size");
    for (float s : {0.6f, 0.7f, 0.8f, 0.9f, 1.0f, 1.25f})
        m.addItem(juce::String(juce::roundToInt(s * 100)) + " %", true, std::abs(s_ - s) < 0.01f, [this, s] { setScale(s); });
    m.addSeparator();
    m.addItem("Tandem " JucePlugin_VersionString " (stage 2: both layers)", false, false, [] {});
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::askPresetName(int what) {
    const bool layer = what == 0 || what == 1;
    const juce::String title = layer ? "Write layer " + juce::String(what + 1) + " as a patch" : juce::String("Write a performance");
    const juce::String hint = layer ? "Name this patch (it can go on either layer):" : "Name these two layers and how they play together:";
    juce::String start = layer ? proc_.layerName(what) : proc_.presetName();
    start = start.upToFirstOccurrenceOf(" (", false, false);
    saveDialog_ = std::make_unique<juce::AlertWindow>(title, hint, juce::MessageBoxIconType::NoIcon, this);
    saveDialog_->addTextEditor("name", start);
    saveDialog_->addButton("Write", 1, juce::KeyPress(juce::KeyPress::returnKey));
    saveDialog_->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TandemEditor> safe(this);
    saveDialog_->enterModalState(true, juce::ModalCallbackFunction::create([safe, what, layer](int result) {
        if (safe == nullptr || safe->saveDialog_ == nullptr) return;
        const auto name = safe->saveDialog_->getTextEditorContents("name");
        if (result == 1) {
            const auto err = layer ? safe->proc_.saveLayerPatch(name, what) : safe->proc_.saveUserPreset(name);
            if (err.isNotEmpty()) juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Couldn't write it", err);
            else safe->notice(layer ? "WRITTEN: " + name.toUpperCase() + ", A PATCH" : "WRITTEN: " + name.toUpperCase());
        }
        juce::MessageManager::callAsync([safe] { if (safe != nullptr) { safe->saveDialog_.reset(); safe->repaint(); } });
    }), false);
}

// WRITE: one layer as a patch, or both as a performance.
void TandemEditor::showWriteMenu() {
    juce::PopupMenu m;
    m.addSectionHeader("Write");
    for (int L = 0; L < 2; ++L)
        m.addItem("Layer " + juce::String(L + 1) + " as a patch  (" + proc_.layerName(L).upToFirstOccurrenceOf(" (", false, false) + ")...",
                  [this, L] { askPresetName(L); });
    m.addItem("Both layers as a performance...", [this] { askPresetName(2); });
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

// ---------------------------------------------------------------------------
// Your own waves
void TandemEditor::showWaveMenu(juce::RangedAudioParameter* slot) {
    if (!slot) return;
    const int cur = juce::roundToInt(slot->convertFrom0to1(slot->getValue()));
    juce::PopupMenu m;
    m.addSectionHeader(slot->getName(40));
    m.addItem("Import a wave into this slot...", [this, slot] { importWaveInto(slot); });
    juce::PopupMenu yours;
    for (int k = 0; k < td::kUserWaves; ++k) {
        const auto n = proc_.userWaveName(k);
        yours.addItem("U" + juce::String(k + 1) + "  " + (n.isEmpty() ? juce::String("(empty)") : n), n.isNotEmpty(), cur == td::WaveBank::kNumAlt + k,
                      [this, slot, k] { EditScope edit(proc_, nameOf(slot)); slot->beginChangeGesture(); TandemProcessor::set(slot, float(td::WaveBank::kNumAlt + k)); slot->endChangeGesture(); repaint(); });
    }
    m.addSubMenu("Your waves", yours);
    juce::PopupMenu built;
    for (int i = 0; i < td::WaveBank::kNumAlt; ++i)
        built.addItem(juce::String(i + 1) + "  " + td::WaveBank::altName(i), true, cur == i,
                      [this, slot, i] { EditScope edit(proc_, nameOf(slot)); slot->beginChangeGesture(); TandemProcessor::set(slot, float(i)); slot->endChangeGesture(); repaint(); });
    m.addSubMenu("Tandem's waves", built);
    if (cur >= td::WaveBank::kNumAlt) {
        const int k = cur - td::WaveBank::kNumAlt;
        m.addItem("Forget your wave U" + juce::String(k + 1), [this, k] { proc_.clearUserWave(k); notice("U" + juce::String(k + 1) + " IS EMPTY NOW"); repaint(); });
    }
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::importWaveInto(juce::RangedAudioParameter* slot) {
    chooser_ = std::make_unique<juce::FileChooser>("A single-cycle wave for " + slot->getName(40), juce::File(), "*.wav;*.aif;*.aiff;*.flac");
    juce::Component::SafePointer<TandemEditor> safe(this);
    chooser_->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, slot](const juce::FileChooser& fc) {
        if (safe == nullptr) return;
        const auto file = fc.getResult();
        if (file == juce::File()) return;
        auto& ed = *safe;
        const int cur = juce::roundToInt(slot->convertFrom0to1(slot->getValue()));
        int k = cur >= td::WaveBank::kNumAlt ? cur - td::WaveBank::kNumAlt : ed.proc_.freeUserWave();
        if (k < 0) k = 0;                                   // all eight in use: the first goes
        EditScope edit(ed.proc_, "WAVE " + file.getFileNameWithoutExtension().toUpperCase());
        const auto err = ed.proc_.importWave(k, file);
        if (err.isNotEmpty()) { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Couldn't use that file", err); return; }
        slot->beginChangeGesture(); TandemProcessor::set(slot, float(td::WaveBank::kNumAlt + k)); slot->endChangeGesture();
        ed.notice("U" + juce::String(k + 1) + ": " + ed.proc_.userWaveName(k).toUpperCase());
        ed.repaint();
    });
}

// ---------------------------------------------------------------------------
// Each layer's patch window: the sound's name; the arrows at its ends step; the name opens the list.
void TandemEditor::loadLayer(int index, int layer, int part) {
    const auto list = proc_.presetList();
    if (index < 0 || index >= int(list.size())) return;
    {
        EditScope edit(proc_, "L" + juce::String(layer + 1) + " PATCH " + list[size_t(index)].name.toUpperCase());
        proc_.loadIntoLayer(index, layer, part);
    }
    if (proc_.editLayer() != layer) proc_.setEditLayer(layer);
    refreshDepths();
    touchedUntil_ = 0;
    notice("LAYER " + juce::String(layer + 1) + ": " + proc_.layerName(layer).toUpperCase());
    repaint();
}

void TandemEditor::showLayerMenu(int layer) {
    const auto list = proc_.presetList();
    juce::PopupMenu m;
    m.addSectionHeader("On layer " + juce::String(layer + 1));
    static const char* const cats[] = {"Init", "Bass", "Lead", "Pad", "Keys", "Pluck", "Arp", "Effects", "Yours"};
    for (const char* cat : cats) {
        juce::PopupMenu sub;
        bool here = false;
        for (int i = 0; i < int(list.size()); ++i) {
            const auto& e = list[size_t(i)];
            if (e.bothLayers || e.category != cat) continue;
            const bool on = proc_.layerIndex(layer) == i;
            here = here || on;
            sub.addItem(e.name, true, on, [this, i, layer] { loadLayer(i, layer, 0); });
        }
        if (sub.getNumItems() == 0) continue;
        if (juce::String(cat) == "Init") { m.addItem("init  (a plain saw, to start from)", [this, layer] { loadLayer(0, layer, 0); }); continue; }
        m.addSubMenu(cat, sub, true, nullptr, here);
    }
    juce::PopupMenu from;                                 // either layer of a two-layer sound
    for (int i = 0; i < int(list.size()); ++i) {
        const auto& e = list[size_t(i)];
        if (!e.bothLayers) continue;
        juce::PopupMenu which;
        for (int part = 0; part < 2; ++part)
            which.addItem("its layer " + juce::String(part + 1), [this, i, layer, part] { loadLayer(i, layer, part); });
        from.addSubMenu(e.name + (e.factory ? juce::String() : juce::String("  (yours)")), which);
    }
    if (from.getNumItems() > 0) m.addSubMenu("From a two-layer sound", from);
    m.addSeparator();
    const int other = 1 - layer;
    m.addItem("Write layer " + juce::String(layer + 1) + " as a patch...", [this, layer] { askPresetName(layer); });
    m.addItem("Copy it to layer " + juce::String(other + 1), [this, layer] { proc_.setEditLayer(layer); copyLayer(); });
    m.addItem("Swap the layers", [this] { swapLayers(); });
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::drawPatchWindow(juce::Graphics& g, int layer) {
    const auto face = patchWin(layer);
    juce::Graphics::ScopedSaveState ss(g);
    g.reduceClipRegion((face * s_).toNearestInt());
    const bool editing = proc_.editLayer() == layer;
    g.setColour(juce::Colour(editing ? 0x22ff9a30 : 0x10ff9a30));
    g.fillRect(face * s_);
    const float a = editing ? 1.0f : 0.72f;
    for (int sgn : {-1, 1}) {                              // the step arrows
        const float cx = face.getCentreX() + float(sgn) * (face.getWidth() / 2 - 8.0f), cy = face.getCentreY(), r = 3.4f;
        juce::Path p;
        p.addTriangle(cx + float(sgn) * r, cy, cx - float(sgn) * r * 0.7f, cy - r, cx - float(sgn) * r * 0.7f, cy + r);
        g.setColour(kGlowDim.withMultipliedAlpha(a));
        g.fillPath(p, juce::AffineTransform::scale(s_));
    }
    const auto list = proc_.presetList();
    const int idx = proc_.layerIndex(layer);
    juce::String code;
    if (idx >= 0 && idx < int(list.size()) && list[size_t(idx)].factory && idx < 64)
        code = juce::String::charToString(juce::juce_wchar('A' + idx / 8)) + juce::String(idx % 8 + 1);
    juce::String name = proc_.layerName(layer);
    if (patchEdited_[layer]) name << " *";
    const auto inner = face.reduced(18.0f, 0.0f);
    if (code.isNotEmpty()) phosphor(g, code, inner.withWidth(24.0f), 9.0f, juce::Justification::centredLeft, a * 0.55f);
    phosphor(g, name, inner.withTrimmedLeft(code.isNotEmpty() ? 22.0f : 0.0f).withTrimmedRight(code.isNotEmpty() ? 22.0f : 0.0f), 11.5f,
             juce::Justification::centred, a);
}

// ---------------------------------------------------------------------------
// The display
const DisplayPage& TandemEditor::currentPage() const {
    const auto& ps = pages(proc_.menu);
    return ps[size_t(juce::jlimit(0, int(ps.size()) - 1, proc_.page[proc_.menu]))];
}

juce::RangedAudioParameter* TandemEditor::pageParam(int col) const {
    const auto& pg = currentPage();
    if (col < 0 || col > 3 || pg.key[col] == nullptr) return nullptr;
    if (col == 2 && shift_ && matrixPage()) return slotParam(shownSlot(), 4);      // SHIFT: the slot's curve
    if (std::strcmp(pg.key[col], "<type>") == 0) return proc_.param(typeKey());
    return proc_.param(pg.key[col]);
}

int TandemEditor::displayColumnAt(juce::Point<float> pt) const {
    if (pt.y < kDispY + 22) return -1;
    int best = 0;
    for (int i = 1; i < 4; ++i)
        if (std::abs(pt.x - kEncX[i]) < std::abs(pt.x - kEncX[best])) best = i;
    return best;
}

juce::String TandemEditor::patchLabel() const {
    const int i = proc_.presetIndex();
    juce::String t = i >= 0 && i < 64 ? juce::String::charToString(juce::juce_wchar('A' + i / 8)) + juce::String(i % 8 + 1) + "  " + proc_.presetName()
                                      : proc_.presetName();
    if (proc_.presetEdited()) t << " *";
    return t;
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
// What the play mode does, in a line for the ZONES page.
juce::String modeLine(TandemProcessor& proc) {
    const int odds = juce::roundToInt(proc.value("perf_odds"));
    switch (juce::roundToInt(proc.value("perf_mode"))) {
        case td::kPlayDual: return "BOTH LAYERS, EVERY NOTE";
        case td::kPlaySplit: return "LAYER 2 BELOW " + tp::noteName(juce::roundToInt(proc.value("perf_split"))) + ", LAYER 1 FROM IT";
        case td::kPlayOneTwo: return "FIRST KEY LAYER 1, KEYS WITH IT LAYER 2";
        case td::kPlayRotate: return "LAYER 1, LAYER 2, IN TURN";
        case td::kPlayRandom: return "EITHER LAYER, AT RANDOM (L1 " + juce::String(odds) + "/6)";
        case td::kPlayChaos: return "AT RANDOM, WITH FIFTHS AND OCTAVES";
        case td::kPlayCascade: return "LAYER 1 PLAYS INTO LAYER 2'S FILTER";
        default: return "LAYER " + juce::String(proc.editLayer() + 1) + " ALONE";
    }
}

// A value as the display shows it: the LFO 1 rate depends on its wave and clock sync.
juce::String valueText(TandemProcessor& proc, juce::RangedAudioParameter* p) {
    if (!p) return {};
    if (p == proc.param("perf_tempo") && proc.value("perf_host") >= 0.5f) return "Host " + juce::String(juce::roundToInt(proc.clockTempo())) + " BPM";
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
    if (dynamic_cast<juce::AudioParameterBool*>(p) != nullptr) return p->getValue() >= 0.5f ? "On" : "Off";
    if (p->getParameterID().contains("_alt")) {           // a wave slot holding one of your own waves: its name
        const int k = juce::roundToInt(p->convertFrom0to1(p->getValue())) - td::WaveBank::kNumAlt;
        if (k >= 0) { const auto n = proc.userWaveName(k); return "U" + juce::String(k + 1) + " " + (n.isEmpty() ? juce::String("(empty)") : n); }
    }
    for (int i = 1; i <= td::kMatrixSlots; ++i)         // a slot on pitch: its amount in semitones
        if (p == proc.param("m" + juce::String(i) + "_amt") && juce::roundToInt(proc.value("m" + juce::String(i) + "_dst")) == td::kMdPitch) {
            const float st = p->convertFrom0to1(p->getValue()) * 12.0f;
            return (st > 0.005f ? "+" : "") + juce::String(st, std::abs(st) < 10.0f ? 2 : 1) + " st";
        }
    return p->getCurrentValueAsText();
}
}  // namespace

void TandemEditor::drawDisplay(juce::Graphics& g) {
    juce::Graphics::ScopedSaveState ss(g);
    const auto face = juce::Rectangle<float>(kDispX, kDispY, kDispW, kDispH);
    g.reduceClipRegion((face * s_).toNearestInt());
    juce::ColourGradient glow(juce::Colour(0x1cff9a30), face.getCentreX() * s_, face.getCentreY() * s_, juce::Colour(0x00ff9a30), face.getX() * s_, face.getY() * s_, true);
    g.setGradientFill(glow);
    g.fillRect(face * s_);

    auto bar = [&](juce::Rectangle<float> r, float v, bool bipolar, float alpha) {
        const int cells = 10;
        const float cw = r.getWidth() / cells;
        for (int k = 0; k < cells; ++k) {
            const float c0 = float(k) / cells, c1 = float(k + 1) / cells;
            const bool on = bipolar ? (v >= 0.5f ? (c1 > 0.5f && c0 < v) : (c0 < 0.5f && c1 > v)) : (c0 < v);
            g.setColour(on ? kGlow.withMultipliedAlpha(alpha) : kGlowFaint.withMultipliedAlpha(alpha));
            g.fillRect(juce::Rectangle<float>(r.getX() + k * cw + 0.6f, r.getY(), cw - 1.2f, r.getHeight()) * s_);
        }
    };
    auto dots = [&](juce::Rectangle<float> r, int n, int sel, float alpha) {
        if (n > 12) { bar(r, (float(sel) + 0.5f) / float(n), false, alpha * 0.6f); return; }
        const float sp = std::min(6.0f, r.getWidth() / float(n));
        const float x0 = r.getCentreX() - sp * float(n - 1) / 2;
        for (int k = 0; k < n; ++k) {
            const float d = k == sel ? 2.2f : 1.3f;
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
    // The glass is 98 points tall: a title line, a rule, then four columns (label, value, a bar).
    const float top = kDispY + 3, th = 16.0f, rule = kDispY + 22;
    const float labY = kDispY + 28, valY = kDispY + 42, barY = kDispY + kDispH - 15;

    if (clock_ < noticeUntil_) {
        phosphor(g, patchLabel(), {kDispX + 7, top, kDispW - 14, th}, 10.5f, juce::Justification::centredRight, 0.6f);
        phosphor(g, noticeText_, {kDispX + 8, kDispY + 30, kDispW - 16, 50}, 17.0f, juce::Justification::centred);
        return;
    }
    if (assign_ && !assigning_) {
        phosphor(g, "ASSIGN", {kDispX + 7, top, kDispW - 14, th}, 11.5f, juce::Justification::centredLeft);
        phosphor(g, patchLabel(), {kDispX + kDispW * 0.5f, top, kDispW * 0.5f - 7, th}, 10.5f, juce::Justification::centredRight, 0.8f);
        phosphor(g, "TURN THE KNOB TO MODULATE", {kDispX + 7, kDispY + 32, kDispW - 14, 26}, 15.0f, juce::Justification::centred);
        phosphor(g, "the turn sets the amount; the first column the source", {kDispX + 7, kDispY + 60, kDispW - 14, 20}, 11.0f, juce::Justification::centred, 0.65f);
        return;
    }
    if (shift_ && !matrixPage()) {
        phosphor(g, "SHIFT", {kDispX + 7, top, kDispW - 14, th}, 11.5f, juce::Justification::centredLeft);
        phosphor(g, "TOUCH A CONTROL FOR ITS PAGE", {kDispX + 7, kDispY + 32, kDispW - 14, 26}, 15.0f, juce::Justification::centred);
        phosphor(g, "red legends: their own jobs (SWAP, COPY, ZONES, INIT)", {kDispX + 7, kDispY + 60, kDispW - 14, 20}, 11.0f, juce::Justification::centred, 0.65f);
        return;
    }
    const bool touched = clock_ < touchedUntil_ && touchedKey_.isNotEmpty();
    if (touched) {
        if (auto* p = proc_.param(touchedKey_)) {
            juce::String name = p->getName(40).toUpperCase();
            if (p->getParameterID() == "l2_ext_level") name = "L2 LAYER 1 IN (CASCADE)";      // layer 2's EXT knob
            phosphor(g, name, {kDispX + 7, top, kDispW - 14, th}, 11.5f, juce::Justification::centredLeft);
            phosphor(g, valueText(proc_, p), {kDispX + 7, kDispY + 28, kDispW - 14, 42}, 30.0f, juce::Justification::centred);
            drawParam(p, {kDispX + 50, barY, kDispW - 100, 7}, 1.0f);
        }
        return;
    }
    const auto& pg = currentPage();
    const int np = int(pages(proc_.menu).size());
    bool ofLayer = false;                                // a page of the edited layer's: the title says which
    for (int col = 0; col < 4; ++col) ofLayer = ofLayer || (pg.key[col] && layerKeyLike(pg.key[col]));
    juce::String title = juce::String(shift_ ? "SHIFT" : kMenuNames[proc_.menu]) + "  " + pg.title;
    if (std::strcmp(pg.title, "LAYER") == 0) title << " " << (proc_.editLayer() + 1);
    else if (ofLayer && !(proc_.menu == 2 && proc_.page[2] == 0)) title = "L" + juce::String(proc_.editLayer() + 1) + " " + title;
    phosphor(g, title, {kDispX + 7, top, kDispW * 0.55f, th}, 11.5f, juce::Justification::centredLeft);
    if (np > 1)
        phosphor(g, juce::String(proc_.page[proc_.menu] + 1) + "/" + juce::String(np), {kDispX + 7 + kDispW * 0.55f - 38, top, 34, th}, 9.5f,
                 juce::Justification::centredRight, 0.6f);
    phosphor(g, patchLabel(), {kDispX + kDispW * 0.55f, top, kDispW * 0.45f - 7, th}, 10.5f, juce::Justification::centredRight, 0.8f);
    g.setColour(kGlowFaint);
    g.fillRect(juce::Rectangle<float>(kDispX + 7, rule, kDispW - 14, 0.8f) * s_);
    if (proc_.menu == 2 && proc_.page[2] == 0) { drawOverview(g); return; }
    if (proc_.menu == 0 && std::strcmp(pg.title, "ZONES") == 0)          // what the play mode does, in a line
        phosphor(g, proc_.learningSplit() ? juce::String("PLAY A KEY TO MOVE THE SPLIT") : modeLine(proc_), {kEncX[2] - 38, kDispY + 30, kEncX[3] - kEncX[2] + 76, 40},
                 11.0f, juce::Justification::centred, proc_.learningSplit() && std::fmod(clock_, 0.8) < 0.4 ? 0.45f : 0.85f);
    const float cw = (kEncX[1] - kEncX[0]) - 4.0f;
    for (int col = 0; col < 4; ++col) {
        auto* p = pageParam(col);
        if (!p) continue;
        const float x = kEncX[col] - cw / 2;
        juce::String label = pg.label[col];
        if (col == 2 && matrixPage()) {                    // the slot's curve: on SHIFT, or named beside the amount
            const int crv = juce::roundToInt(slotValue(shownSlot(), 4));
            if (shift_) label = "CURVE";
            else if (crv != td::kCurveLin) label = "AMT  " + tp::kModCurves[crv].toUpperCase();
        }
        phosphor(g, label, {x, labY, cw, 12}, 9.5f, juce::Justification::centred, 0.7f);
        juce::String v = valueText(proc_, p);
        float size = 16.0f;
        while (size > 9.0f && juce::GlyphArrangement::getStringWidth(font(size), v) > (cw - 2) * s_) size -= 0.5f;
        phosphor(g, v, {x, valY, cw, 24}, size, juce::Justification::centred);
        drawParam(p, {x + 8, barY, cw - 16, 6}, 1.0f);
    }
}

// The matrix at a glance: every slot in use, two columns of four; click one for its page.
namespace {
const char* const kSrcShort[td::kNumModSources] = {"", "OSC2", "LFO1", "LFO2", "ENV1", "ENV2", "VEL", "AT", "EXPR", "FOLLOW", "NOTE", "WHEEL", "RND"};
const char* const kDstShort[td::kNumModDests] = {"", "LFO1 RATE", "CUTOFF", "DELTA", "ENV1>CUT", "ENV1 DEPTH", "LFO1 DEPTH", "FM", "WAVE",
                                                 "OSC1 LVL", "OSC2 LVL", "RES", "ENV1 DEC", "DLY TIME", "VCA", "ENV1 ATK", "PITCH", "PAN"};
constexpr int kOverviewRows = 4;
}

void TandemEditor::drawOverview(juce::Graphics& g) {
    std::vector<int> used;
    for (int i = 0; i < td::kMatrixSlots; ++i)
        if (juce::roundToInt(slotValue(i, 0)) != td::kMsNone && juce::roundToInt(slotValue(i, 1)) != td::kMdNone) used.push_back(i);
    if (used.empty()) {
        phosphor(g, "NOTHING ROUTED YET", {kDispX + 7, kDispY + 32, kDispW - 14, 24}, 15.0f, juce::Justification::centred);
        phosphor(g, "ASSIGN, then turn a knob; or the next page", {kDispX + 7, kDispY + 60, kDispW - 14, 20}, 11.0f, juce::Justification::centred, 0.65f);
        return;
    }
    const float colW = (kDispW - 14) / 2, rowH = 17.0f, y0 = kDispY + 26;
    for (int n = 0; n < int(used.size()) && n < 2 * kOverviewRows; ++n) {
        const float x = kDispX + 7 + float(n / kOverviewRows) * colW, y = y0 + float(n % kOverviewRows) * rowH;
        if (n == 2 * kOverviewRows - 1 && int(used.size()) > 2 * kOverviewRows) {
            phosphor(g, "+ " + juce::String(int(used.size()) - n) + " MORE", {x + 16, y, colW - 20, rowH}, 10.5f, juce::Justification::centredLeft, 0.7f);
            break;
        }
        const int i = used[size_t(n)];
        const int src = juce::roundToInt(slotValue(i, 0)), dst = juce::roundToInt(slotValue(i, 1)), via = juce::roundToInt(slotValue(i, 3));
        juce::String route = juce::String(kSrcShort[juce::jlimit(0, td::kNumModSources - 1, src)]);
        if (via != td::kMsNone) route << "x" << kSrcShort[juce::jlimit(0, td::kNumModSources - 1, via)];
        route << ">" << kDstShort[juce::jlimit(0, td::kNumModDests - 1, dst)];
        const int amt = juce::roundToInt(slotValue(i, 2) * 63.0f);
        phosphor(g, juce::String(i + 1), {x, y, 13, rowH}, 9.5f, juce::Justification::centredRight, 0.55f);
        phosphor(g, route, {x + 18, y, colW - 52, rowH}, 10.5f, juce::Justification::centredLeft);
        phosphor(g, (amt > 0 ? "+" : "") + juce::String(amt), {x + colW - 36, y, 32, rowH}, 10.5f, juce::Justification::centredRight, 0.85f);
        const float bx = x + 18, bw = colW - 24, mid = bx + bw * 0.5f, a = slotValue(i, 2);     // the amount as a hairline from the middle
        g.setColour(kGlowFaint);
        g.fillRect(juce::Rectangle<float>(bx, y + rowH - 2.5f, bw, 1.0f) * s_);
        g.setColour(kGlowDim.withAlpha(ovSlot_ == i ? 1.0f : 0.75f));
        g.fillRect(juce::Rectangle<float>(a >= 0 ? mid : mid + a * bw * 0.5f, y + rowH - 3.0f, std::abs(a) * bw * 0.5f, 2.0f) * s_);
    }
}

int TandemEditor::overviewSlotAt(juce::Point<float> pt) const {
    if (!(proc_.menu == 2 && proc_.page[2] == 0) || pt.y < kDispY + 26) return -1;
    std::vector<int> used;
    for (int i = 0; i < td::kMatrixSlots; ++i)
        if (juce::roundToInt(slotValue(i, 0)) != td::kMsNone && juce::roundToInt(slotValue(i, 1)) != td::kMdNone) used.push_back(i);
    const float colW = (kDispW - 14) / 2;
    const int col = pt.x < kDispX + 7 + colW ? 0 : 1, row = int((pt.y - (kDispY + 26)) / 17.0f);
    const int n = col * kOverviewRows + row;
    return row >= 0 && row < kOverviewRows && n < int(used.size()) ? used[size_t(n)] : -1;
}

void TandemEditor::showSlotMenu(int slot) {
    const juce::String n(slot + 1);
    const int other = 1 - proc_.editLayer();
    juce::PopupMenu m;
    m.addSectionHeader("MATRIX " + n + " (LAYER " + juce::String(proc_.editLayer() + 1) + ")");
    m.addItem("Open its page", [this, slot] { showMenu(2, slot + kMatrixFirst); });
    m.addItem("Amount to zero", [this, slot] {
        EditScope edit(proc_, "MATRIX AMOUNT");
        if (auto* p = slotParam(slot, 2)) { p->beginChangeGesture(); TandemProcessor::set(p, 0.0f); p->endChangeGesture(); }
        refreshDepths(); repaint();
    });
    m.addItem("Turn it the other way", [this, slot] {
        EditScope edit(proc_, "MATRIX AMOUNT");
        if (auto* p = slotParam(slot, 2)) { p->beginChangeGesture(); TandemProcessor::set(p, -slotValue(slot, 2)); p->endChangeGesture(); }
        refreshDepths(); repaint();
    });
    m.addItem("Copy it to layer " + juce::String(other + 1), [this, slot] { copySlotToOtherLayer(slot); });
    m.addSeparator();
    m.addItem("Clear it", [this, slot] {
        EditScope edit(proc_, "CLEAR MATRIX " + juce::String(slot + 1));
        for (int f = 0; f < 5; ++f)
            if (auto* p = slotParam(slot, f)) { p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture(); }
        refreshDepths(); repaint();
    });
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withMousePosition());
}

void TandemEditor::copySlotToOtherLayer(int slot) {
    const int L = proc_.editLayer(), other = 1 - L;
    auto field = [&](int layer, int s, int f) { return slot_[layer][s][f]; };
    auto val = [&](int layer, int s, int f) { auto* p = field(layer, s, f); return p ? juce::roundToInt(p->convertFrom0to1(p->getValue())) : 0; };
    int to = -1;
    for (int i = 0; i < td::kMatrixSlots && to < 0; ++i)          // the same route there already: that one
        if (val(other, i, 0) == val(L, slot, 0) && val(other, i, 1) == val(L, slot, 1) && val(other, i, 3) == val(L, slot, 3)) to = i;
    for (int i = 0; i < td::kMatrixSlots && to < 0; ++i)          // else the first free one
        if (val(other, i, 0) == td::kMsNone || val(other, i, 1) == td::kMdNone) to = i;
    if (to < 0) { notice("LAYER " + juce::String(other + 1) + ": ALL 12 SLOTS ARE IN USE"); return; }
    {
        EditScope edit(proc_, "COPY MATRIX " + juce::String(slot + 1));
        for (int f = 0; f < 5; ++f)
            if (auto* src = field(L, slot, f))
                if (auto* dst = field(other, to, f)) { dst->beginChangeGesture(); dst->setValueNotifyingHost(src->getValue()); dst->endChangeGesture(); }
    }
    notice("COPIED TO LAYER " + juce::String(other + 1) + ", SLOT " + juce::String(to + 1));
    repaint();
}

void TandemEditor::drawTypeWindow(juce::Graphics& g) {
    juce::Graphics::ScopedSaveState ss(g);
    const auto face = juce::Rectangle<float>(kTypeX, kTypeY, kTypeW, kTypeH);
    g.reduceClipRegion((face * s_).toNearestInt());
    g.setColour(juce::Colour(0x14ff9a30));
    g.fillRect(face * s_);
    if (auto* p = proc_.param(typeKey())) phosphor(g, p->getCurrentValueAsText(), face, 12.5f, juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Painting
void TandemEditor::drawSprite(juce::Graphics& g, const juce::Image& im, float cx, float cy, float wPt, float hPt, int frame, int frames) {
    if (!im.isValid()) return;
    const int fh = im.getHeight() / std::max(1, frames);
    g.drawImage(im, int(std::round((cx - wPt / 2) * s_)), int(std::round((cy - hPt / 2) * s_)), int(std::round(wPt * s_)), int(std::round(hPt * s_)),
                0, frame * fh, im.getWidth(), fh);
}

void TandemEditor::drawLed(juce::Graphics& g, int li) {
    const auto& L = kLeds[li];
    if (!art_->leds.isValid()) return;
    const int s = int(std::ceil(2 * L.half * kArtScale));
    g.drawImage(art_->leds, int(std::round((L.x - L.half) * s_)), int(std::round((L.y - L.half) * s_)), int(std::round(2 * L.half * s_)),
                int(std::round(2 * L.half * s_)), L.ax, L.ay, s, s);
}

void TandemEditor::paint(juce::Graphics& g) {
    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    const auto clip = g.getClipBounds().toFloat() / s_;
    if (art_->panel.isValid()) g.drawImage(art_->panel, juce::Rectangle<float>(0, 0, W, H) * s_);
    else g.fillAll(juce::Colour(0xff9fb9cc));

    auto drawControl = [&](int i) {
        const auto& c = kControls[i];
        for (int k = 0; k < c.nLeds; ++k)
            if (ledOn(i, k)) {
                const float a = lampAlpha(i);
                if (a < 1.0f) g.setOpacity(a);
                drawLed(g, c.led + k);
                if (a < 1.0f) g.setOpacity(1.0f);
            }
        switch (c.type) {
            case Knob: drawKnob(g, i); break;
            case Fader: drawSprite(g, art_->faderCap[juce::jlimit(0, kNumFaderCaps - 1, c.strip)], c.x, c.y + c.travel / 2 - c.travel * norm(i), kFaderBoxW, kFaderBoxH); break;
            case Key:
                if (pressed_ == i || (c.shiftCap && shift_)) {
                    g.setColour(juce::Colours::black.withAlpha(0.2f));
                    g.fillRoundedRectangle(juce::Rectangle<float>(c.x - c.w / 2 + 1.2f, c.y - c.h / 2 + 1.2f, c.w - 2.4f, c.h - 2.4f) * s_, 2 * s_);
                }
                break;
            case Lamp: break;
        }
        if (destOf(i)) drawModMarks(g, i);
    };
    for (int i = 0; i < kNumControls; ++i)
        if (!kControls[i].drawer && boundsOf(i).expanded(2).intersects(clip)) drawControl(i);
    for (int L = 0; L < 2; ++L)
        if (clip.intersects(patchWin(L).expanded(3))) drawPatchWindow(g, L);
    if (proc_.drawerOpen && clip.getBottom() > H - kDrawerH) {      // the drawer, slid up over the bottom row
        juce::Graphics::ScopedSaveState ss(g);
        g.reduceClipRegion((juce::Rectangle<float>(0, H - kDrawerH, W, kDrawerH) * s_).toNearestInt());
        if (art_->drawer.isValid()) g.drawImage(art_->drawer, juce::Rectangle<float>(0, H - kDrawerH, W, kDrawerH) * s_);
        else { g.setColour(juce::Colour(0xff1d2638)); g.fillRect(juce::Rectangle<float>(0, H - kDrawerH, W, kDrawerH) * s_); }
        g.setColour(juce::Colours::black.withAlpha(0.35f));         // its shadow on the panel above
        g.fillRect(juce::Rectangle<float>(0, H - kDrawerH - 3, W, 3) * s_);
        g.addTransform(juce::AffineTransform::translation(0.0f, -kDrawerH * s_));
        for (int i = 0; i < kNumControls; ++i)
            if (kControls[i].drawer) drawControl(i);
    }
    if (clip.intersects(juce::Rectangle<float>(kDispX, kDispY, kDispW, kDispH))) drawDisplay(g);
    if (clip.intersects(juce::Rectangle<float>(kTypeX, kTypeY, kTypeW, kTypeH))) drawTypeWindow(g);
    drawValueTag(g);
    if (focusCi_ != kNone) {                            // the keyboard's place: an amber outline (cleared when focus leaves)
        juce::Rectangle<float> r;
        if (focusCi_ == kDisplay) r = {kDispX - 4, kDispY - 4, kDispW + 8, kDispH + 8};
        else if (focusCi_ >= 0 && focusCi_ < kNumControls) {
            const auto& c = kControls[focusCi_];
            switch (c.type) {
                case Knob: { const float rr = kKnobRadius[c.kind] + (c.kind == kRing ? 13.0f : 9.0f); r = {c.x - rr, c.y - rr, 2 * rr, 2 * rr}; break; }
                case Fader: r = {c.x - 13, c.y - c.travel / 2 - 14, 26, c.travel + 28}; break;
                case Key: r = {c.x - c.w / 2 - 5, c.y - c.h / 2 - 5, c.w + 10, c.h + 10}; break;
                case Lamp: r = {c.x - 7, c.y - 7, 14, 14}; break;
            }
            r = r.translated(0.0f, dy(focusCi_));
        }
        g.setColour(kModAmber.withAlpha(0.3f));
        g.drawRoundedRectangle(r * s_, 5 * s_, 4.0f * s_);
        g.setColour(kModAmber);
        g.drawRoundedRectangle(r * s_, 5 * s_, 1.4f * s_);
    }
}

// The value tag: a small piece of the display's glass by a control while you turn it.
juce::String TandemEditor::tagText(int ci) const {
    auto* p = param(ci);
    return p ? valueText(proc_, p) : juce::String();
}

juce::Rectangle<float> TandemEditor::tagRect(int ci, const juce::String& text) const {
    const auto& c = kControls[ci];
    const float w = juce::GlyphArrangement::getStringWidth(font(10.5f), text) / s_ + 14.0f, h = 16.0f;
    juce::Point<float> at;
    if (c.type == Fader) at = {c.x + 14.0f + w / 2, c.y + c.travel / 2 - c.travel * norm(ci)};
    else if (c.type == Knob && c.kind == kRing) at = {c.x, c.y - 52.0f};
    else at = {c.x, c.y - kKnobRadius[c.kind] - 18.0f};
    auto r = juce::Rectangle<float>(w, h).withCentre(at.translated(0.0f, dy(ci)));
    return r.constrainedWithin({2, 2, W - 4, height() - 4});
}

void TandemEditor::showTagFor(const char* controlId) {
    const int ci = indexOf(controlId);
    if (ci < 0) return;
    if (tagCi_ != kNone) repaintPts(tagShown_.expanded(3));
    tagCi_ = ci;
    tagUntil_ = clock_ + 0.9;
    repaintPts(tagRect(ci, tagText(ci)).expanded(3));
}

void TandemEditor::drawValueTag(juce::Graphics& g) {
    if (tagCi_ == kNone || clock_ > tagUntil_) return;
    const auto text = tagText(tagCi_);
    if (text.isEmpty()) return;
    const auto r = tagRect(tagCi_, text);
    tagShown_ = r;
    g.setColour(juce::Colour(0xf01a1d17));
    g.fillRoundedRectangle(r * s_, 3.0f * s_);
    g.setColour(kModAmber.withAlpha(0.45f));
    g.drawRoundedRectangle(r * s_, 3.0f * s_, 0.8f * s_);
    phosphor(g, text, r, 10.5f, juce::Justification::centred);
}

void TandemEditor::drawKnob(juce::Graphics& g, int ci) {
    const auto& c = kControls[ci];
    const float box = knobBox(c.kind);
    const auto& im = art_->strip[c.strip];
    const int n = std::max(1, art_->stripFrames[c.strip]);
    int frame = 0;
    if (c.encoder) {
        const float ang = encAngle_[juce::jlimit(0, 3, ci - indexOf("enc_0"))];
        frame = ((int(std::lround(ang / (360.0f / float(n)))) % n) + n) % n;
    } else if (c.kind == kRing) {
        frame = c.stepFrame[juce::jlimit(0, std::max(0, c.nSteps - 1), param(ci) ? choice(ci) : 0)];
    } else {
        frame = juce::jlimit(0, n - 1, int(std::lround(norm(ci) * float(n - 1))));
    }
    drawSprite(g, im, c.x, c.y, box, box, frame, n);
}

// A modulated control: a pinpoint amber dot in the gap at the foot of its scale, always; and while it is
// touched, or ASSIGN is lit, an arc over its scale for the range the matrix can move it, with a tick
// where the newest voice has it now.
void TandemEditor::drawModMarks(juce::Graphics& g, int ci) {
    const int d = destOf(ci);
    const float depth = d ? depth_[d] : 0.0f;
    if (depth <= 0.0f) return;
    const auto& c = kControls[ci];
    const bool arc = assign_ || drag_ == ci || (clock_ < touchedUntil_ && touchedKey_ == keyOf(ci));
    auto dot = [&](float x, float y, float r, float alpha) {
        g.setColour(kModAmber.withAlpha(0.28f * alpha));
        g.fillEllipse(juce::Rectangle<float>(x - 2.3f * r, y - 2.3f * r, 4.6f * r, 4.6f * r) * s_);
        g.setColour(kModAmber.withAlpha(alpha));
        g.fillEllipse(juce::Rectangle<float>(x - r, y - r, 2 * r, 2 * r) * s_);
    };
    const float v = norm(ci), k = destScale(ci);
    const float lo = juce::jlimit(0.0f, 1.0f, v - depth * k), hi = juce::jlimit(0.0f, 1.0f, v + depth * k);
    const float live = juce::jlimit(0.0f, 1.0f, v + proc_.modNow(d) * k);
    if (c.type == Knob) {
        const float r = kKnobRadius[c.kind];
        dot(c.x, c.y + r + 5.8f, 1.9f, 1.0f);
        if (!arc) return;
        auto ang = [](float n) { return juce::degreesToRadians(-kSweep + 2.0f * kSweep * n); };
        const float ra = r + 3.6f;
        juce::Path p;
        p.addCentredArc(c.x * s_, c.y * s_, ra * s_, ra * s_, 0.0f, ang(lo), ang(std::max(hi, lo + 0.004f)), true);
        g.setColour(kModAmber.withAlpha(0.25f));
        g.strokePath(p, juce::PathStrokeType(4.2f * s_, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(kModAmber.withAlpha(0.9f));
        g.strokePath(p, juce::PathStrokeType(1.5f * s_, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float a = ang(live);
        dot(c.x + std::sin(a) * ra, c.y - std::cos(a) * ra, 1.7f, 1.0f);
    } else if (c.type == Fader) {
        const float bot = c.y + c.travel / 2;
        dot(c.x, bot + 12.5f, 1.9f, 1.0f);
        if (!arc) return;
        const float x = c.x + 9.5f;
        auto yOf = [&](float n) { return bot - c.travel * n; };
        g.setColour(kModAmber.withAlpha(0.25f));
        g.fillRoundedRectangle(juce::Rectangle<float>(x - 2.1f, yOf(hi) - 1.0f, 4.2f, yOf(lo) - yOf(hi) + 2.0f) * s_, 2.0f * s_);
        g.setColour(kModAmber.withAlpha(0.9f));
        g.fillRoundedRectangle(juce::Rectangle<float>(x - 0.75f, yOf(hi), 1.5f, std::max(0.8f, yOf(lo) - yOf(hi))) * s_, 0.75f * s_);
        dot(x, yOf(live), 1.7f, 1.0f);
    }
}

// ---------------------------------------------------------------------------
void TandemEditor::timerCallback() {
    clock_ += 1.0 / kFrameRate;
    if (const int n = proc_.learnedSplit(); n >= 0) {           // ZONES: a key played sets the split
        if (auto* p = proc_.param("perf_split")) {
            EditScope edit(proc_, "SPLIT POINT");
            p->beginChangeGesture(); TandemProcessor::set(p, float(n)); p->endChangeGesture();
        }
        learnUntil_ = 0;
        notice("SPLIT AT " + tp::noteName(n) + ": LAYER 1 FROM THERE UP");
    } else if (proc_.learningSplit() && clock_ > learnUntil_) proc_.learnSplit(false);
    if (proc_.overdriven()) odUntil_ = clock_ + 0.15;
    refreshDepths();
    for (int i = 0; i < kNumControls; ++i) {           // arcs follow the voices: redraw them every frame
        if (const int d = destOf(i); d && depth_[d] > 0.0f)
            if (assign_ || drag_ == i || (clock_ < touchedUntil_ + 0.1 && touchedKey_ == keyOf(i))) repaintPts(boundsOf(i));
    }
    if (tagCi_ != kNone) {                               // the value tag: follow it, then wipe it
        repaintPts(tagShown_.expanded(3));
        if (clock_ > tagUntil_) tagCi_ = kNone;
        else if (auto* p = param(tagCi_)) repaintPts(tagRect(tagCi_, tagText(tagCi_)).expanded(3));
    }
    repaintPts({kDispX - 2, kDispY - 2, kDispW + 4, kDispH + 4});
    repaintPts({kTypeX - 2, kTypeY - 2, kTypeW + 4, kTypeH + 4});
    {   // the patch windows: their names, whether they have been changed, which layer is being edited
        juce::String now = juce::String(proc_.editLayer());
        for (int L = 0; L < 2; ++L) {
            patchEdited_[L] = proc_.layerEdited(L);
            now << "|" << proc_.layerName(L) << (patchEdited_[L] ? "*" : "") << proc_.layerIndex(L);
        }
        if (now != patchSeen_) { patchSeen_ = now; for (int L = 0; L < 2; ++L) repaintPts(patchWin(L).expanded(3)); }
    }
    // A control that has changed since it was last asked to be repainted: all of it, again. (Painting
    // cannot keep this record: a paint may cover only a sliver of a control, under a value tag, say.)
    for (int i = 0; i < kNumControls; ++i) {
        if (kControls[i].encoder || (kControls[i].drawer && !proc_.drawerOpen)) continue;
        const uint64_t now = look(i);
        if (now != drawn_[size_t(i)]) { repaintPts(boundsOf(i)); drawn_[size_t(i)] = now; }
    }
}

// ---------------------------------------------------------------------------
// The mouse
juce::String TandemEditor::getTooltip() {
    if (hoverId_.isEmpty() || drag_ != kNone) return {};
    if (const auto* e = helpFor(hoverId_)) return juce::String::fromUTF8(e->name) + ": " + juce::String::fromUTF8(e->text);
    return {};
}

void TandemEditor::mouseMove(const juce::MouseEvent& e) {
    const auto pt = e.position / s_;
    const int h = hit(pt);
    juce::String id;
    if (h == kDisplay) id = "disp";
    else if (h == kTypeWin) id = "f_type";
    else if (h == kPatchWin1) id = "pw1";
    else if (h == kPatchWin2) id = "pw2";
    else if (h >= 0) id = kControls[h].id;
    hoverId_ = id;
    const bool drags = h >= 0 && h < kNumControls && (kControls[h].type == Knob || kControls[h].type == Fader);
    setMouseCursor(drags || h == kDisplay ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
}

void TandemEditor::mouseExit(const juce::MouseEvent&) { hoverId_ = {}; }

void TandemEditor::mouseDown(const juce::MouseEvent& e) {
    const auto pt = e.position / s_;
    const int h = hit(pt);
    drag_ = kNone; dragCol_ = -1; dragMoved_ = false; dragAccum_ = 0; dragSteps_ = 0; dragLast_ = e.position; pressLamp_ = -1;
    // ASSIGN, then a knob or fader that stands for a destination: a slot for it, and the drag sets the amount.
    if (assign_ && !e.mods.isPopupMenu() && h >= 0 && h < kNumControls) {
        const auto& c = kControls[h];
        if (((c.type == Knob && c.kind != kRing && !c.encoder) || c.type == Fader)) {
            proc_.beginEdit("ASSIGN");
            if (startAssign(h))
                if (auto* p = slotParam(assignSlot_, 2)) {
                    drag_ = h; assigning_ = true; dragStartNorm_ = p->getValue(); p->beginChangeGesture();
                }
            if (!assigning_) proc_.endEdit();
            return;
        }
    }
    if (h >= 0 && h < kNumControls && e.mods.isPopupMenu()) {        // right-click: the control's menu
        const auto& c = kControls[h];
        if (startsWith(c, "patch_")) { showPresetMenu(); return; }
        if (is(c, "d_size")) { showSizeMenu(); return; }
        if (c.encoder) { if (auto* p = pageParam(h - indexOf("enc_0"))) { touch(p->getParameterID().fromFirstOccurrenceOf("_", false, false)); } return; }
        showControlMenu(h);
        return;
    }
    const bool shiftEncoder = shift_ && matrixPage() && h >= 0 && h < kNumControls && kControls[h].encoder;   // SHIFT + encoder 3: the curve
    if (h >= 0 && h < kNumControls && shift_ && !shiftEncoder) {        // its page, or its red job
        const auto& c = kControls[h];
        if (!c.shiftCap) { openPageFor(c.id); return; }
    }
    if (h == kTypeWin) { showTypeMenu(); return; }
    if (h == kPatchWin1 || h == kPatchWin2) {           // the arrows step; the name opens the list
        const int L = h == kPatchWin1 ? 0 : 1;
        const auto r = patchWin(L);
        if (e.mods.isPopupMenu()) { showLayerMenu(L); return; }
        if (pt.x < r.getX() + 16.0f || pt.x > r.getRight() - 16.0f) {
            const auto before = proc_.layerName(L);
            { EditScope edit(proc_, "L" + juce::String(L + 1) + " PATCH"); proc_.stepLayer(L, pt.x < r.getCentreX() ? -1 : 1); }
            if (proc_.editLayer() != L) proc_.setEditLayer(L);
            refreshDepths();
            touchedUntil_ = 0;
            notice("LAYER " + juce::String(L + 1) + ": " + proc_.layerName(L).toUpperCase());
            juce::ignoreUnused(before);
            repaint();
        } else showLayerMenu(L);
        return;
    }
    if (h == kDisplay) {
        if (e.mods.isPopupMenu()) {
            if (const int slot = overviewSlotAt(pt); slot >= 0) { showSlotMenu(slot); return; }
            if (auto* p = pageParam(displayColumnAt(pt)); p && p->getParameterID().contains("_alt")) { showWaveMenu(p); return; }
            showPresetMenu();
            return;
        }
        if (const int slot = overviewSlotAt(pt); slot >= 0) {           // a line of the overview: drag for its amount, click for its page
            if (auto* p = slotParam(slot, 2)) {
                ovSlot_ = slot;
                drag_ = kDisplay;
                dragStartNorm_ = p->getValue();
                proc_.beginEdit("MATRIX " + juce::String(slot + 1) + " AMOUNT");
                p->beginChangeGesture();
            }
            return;
        }
        drag_ = kDisplay;
        dragCol_ = displayColumnAt(pt);
        if (dragCol_ < 0) {                             // the title line: the left half lists the pages, the right the patches
            drag_ = kNone;
            if (pt.x < kDispX + kDispW * 0.5f) showPageMenu(); else showPresetMenu();
            return;
        }
        if (auto* p = pageParam(dragCol_)) { dragStartNorm_ = p->getValue(); proc_.beginEdit(nameOf(p)); p->beginChangeGesture(); }
        return;
    }
    if (h < 0 || h >= kNumControls) return;
    const auto& c = kControls[h];
    if (c.encoder) {
        drag_ = h;
        dragCol_ = h - indexOf("enc_0");
        if (auto* p = pageParam(dragCol_)) { dragStartNorm_ = p->getValue(); proc_.beginEdit(nameOf(p)); p->beginChangeGesture(); }
        return;
    }
    if ((c.type == Knob && c.kind != kRing) || c.type == Fader) {
        if (auto* p = param(h)) { drag_ = h; dragStartNorm_ = p->getValue(); proc_.beginEdit(nameOf(p)); p->beginChangeGesture(); }
        return;
    }
    if (c.type == Knob && c.kind == kRing) { drag_ = h; if (auto* p = param(h)) proc_.beginEdit(nameOf(p)); return; }   // a click steps it (or picks a lamp); a drag turns it
    if (c.type == Key) { pressed_ = h; pressLamp_ = lampAt(h, pt); repaintPts(boundsOf(h)); }
    if (c.type == Lamp && (startsWith(c, "vm1_") || startsWith(c, "v1_") || startsWith(c, "vm2_") || startsWith(c, "v2_"))) press(h);
}

void TandemEditor::mouseDrag(const juce::MouseEvent& e) {
    const float dx = e.position.x - dragLast_.x, dy = e.position.y - dragLast_.y;
    dragLast_ = e.position;
    const bool fine = e.mods.isShiftDown();
    auto stepDrag = [&](juce::RangedAudioParameter* p, bool wrap) {
        dragSteps_ += -dy / (kStepDragPx * s_);
        while (std::abs(dragSteps_) >= 1.0f) {
            const int dir = dragSteps_ > 0 ? 1 : -1;
            dragSteps_ -= float(dir);
            stepParam(p, dir, wrap);
        }
    };
    auto columnDrag = [&](int col) {
        dragMoved_ = true;
        auto* p = pageParam(col);
        if (!p) return;
        const int n = p->getNumSteps();
        if (n > 1 && n < 200) stepDrag(p, false);
        else {
            dragAccum_ += (dx - dy) / (220.0f * s_) * (fine ? 0.15f : 1.0f);
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartNorm_ + dragAccum_));
        }
    };
    if (drag_ == kDisplay && ovSlot_ >= 0) {
        if (auto* p = slotParam(ovSlot_, 2)) {
            dragMoved_ = dragMoved_ || std::abs(e.position.x - e.mouseDownPosition.x) + std::abs(e.position.y - e.mouseDownPosition.y) > 3.0f;
            if (!dragMoved_) return;
            dragAccum_ += (dx - dy) / (260.0f * s_) * (fine ? 0.15f : 1.0f);
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartNorm_ + dragAccum_));
            refreshDepths();
            repaintPts({kDispX, kDispY, kDispW, kDispH});
        }
        return;
    }
    if (drag_ == kDisplay) { columnDrag(dragCol_); return; }
    if (drag_ < 0 || drag_ >= kNumControls) return;
    const auto& c = kControls[drag_];
    if (assigning_) {                                   // the knob turned sets the new slot's amount, both ways from zero
        if (auto* p = slotParam(assignSlot_, 2)) {
            dragMoved_ = true;
            dragAccum_ += (c.type == Fader ? -dy / (c.travel * s_) : (dx - dy) / (220.0f * s_)) * (fine ? 0.15f : 0.5f);
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartNorm_ + dragAccum_));
            refreshDepths();
            repaintPts(boundsOf(drag_));
        }
        return;
    }
    if (c.encoder) {
        const int k = drag_ - indexOf("enc_0");
        encAngle_[k] = std::fmod(encAngle_[k] - dy * 1.6f / s_ + 3600.0f, 360.0f);
        repaintPts(boundsOf(drag_));
        columnDrag(dragCol_);
        return;
    }
    if (c.type == Knob && c.kind == kRing) {
        if (std::abs(e.getDistanceFromDragStart()) > 3) dragMoved_ = true;
        if (dragMoved_) { stepDrag(param(drag_), false); touch(keyOf(drag_)); showTagFor(c.id); }
        return;
    }
    if (auto* p = param(drag_)) {
        dragMoved_ = true;
        if (c.type == Fader) dragAccum_ += -dy / (c.travel * s_) * (fine ? 0.2f : 1.0f);
        else dragAccum_ += (dx - dy) / (220.0f * s_) * (fine ? 0.15f : 1.0f);
        p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, dragStartNorm_ + dragAccum_));
        // The drive does nothing while its kind is off: turning it up picks the first kind.
        if (is(c, "fx_drive") && proc_.value("drv_mode") < 0.5f && p->getValue() > 0.0f)
            if (auto* m = proc_.param("drv_mode")) { m->beginChangeGesture(); TandemProcessor::set(m, 1.0f); m->endChangeGesture(); }
        if (is(c, "clock")) if (auto* h = proc_.param("perf_host"); h && h->getValue() >= 0.5f) { h->beginChangeGesture(); h->setValueNotifyingHost(0.0f); h->endChangeGesture(); }
        repaintPts(boundsOf(drag_));                    // the knob under the mouse at once, not at the next frame
        touch(keyOf(drag_));
        showTagFor(c.id);
    }
}

void TandemEditor::mouseUp(const juce::MouseEvent& e) {
    const bool wasMoved = dragMoved_;
    const auto pt = e.position / s_;
    if (assigning_) {                                   // one slot made: ASSIGN goes out, the slot's page stays up
        if (auto* p = slotParam(assignSlot_, 2)) p->endChangeGesture();
        proc_.endEdit();
        assigning_ = false;
        assign_ = false;
        const int was = drag_;
        drag_ = kNone; dragCol_ = -1;
        refreshDepths();
        if (was >= 0) repaintPts(boundsOf(was));
        repaint();
        return;
    }
    if (ovSlot_ >= 0) {
        const int slot = ovSlot_;
        ovSlot_ = -1;
        drag_ = kNone;
        if (auto* p = slotParam(slot, 2)) p->endChangeGesture();
        proc_.endEdit();
        if (!wasMoved) showMenu(2, slot + kMatrixFirst);
        repaint();
        return;
    }
    const bool curveEdit = shift_ && matrixPage() && dragCol_ == 2;
    if (drag_ == kDisplay || (drag_ >= 0 && drag_ < kNumControls && kControls[drag_].encoder)) {
        if (auto* p = pageParam(dragCol_)) p->endChangeGesture();
        if (!wasMoved)                         // a click steps a list or flips a switch
            if (auto* p = pageParam(dragCol_)) {
                const int n = p->getNumSteps();
                if (n > 1 && n < 200) stepParam(p, e.mods.isAltDown() ? -1 : 1, true);
            }
        proc_.endEdit();
    } else if (drag_ >= 0 && drag_ < kNumControls) {
        const auto& c = kControls[drag_];
        if (c.type == Knob && c.kind == kRing) {
            proc_.endEdit();
            if (!wasMoved) {
                const int k = lampAt(drag_, pt);
                if (k >= 0) selectLamp(drag_, k); else press(drag_, e.mods.isAltDown());
            }
        } else if (auto* p = param(drag_)) { p->endChangeGesture(); proc_.endEdit(); }
    }
    drag_ = kNone; dragCol_ = -1;
    if (curveEdit) { shift_ = false; repaint(); }       // SHIFT is used up
    if (pressed_ >= 0) {
        const int was = pressed_, lamp = pressLamp_;
        pressed_ = kNone; pressLamp_ = -1;
        repaintPts(boundsOf(was));
        if (hit(pt) == was) {
            if (lamp >= 0 && lamp == lampAt(was, pt)) selectLamp(was, lamp);
            else press(was, e.mods.isAltDown());
        }
    }
}

void TandemEditor::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto pt = e.position / s_;
    const int h = hit(pt);
    juce::RangedAudioParameter* p = nullptr;
    if (h == kDisplay) p = pageParam(displayColumnAt(pt));
    else if (h >= 0 && h < kNumControls) {
        const auto& c = kControls[h];
        if (c.encoder) p = pageParam(h - indexOf("enc_0"));
        else if ((c.type == Knob && c.kind != kRing) || c.type == Fader) p = param(h);
    }
    if (p) {
        EditScope edit(proc_, nameOf(p));
        p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture();
        if (h >= 0 && h < kNumControls && !kControls[h].encoder) showTagFor(kControls[h].id);
    }
}

void TandemEditor::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) {
    const auto pt = e.position / s_;
    const int h = hit(pt);
    const float d = wheel.deltaY * (wheel.isReversed ? -1.0f : 1.0f);
    if (d == 0.0f) return;
    const int dir = d > 0 ? 1 : -1;
    auto nudge = [&](juce::RangedAudioParameter* p) {
        if (!p) return;
        EditScope edit(proc_, nameOf(p));
        const int n = p->getNumSteps();
        if (n > 1 && n < 200) stepParam(p, dir, false);
        else p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + d * (e.mods.isShiftDown() ? 0.03f : 0.2f)));
    };
    if (h == kPatchWin1 || h == kPatchWin2) {           // scrolling over a layer's name steps its sound
        const int L = h == kPatchWin1 ? 0 : 1;
        { EditScope edit(proc_, "L" + juce::String(L + 1) + " PATCH"); proc_.stepLayer(L, -dir); }
        refreshDepths();
        repaint();
        return;
    }
    if (h == kDisplay && overviewSlotAt(pt) >= 0) {       // over a line of the overview: its amount
        nudge(slotParam(overviewSlotAt(pt), 2));
        refreshDepths();
        repaintPts({kDispX, kDispY, kDispW, kDispH});
        return;
    }
    if (h == kDisplay) {
        const int col = displayColumnAt(pt);
        if (col < 0) { const int n = int(pages(proc_.menu).size()); showMenu(proc_.menu, (proc_.page[proc_.menu] + dir + n) % n); }
        else nudge(pageParam(col));
        return;
    }
    if (h < 0 || h >= kNumControls) return;
    const auto& c = kControls[h];
    if (c.encoder) {
        const int k = h - indexOf("enc_0");
        encAngle_[k] = std::fmod(encAngle_[k] + float(dir) * 15.0f + 3600.0f, 360.0f);
        repaintPts(boundsOf(h));
        nudge(pageParam(k));
        return;
    }
    if (c.type == Key) return;
    if (auto* p = param(h)) {
        nudge(p);
        if (is(c, "clock")) if (auto* hp = proc_.param("perf_host"); hp && hp->getValue() >= 0.5f) { hp->beginChangeGesture(); hp->setValueNotifyingHost(0.0f); hp->endChangeGesture(); }
        touch(keyOf(h));
        showTagFor(c.id);
    }
}

// ---------------------------------------------------------------------------
// The keyboard: Cmd-Z and Shift-Cmd-Z, then each control's stand-in takes the arrows.
bool TandemEditor::keyPressed(const juce::KeyPress& k) {
    if (k.getModifiers().isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z'))
        return k.getModifiers().isShiftDown() ? (doRedo(), true) : (doUndo(), true);
    if (k.getKeyCode() == juce::KeyPress::escapeKey && (shift_ || assign_)) { shift_ = assign_ = false; repaint(); return true; }
    return false;
}

void TandemEditor::focusControl(const char* controlId) {
    const int ci = indexOf(controlId);
    for (auto& p : proxies_)
        if (p->index == ci) p->grabKeyboardFocus();
    focusChanged(ci);
}

juce::String TandemEditor::accessibleReading(const char* controlId) {
    const int ci = juce::String(controlId) == "disp" ? int(kDisplay) : indexOf(controlId);
    for (auto& p : proxies_)
        if (p->index == ci) {
            auto h = p->createAccessibilityHandler();
            if (!h) return {};
            const auto role = h->getRole();
            juce::String r = role == juce::AccessibilityRole::slider ? "slider" : role == juce::AccessibilityRole::button ? "button"
                           : role == juce::AccessibilityRole::staticText ? "text" : role == juce::AccessibilityRole::radioButton ? "radio" : "other";
            return r + ": " + p->getTitle() + ", " + (h->getValueInterface() ? h->getValueInterface()->getCurrentValueAsString() : juce::String());
        }
    return {};
}

void TandemEditor::focusChanged(int ci) {
    focusCi_ = ci;
    repaint();
}

void TandemEditor::accessibleNudge(int ci, int dir, bool fine) {
    if (ci == kDisplay) { const int n = int(pages(proc_.menu).size()); showMenu(proc_.menu, (proc_.page[proc_.menu] + dir + n) % n); return; }
    if (ci < 0 || ci >= kNumControls) return;
    const auto& c = kControls[ci];
    auto* p = c.encoder ? pageParam(ci - indexOf("enc_0")) : param(ci);
    if (!p) { if (c.type == Key) press(ci, dir < 0); return; }
    {
        EditScope edit(proc_, nameOf(p));
        const int n = p->getNumSteps();
        p->beginChangeGesture();
        if (n > 1 && n < 200) stepParam(p, dir, c.type == Key);
        else p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, p->getValue() + float(dir) * (fine ? 0.002f : 0.01f)));
        p->endChangeGesture();
    }
    if (!c.encoder) { touch(keyOf(ci)); if (c.type != Key) showTagFor(c.id); }
    repaintPts(boundsOf(ci));
}

void TandemEditor::accessibleActivate(int ci) {
    if (ci == kDisplay) { showPageMenu(); return; }
    if (ci < 0 || ci >= kNumControls) return;
    const auto& c = kControls[ci];
    if (c.type == Knob && c.kind != kRing && !c.encoder) { touch(keyOf(ci)); showTagFor(c.id); return; }
    if (c.encoder) return;
    press(ci);
}

// ---------------------------------------------------------------------------
// The stand-ins' workings.
ControlProxy::ControlProxy(TandemEditor& e, int ci) : ed(e), index(ci) {
    setInterceptsMouseClicks(false, false);
    setWantsKeyboardFocus(!(ci >= 0 && ci < kNumControls && kControls[ci].type == Lamp));     // lamps are read out with their key
    setTitle(name());
    if (ci >= 0 && ci < kNumControls)
        if (const auto* h = helpFor(kControls[ci].id)) setDescription(juce::String::fromUTF8(h->text));
}

juce::String ControlProxy::name() const {
    if (index == TandemEditor::kDisplay) return "Display";
    const auto& c = kControls[index];
    if (c.encoder) return "Encoder " + juce::String(index - indexOf("enc_0") + 1);
    if (const auto* h = helpFor(c.id)) return juce::String::fromUTF8(h->name);
    return c.id;
}

juce::RangedAudioParameter* ControlProxy::param() const {
    if (index == TandemEditor::kDisplay || index < 0 || index >= kNumControls) return nullptr;
    const auto& c = kControls[index];
    return c.encoder ? ed.pageParam(index - indexOf("enc_0")) : ed.param(index);
}

juce::String ControlProxy::valueString() const {
    if (index == TandemEditor::kDisplay) {
        const auto& pg = ed.currentPage();
        juce::String t = juce::String(TandemEditor::kMenuNames[ed.proc_.menu]) + " " + pg.title + ". ";
        for (int col = 0; col < 4; ++col)
            if (auto* p = ed.pageParam(col)) t << pg.label[col] << " " << valueText(ed.proc_, p) << ". ";
        return t + "Patch " + ed.patchLabel();
    }
    const auto& c = kControls[index];
    if (startsWith(c, "vm1_") || startsWith(c, "vm2_"))
        return juce::roundToInt(ed.proc_.value("voice_mode", c.id[2] - '1')) == c.id[4] - '0' ? "on" : "off";
    if (is(c, "layer_1") || is(c, "layer_2")) return ed.proc_.editLayer() == c.id[6] - '1' ? "editing" : "not editing";
    if (is(c, "tap")) return juce::String(ed.proc_.clockTempo(), 1) + " BPM";
    if (auto* p = param()) return valueText(ed.proc_, p);
    return {};
}

std::unique_ptr<juce::AccessibilityHandler> ControlProxy::createAccessibilityHandler() {
    struct Value final : juce::AccessibilityValueInterface {
        explicit Value(ControlProxy& o) : owner(o) {}
        bool isReadOnly() const override { return owner.param() == nullptr; }
        double getCurrentValue() const override { auto* p = owner.param(); return p ? p->getValue() : 0.0; }
        juce::String getCurrentValueAsString() const override { return owner.valueString(); }
        void setValue(double v) override {
            if (auto* p = owner.param()) {
                EditScope edit(owner.ed.proc_, nameOf(p));
                p->beginChangeGesture(); p->setValueNotifyingHost(float(juce::jlimit(0.0, 1.0, v))); p->endChangeGesture();
            }
        }
        void setValueAsString(const juce::String& t) override {
            if (auto* p = owner.param()) setValue(p->getValueForText(t));
        }
        AccessibleValueRange getRange() const override {
            auto* p = owner.param();
            if (!p) return {};
            const int n = p->getNumSteps();
            return {{0.0, 1.0}, n > 1 && n < 200 ? 1.0 / double(n - 1) : 0.01};
        }
        ControlProxy& owner;
    };
    juce::AccessibilityRole role = juce::AccessibilityRole::button;
    if (index == TandemEditor::kDisplay) role = juce::AccessibilityRole::staticText;
    else {
        const auto& c = kControls[index];
        if (c.type == Knob || c.type == Fader) role = juce::AccessibilityRole::slider;
        if (c.type == Lamp) role = startsWith(c, "vm1_") || startsWith(c, "vm2_") ? juce::AccessibilityRole::radioButton : juce::AccessibilityRole::ignored;
    }
    auto actions = juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press, [this] { ed.accessibleActivate(index); });
    return std::make_unique<juce::AccessibilityHandler>(*this, role, std::move(actions),
                                                        juce::AccessibilityHandler::Interfaces{std::make_unique<Value>(*this)});
}

bool ControlProxy::keyPressed(const juce::KeyPress& k) {
    const bool fine = k.getModifiers().isShiftDown();
    const int code = k.getKeyCode();
    if (k.getModifiers().isCommandDown()) return false;                 // Cmd-Z and the like: the window's
    if (code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey) { ed.accessibleNudge(index, 1, fine); return true; }
    if (code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey) { ed.accessibleNudge(index, -1, fine); return true; }
    if (code == juce::KeyPress::pageUpKey || code == juce::KeyPress::pageDownKey) {
        for (int i = 0; i < 10; ++i) ed.accessibleNudge(index, code == juce::KeyPress::pageUpKey ? 1 : -1, false);
        return true;
    }
    if (code == juce::KeyPress::spaceKey || code == juce::KeyPress::returnKey) { ed.accessibleActivate(index); return true; }
    if ((code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey || code == juce::KeyPress::homeKey) && param()) {
        auto* p = param();
        EditScope edit(ed.proc_, nameOf(p));
        p->beginChangeGesture(); p->setValueNotifyingHost(p->getDefaultValue()); p->endChangeGesture();
        return true;
    }
    return false;
}

void ControlProxy::focusGained(FocusChangeType) { ed.focusChanged(index); }
void ControlProxy::focusLost(FocusChangeType) { if (ed.focusCi_ == index) ed.focusChanged(TandemEditor::kNone); }
