#include "SoundBrowser.h"

namespace {
const juce::Colour kAmber(0xffffc066), kAmberDim(0xa0f0a850), kAmberFaint(0x30ff9a30), kGlass(0xfa141a16);
constexpr float kTitleH = 30.0f, kRowH = 21.0f, kKindW = 112.0f, kFootH = 22.0f, kPad = 8.0f;
}  // namespace

SoundBrowser::SoundBrowser(TandemProcessor& p, int layer, juce::Typeface::Ptr face, std::function<void(int, int)> load, std::function<void()> close)
    : proc_(p), layer_(juce::jlimit(0, 1, layer)), face_(std::move(face)), load_(std::move(load)), close_(std::move(close)) {
    setWantsKeyboardFocus(true);
    setTitle("Layer " + juce::String(layer_ + 1) + " sounds");
    setDescription("Click a sound to hear it on the layer; double-click or Enter keeps it. Type to search.");
    loadFavourites();
    // Open on the kind of the sound on the layer now.
    const auto list = proc_.presetList();
    const int cur = proc_.layerIndex(layer_);
    if (cur >= 0 && cur < int(list.size())) {
        const auto& e = list[size_t(cur)];
        const int k = kinds().indexOf(e.bothLayers ? juce::String("Two-layer") : (e.factory ? e.category : juce::String("Yours")));
        kind_ = k > 1 ? k : 0;
    }
    refresh();
    showRow(sel_);
}

const juce::StringArray& SoundBrowser::kinds() {
    static const juce::StringArray k{"All", "Favourites", "Bass", "Lead", "Pad", "Keys", "Pluck", "Arp", "Effects", "Two-layer", "Yours"};
    return k;
}

juce::File SoundBrowser::favouritesFile() { return TandemProcessor::presetFolder().getChildFile("favourites.txt"); }

void SoundBrowser::loadFavourites() {
    favs_.clear();
    juce::StringArray lines;
    lines.addLines(favouritesFile().loadFileAsString());
    for (auto& l : lines)
        if (l.trim().isNotEmpty()) favs_.insert(l.trim());
}

void SoundBrowser::saveFavourites() const {
    juce::StringArray lines;
    for (const auto& f : favs_) lines.add(f);
    if (TandemProcessor::presetFolder().createDirectory()) favouritesFile().replaceWithText(lines.joinIntoString("\n") + "\n");
}

void SoundBrowser::refresh() {
    rows_.clear();
    const auto list = proc_.presetList();
    const auto want = kinds()[kind_];
    for (int i = 0; i < int(list.size()); ++i) {
        const auto& e = list[size_t(i)];
        for (int part = 0; part < (e.bothLayers ? 2 : 1); ++part) {
            Row r;
            r.index = i;
            r.part = part;
            r.label = e.bothLayers ? e.name + "  (its layer " + juce::String(part + 1) + ")" : e.name;
            r.key = e.bothLayers ? e.name + "|" + juce::String(part + 1) : e.name;
            r.kind = e.bothLayers ? juce::String("Two-layer") : (e.factory ? e.category : juce::String("Yours"));
            const bool mine = !e.factory;
            bool show = true;
            if (want == "Favourites") show = favs_.count(r.key) > 0;
            else if (want == "Yours") show = mine;
            else if (want != "All") show = r.kind == want;
            if (search_.isNotEmpty()) show = show && r.label.containsIgnoreCase(search_);
            if (show) rows_.push_back(r);
        }
    }
    sel_ = -1;
    for (int r = 0; r < numRows(); ++r)
        if (rows_[size_t(r)].index == proc_.layerIndex(layer_) && rows_[size_t(r)].part == proc_.layerPart(layer_)) sel_ = r;
    top_ = juce::jlimit(0, std::max(0, numRows() - visibleRows()), top_);
    repaint();
}

void SoundBrowser::setKind(int k) {
    kind_ = juce::jlimit(0, kinds().size() - 1, k);
    top_ = 0;
    refresh();
    showRow(sel_);
}

