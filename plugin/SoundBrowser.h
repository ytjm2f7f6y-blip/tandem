// A layer's sound browser: drops down from its name window. Kinds on the left, sounds on the right;
// a click plays the sound on the layer at once, a double-click (or Enter) keeps it and closes.
// Type to search; a star marks a favourite (kept for every Tandem, in the presets folder).
#pragma once

#include <functional>
#include <set>

#include "PluginProcessor.h"

class SoundBrowser : public juce::Component {
public:
    // load(index, part): put that sound (or that layer of a two-layer sound) on the layer. close(): take the browser down.
    SoundBrowser(TandemProcessor& p, int layer, juce::Typeface::Ptr face, std::function<void(int, int)> load, std::function<void()> close);

    static constexpr float kW = 470.0f, kH = 352.0f;        // points, at 100%
    void setScale(float s) { s_ = s; repaint(); }
    int layer() const { return layer_; }

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

    // The kinds of sound, as listed on the left.
    static const juce::StringArray& kinds();
    // For tests (and the keyboard): the rows shown, choosing one, a kind, a search.
    int numRows() const { return int(rows_.size()); }
    juce::String rowName(int r) const { return r >= 0 && r < numRows() ? rows_[size_t(r)].label : juce::String(); }
    void choose(int r);
    void setKind(int k);
    int kind() const { return kind_; }
    void setSearch(const juce::String& s);
    void toggleFavourite(int r);
    bool isFavourite(int r) const;
    void refresh();

    static juce::File favouritesFile();

private:
    struct Row { int index = 0, part = 0; juce::String label, key, kind; };
    TandemProcessor& proc_;
    const int layer_;
    juce::Typeface::Ptr face_;
    std::function<void(int, int)> load_;
    std::function<void()> close_;
    float s_ = 1.0f;
    int kind_ = 0, sel_ = -1, top_ = 0, hoverRow_ = -1, hoverKind_ = -1;
    juce::String search_;
    std::vector<Row> rows_;
    std::set<juce::String> favs_;

    juce::Font font(float h, bool bold = false) const;
    juce::Rectangle<float> listArea() const;
    juce::Rectangle<float> kindArea() const;
    int visibleRows() const;
    int rowAt(juce::Point<float> pt) const;      // points
    int kindAt(juce::Point<float> pt) const;
    bool onStar(juce::Point<float> pt) const;
    bool onClose(juce::Point<float> pt) const;
    void showRow(int r);
    void loadFavourites();
    void saveFavourites() const;
};