void SoundBrowser::setSearch(const juce::String& s) {
    search_ = s;
    if (search_.isNotEmpty() && kind_ != 0 && kind_ != 1) kind_ = 0;      // a search looks through everything
    top_ = 0;
    refresh();
}

void SoundBrowser::choose(int r) {
    if (r < 0 || r >= numRows()) return;
    const auto row = rows_[size_t(r)];
    sel_ = r;
    showRow(r);
    if (load_) load_(row.index, row.part);
    repaint();
}

void SoundBrowser::toggleFavourite(int r) {
    if (r < 0 || r >= numRows()) return;
    const auto key = rows_[size_t(r)].key;
    if (favs_.count(key)) favs_.erase(key); else favs_.insert(key);
    saveFavourites();
    if (kinds()[kind_] == "Favourites") refresh();
    repaint();
}

bool SoundBrowser::isFavourite(int r) const { return r >= 0 && r < numRows() && favs_.count(rows_[size_t(r)].key) > 0; }

// ---------------------------------------------------------------------------
juce::Font SoundBrowser::font(float h, bool bold) const {
    auto f = face_ != nullptr ? juce::Font(juce::FontOptions(face_).withHeight(h)) : juce::Font(juce::FontOptions(h));
    return bold ? f.boldened() : f;
}

juce::Rectangle<float> SoundBrowser::kindArea() const { return {kPad, kTitleH + 6.0f, kKindW, kH - kTitleH - kFootH - 10.0f}; }
juce::Rectangle<float> SoundBrowser::listArea() const {
    return {kPad + kKindW + 10.0f, kTitleH + 6.0f, kW - kKindW - 2 * kPad - 10.0f, kH - kTitleH - kFootH - 10.0f};
}
int SoundBrowser::visibleRows() const { return int(listArea().getHeight() / kRowH); }

int SoundBrowser::rowAt(juce::Point<float> pt) const {
    const auto a = listArea();
    if (!a.contains(pt)) return -1;
    const int r = top_ + int((pt.y - a.getY()) / kRowH);
    return r < numRows() ? r : -1;
}

int SoundBrowser::kindAt(juce::Point<float> pt) const {
    const auto a = kindArea();
    if (!a.contains(pt)) return -1;
    const int k = int((pt.y - a.getY()) / kRowH);
    return k < kinds().size() ? k : -1;
}

bool SoundBrowser::onStar(juce::Point<float> pt) const { return listArea().contains(pt) && pt.x > listArea().getRight() - 26.0f; }
bool SoundBrowser::onClose(juce::Point<float> pt) const { return juce::Rectangle<float>(kW - 30.0f, 4.0f, 24.0f, 22.0f).contains(pt); }

void SoundBrowser::showRow(int r) {
    if (r < 0) return;
    const int n = visibleRows();
    if (r < top_) top_ = r;
    else if (r >= top_ + n) top_ = r - n + 1;
    top_ = juce::jlimit(0, std::max(0, numRows() - n), top_);
}

void SoundBrowser::paint(juce::Graphics& g) {
    g.addTransform(juce::AffineTransform::scale(s_));
    const auto all = juce::Rectangle<float>(0, 0, kW, kH);
    g.setColour(juce::Colours::black.withAlpha(0.35f));          // a shadow on the panel
    g.fillRoundedRectangle(all.translated(2.0f, 3.0f).reduced(1.0f), 7.0f);
    g.setColour(kGlass);
    g.fillRoundedRectangle(all.reduced(1.0f), 7.0f);
    g.setColour(juce::Colour(0xff3a4250));
    g.drawRoundedRectangle(all.reduced(1.5f), 7.0f, 2.0f);

    // Title, search, close.
    g.setColour(kAmber);
    g.setFont(font(14.0f));
    g.drawText("LAYER " + juce::String(layer_ + 1) + "  ·  SOUNDS", juce::Rectangle<float>(14, 0, 200, kTitleH), juce::Justification::centredLeft);
    g.setFont(font(12.0f));
    if (search_.isEmpty()) {
        g.setColour(kAmberDim);
        g.drawText("type to search", juce::Rectangle<float>(200, 0, kW - 246, kTitleH), juce::Justification::centredRight);
    } else {
        g.setColour(kAmber);
        g.drawText("search: " + search_ + "_", juce::Rectangle<float>(200, 0, kW - 246, kTitleH), juce::Justification::centredRight);
    }
    g.setColour(hoverRow_ == -2 ? kAmber : kAmberDim);
    const float cx = kW - 18.0f, cy = 15.0f;
    g.drawLine(cx - 5, cy - 5, cx + 5, cy + 5, 1.6f);
    g.drawLine(cx - 5, cy + 5, cx + 5, cy - 5, 1.6f);
    g.setColour(kAmberFaint);
    g.fillRect(juce::Rectangle<float>(kPad, kTitleH, kW - 2 * kPad, 1.0f));

    // Kinds.
    const auto ka = kindArea();
    for (int k = 0; k < kinds().size(); ++k) {
        const auto r = juce::Rectangle<float>(ka.getX(), ka.getY() + k * kRowH, ka.getWidth(), kRowH - 2.0f);
        if (k == kind_) { g.setColour(juce::Colour(0x40ff9a30)); g.fillRoundedRectangle(r, 3.0f); }
        else if (k == hoverKind_) { g.setColour(juce::Colour(0x18ff9a30)); g.fillRoundedRectangle(r, 3.0f); }
        g.setColour(k == kind_ ? kAmber : kAmberDim);
        g.setFont(font(12.5f));
        g.drawText(kinds()[k], r.reduced(8.0f, 0.0f), juce::Justification::centredLeft);
    }
    g.setColour(kAmberFaint);
    g.fillRect(juce::Rectangle<float>(ka.getRight() + 4.0f, ka.getY(), 1.0f, ka.getHeight()));

    // Sounds.
    const auto la = listArea();
    if (rows_.empty()) {
        g.setColour(kAmberDim);
        g.setFont(font(12.5f));
        g.drawText(kinds()[kind_] == "Favourites" && search_.isEmpty() ? "No favourites yet: click a sound's star" : "Nothing found",
                   la.withHeight(40.0f), juce::Justification::centred);
    }
    const bool showKind = kinds()[kind_] == "All" || kinds()[kind_] == "Favourites" || search_.isNotEmpty();
    for (int r = top_; r < std::min(numRows(), top_ + visibleRows()); ++r) {
        const auto& row = rows_[size_t(r)];
        const auto rr = juce::Rectangle<float>(la.getX(), la.getY() + (r - top_) * kRowH, la.getWidth() - 8.0f, kRowH - 1.0f);
        if (r == sel_) { g.setColour(juce::Colour(0x48ff9a30)); g.fillRoundedRectangle(rr, 3.0f); }
        else if (r == hoverRow_) { g.setColour(juce::Colour(0x1cff9a30)); g.fillRoundedRectangle(rr, 3.0f); }
        g.setColour(r == sel_ ? kAmber : kAmber.withAlpha(0.82f));
        g.setFont(font(13.0f));
        g.drawText(row.label, rr.reduced(8.0f, 0.0f).withTrimmedRight(showKind ? 92.0f : 24.0f), juce::Justification::centredLeft, true);
        if (showKind) {
            g.setColour(kAmberDim);
            g.setFont(font(10.5f));
            g.drawText(row.kind.toUpperCase(), rr.withTrimmedRight(26.0f).removeFromRight(70.0f), juce::Justification::centredRight);
        }
        const bool fav = favs_.count(row.key) > 0;           // the star
        juce::Path star;
        star.addStar({rr.getRight() - 11.0f, rr.getCentreY()}, 5, 2.6f, 6.2f, 0.0f);
        if (fav) { g.setColour(kAmber); g.fillPath(star); }
        else { g.setColour(r == hoverRow_ || r == sel_ ? kAmberDim : kAmberFaint); g.strokePath(star, juce::PathStrokeType(1.0f)); }
    }
    if (numRows() > visibleRows()) {                         // where in the list
        const float h = la.getHeight() * float(visibleRows()) / float(numRows());
        const float y = la.getY() + (la.getHeight() - h) * float(top_) / float(std::max(1, numRows() - visibleRows()));
        g.setColour(kAmberFaint.withAlpha(0.5f));
        g.fillRoundedRectangle(juce::Rectangle<float>(la.getRight() - 3.0f, y, 3.0f, h), 1.5f);
    }

    // How it works.
    g.setColour(kAmberFaint);
    g.fillRect(juce::Rectangle<float>(kPad, kH - kFootH - 2.0f, kW - 2 * kPad, 1.0f));
    g.setColour(kAmberDim);
    g.setFont(font(10.5f));
    g.drawText("click: hear it on layer " + juce::String(layer_ + 1) + "   ·   double-click or Enter: keep it   ·   Esc: close   ·   star: a favourite",
               juce::Rectangle<float>(kPad, kH - kFootH, kW - 2 * kPad, kFootH - 2.0f), juce::Justification::centred);
}

void SoundBrowser::mouseMove(const juce::MouseEvent& e) {
    const auto pt = e.position / s_;
    const int r = onClose(pt) ? -2 : rowAt(pt), k = kindAt(pt);
    if (r != hoverRow_ || k != hoverKind_) { hoverRow_ = r; hoverKind_ = k; repaint(); }
}

void SoundBrowser::mouseExit(const juce::MouseEvent&) { hoverRow_ = hoverKind_ = -1; repaint(); }

void SoundBrowser::mouseDown(const juce::MouseEvent& e) {
    const auto pt = e.position / s_;
    if (onClose(pt)) { if (close_) close_(); return; }
    if (const int k = kindAt(pt); k >= 0) { setKind(k); return; }
    if (const int r = rowAt(pt); r >= 0) {
        if (onStar(pt)) toggleFavourite(r);
        else choose(r);
    }
}

void SoundBrowser::mouseDoubleClick(const juce::MouseEvent& e) {
    const auto pt = e.position / s_;
    if (rowAt(pt) >= 0 && !onStar(pt) && close_) close_();
}

void SoundBrowser::mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) {
    const float d = w.deltaY * (w.isReversed ? -1.0f : 1.0f);
    if (d == 0.0f) return;
    top_ = juce::jlimit(0, std::max(0, numRows() - visibleRows()), top_ - (d > 0 ? 3 : -3));
    repaint();
}

bool SoundBrowser::keyPressed(const juce::KeyPress& k) {
    const int code = k.getKeyCode();
    if (code == juce::KeyPress::escapeKey || code == juce::KeyPress::returnKey) { if (close_) close_(); return true; }
    if (code == juce::KeyPress::downKey) { choose(sel_ < 0 ? 0 : std::min(numRows() - 1, sel_ + 1)); return true; }
    if (code == juce::KeyPress::upKey) { choose(sel_ < 0 ? 0 : std::max(0, sel_ - 1)); return true; }
    if (code == juce::KeyPress::rightKey) { setKind((kind_ + 1) % kinds().size()); return true; }
    if (code == juce::KeyPress::leftKey) { setKind((kind_ + kinds().size() - 1) % kinds().size()); return true; }
    if (code == juce::KeyPress::backspaceKey) { if (search_.isNotEmpty()) setSearch(search_.dropLastCharacters(1)); return true; }
    const auto ch = k.getTextCharacter();
    if (ch >= ' ' && !k.getModifiers().isCommandDown()) { setSearch(search_ + juce::String::charToString(ch)); return true; }
    return false;
}
