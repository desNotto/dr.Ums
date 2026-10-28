#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <numeric>
#include "PluginProcessor.h"

// Графические элементы интерфейса в стиле референса: белый фон, жирные чёрные рамки.
namespace ui
{
inline juce::String ru (const char* utf8) { return juce::String::fromUTF8 (utf8); }
inline juce::Font boldFont (float h)  { return juce::Font (juce::FontOptions (h, juce::Font::bold)); }
inline juce::Font plainFont (float h) { return juce::Font (juce::FontOptions (h)); }

inline const juce::Colour kRed { 0xFFE5262B };

class ReferenceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReferenceLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colours::white);
        setColour (juce::PopupMenu::textColourId, juce::Colours::black);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colours::black);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ScrollBar::thumbColourId, juce::Colours::black);
    }
};

// ------------------------------------------------------------------ иконки
inline void drawDice (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto c = r.getCentre();
    const float R = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;

    juce::Point<float> v[6];
    for (int i = 0; i < 6; ++i)
    {
        const float a = -juce::MathConstants<float>::halfPi + (float) i * juce::MathConstants<float>::pi / 3.0f;
        v[i] = { c.x + R * std::cos (a), c.y + R * std::sin (a) };
    }

    juce::Path hex;
    hex.startNewSubPath (v[0]);
    for (int i = 1; i < 6; ++i)
        hex.lineTo (v[i]);
    hex.closeSubPath();

    g.setColour (juce::Colours::white);
    g.fillPath (hex);

    g.setColour (juce::Colours::black);
    const juce::PathStrokeType st (R * 0.09f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.strokePath (hex, st);
    for (int k : { 1, 3, 5 })
    {
        juce::Path l;
        l.startNewSubPath (c);
        l.lineTo (v[k]);
        g.strokePath (l, st);
    }

    g.setFont (boldFont (R * 0.55f));
    const juce::Point<float> faces[3] = { (c + v[5] + v[0] + v[1]) / 4.0f,
                                          (c + v[1] + v[2] + v[3]) / 4.0f,
                                          (c + v[3] + v[4] + v[5]) / 4.0f };
    for (const auto& f : faces)
        g.drawText ("?", juce::Rectangle<float> (R * 0.6f, R * 0.6f).withCentre (f), juce::Justification::centred, false);
}

inline void drawGear (juce::Graphics& g, juce::Rectangle<float> r)
{
    const auto c = r.getCentre();
    const float R = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
    const float rin = R * 0.74f, hole = R * 0.28f;
    const int teeth = 8;
    const float step = juce::MathConstants<float>::twoPi / (float) teeth;

    auto pt = [&] (float ang, float rad)
    {
        return juce::Point<float> (c.x + rad * std::sin (ang), c.y - rad * std::cos (ang));
    };

    juce::Path p;
    for (int i = 0; i < teeth; ++i)
    {
        const float a = (float) i * step;
        const float w0 = step * 0.30f, w1 = step * 0.18f;
        const auto p0 = pt (a - w0, rin), p1 = pt (a - w1, R), p2 = pt (a + w1, R), p3 = pt (a + w0, rin);
        if (i == 0) p.startNewSubPath (p0); else p.lineTo (p0);
        p.lineTo (p1);
        p.lineTo (p2);
        p.lineTo (p3);
    }
    p.closeSubPath();
    p.addEllipse (c.x - hole, c.y - hole, hole * 2.0f, hole * 2.0f);
    p.setUsingNonZeroWinding (false);

    g.setColour (juce::Colours::black);
    g.fillPath (p);
}

inline void drawLock (juce::Graphics& g, juce::Rectangle<float> r, bool locked)
{
    const auto bg = locked ? juce::Colours::black : juce::Colours::white;
    const auto fg = locked ? juce::Colours::white : juce::Colours::black;

    g.setColour (bg);
    g.fillRoundedRectangle (r.reduced (1.5f), 9.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r.reduced (1.5f), 9.0f, 3.0f);

    const auto c = r.getCentre();
    const float s = juce::jmin (r.getWidth(), r.getHeight());
    const auto body = juce::Rectangle<float> (s * 0.46f, s * 0.34f).withCentre (c.translated (0.0f, s * 0.12f));

    const float shW = body.getWidth() * 0.62f;
    const float sx = c.x - shW * 0.5f;
    const float topY = body.getY() - s * 0.24f;
    const float rightBottom = locked ? body.getY() : body.getY() - s * 0.10f;

    juce::Path sh;
    sh.startNewSubPath (sx, body.getY());
    sh.lineTo (sx, topY + shW * 0.5f);
    sh.addArc (sx, topY, shW, shW, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, false);
    sh.lineTo (sx + shW, rightBottom);

    g.setColour (fg);
    g.strokePath (sh, juce::PathStrokeType (s * 0.075f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.fillRoundedRectangle (body, 3.0f);

    g.setColour (bg);
    g.fillEllipse (juce::Rectangle<float> (s * 0.09f, s * 0.09f).withCentre (body.getCentre()));
}

inline void drawHeart (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (kRed);
    g.fillEllipse (r);

    juce::Path h;
    h.startNewSubPath (0.5f, 0.88f);
    h.cubicTo (-0.05f, 0.50f, 0.02f, 0.10f, 0.28f, 0.10f);
    h.cubicTo (0.42f, 0.10f, 0.50f, 0.20f, 0.50f, 0.30f);
    h.cubicTo (0.50f, 0.20f, 0.58f, 0.10f, 0.72f, 0.10f);
    h.cubicTo (0.98f, 0.10f, 1.05f, 0.50f, 0.50f, 0.88f);
    h.closeSubPath();

    const auto box = r.reduced (r.getWidth() * 0.22f);
    h.applyTransform (juce::AffineTransform::scale (box.getWidth(), box.getHeight()).translated (box.getX(), box.getY()));
    g.setColour (juce::Colours::white);
    g.fillPath (h);
}

inline void drawPlay (juce::Graphics& g, juce::Rectangle<float> r, bool on)
{
    g.setColour (juce::Colours::white);
    g.fillRect (r.reduced (1.5f));
    g.setColour (juce::Colours::black);
    g.drawRect (r.reduced (1.5f), 3.0f);

    const auto in = r.reduced (r.getWidth() * 0.30f);
    if (on)
    {
        g.fillRect (in);
    }
    else
    {
        juce::Path t;
        t.addTriangle (in.getX() + in.getWidth() * 0.1f, in.getY(),
                       in.getX() + in.getWidth() * 0.1f, in.getBottom(),
                       in.getRight(), in.getCentreY());
        g.fillPath (t);
    }
}

// ------------------------------------------------------------------ кнопки
enum class Icon { Dice, Lock, Gear, Play, Heart };

class IconButton : public juce::Button
{
public:
    explicit IconButton (Icon i) : juce::Button (juce::String()), icon (i)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        r = down ? r.reduced (3.0f) : (over ? r.reduced (1.0f) : r.reduced (2.0f));

        switch (icon)
        {
            case Icon::Dice:  drawDice (g, r.reduced (3.0f)); break;
            case Icon::Lock:  drawLock (g, r, getToggleState()); break;
            case Icon::Gear:  drawGear (g, r); break;
            case Icon::Play:  drawPlay (g, r, getToggleState()); break;
            case Icon::Heart: drawHeart (g, r); break;
        }
    }

private:
    Icon icon;
};

class BoxButton : public juce::Button
{
public:
    BoxButton() : juce::Button (juce::String())
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setLabel (const juce::String& t) { label = t; repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (down ? juce::Colour (0xFFDDDDDD) : (over ? juce::Colour (0xFFF0F0F0) : juce::Colours::white));
        g.fillRect (r);
        g.setColour (juce::Colours::black);
        g.drawRect (r, 3.0f);
        g.setFont (boldFont (15.0f));
        g.drawFittedText (label, getLocalBounds().reduced (6, 2), juce::Justification::centred, 1);
    }

private:
    juce::String label;
};

// Рамка с несколькими вариантами в ряд (как "intro / verse / ..." в референсе).
class SegmentedBox : public juce::Component
{
public:
    std::function<void (int)> onChange;

    void setItems (const juce::StringArray& s) { items = s; repaint(); }
    void setSelected (int i) { if (sel != i) { sel = i; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colours::white);
        g.fillRect (r);

        float x = 0.0f;
        for (int i = 0; i < items.size(); ++i)
        {
            const float w = r.getWidth() * weight (i) / totalWeight();
            const juce::Rectangle<float> seg (x, 0.0f, w, r.getHeight());

            if (i == sel)
            {
                g.setColour (juce::Colours::black);
                g.fillRect (seg);
            }
            else if (i > 0)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.fillRect (seg.getX() - 0.5f, 8.0f, 1.0f, seg.getHeight() - 16.0f);
            }

            g.setColour (i == sel ? juce::Colours::white : juce::Colours::black);
            g.setFont (boldFont (14.0f));
            g.drawFittedText (items[i], seg.toNearestInt().reduced (3, 2), juce::Justification::centred, 1);
            x += w;
        }

        g.setColour (juce::Colours::black);
        g.drawRect (r.reduced (1.5f), 3.0f);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.position.x);
        if (i < 0)
            return;
        sel = i;
        repaint();
        if (onChange)
            onChange (i);
    }

private:
    float weight (int i) const { return (float) items[i].length() + 6.5f; }

    float totalWeight() const
    {
        float t = 0.0f;
        for (int i = 0; i < items.size(); ++i)
            t += weight (i);
        return juce::jmax (1.0f, t);
    }

    int indexAt (float x) const
    {
        float acc = 0.0f;
        for (int i = 0; i < items.size(); ++i)
        {
            acc += (float) getWidth() * weight (i) / totalWeight();
            if (x < acc)
                return i;
        }
        return items.size() - 1;
    }

    juce::StringArray items;
    int sel = 0;
};

// Поле музыкального размера с ручным вводом.
class SigBox : public juce::Component
{
public:
    std::function<void (int, int)> onApply;

    SigBox()
    {
        addAndMakeVisible (ed);
        ed.setJustification (juce::Justification::centred);
        ed.setFont (boldFont (17.0f));
        ed.setInputRestrictions (5, "0123456789/");
        ed.setSelectAllWhenFocused (true);
        ed.setBorder (juce::BorderSize<int> (0));
        ed.setColour (juce::TextEditor::backgroundColourId, juce::Colours::white);
        ed.setColour (juce::TextEditor::textColourId, juce::Colours::black);
        ed.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        ed.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        ed.setColour (juce::TextEditor::highlightColourId, juce::Colour (0xFFCCCCCC));
        ed.setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::black);
        ed.setColour (juce::CaretComponent::caretColourId, juce::Colours::black);
        ed.onReturnKey = [this] { commit(); ed.giveAwayKeyboardFocus(); };
        ed.onFocusLost = [this] { commit(); };
    }

    void setValue (int n, int d)
    {
        curNum = n;
        curDen = d;
        if (! ed.hasKeyboardFocus (true))
            ed.setText (juce::String (n) + "/" + juce::String (d), false);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::white);
        g.setColour (juce::Colours::black);
        g.drawRect (getLocalBounds(), 3);
    }

    void resized() override { ed.setBounds (getLocalBounds().reduced (4, 3)); }

private:
    void commit()
    {
        const auto t = ed.getText();
        const int n = t.upToFirstOccurrenceOf ("/", false, false).getIntValue();
        const int d = t.fromFirstOccurrenceOf ("/", false, false).getIntValue();

        if (drums::validTimeSig (n, d))
        {
            if (n != curNum || d != curDen)
            {
                curNum = n;
                curDen = d;
                if (onApply)
                    onApply (n, d);
            }
        }
        else
        {
            ed.setText (juce::String (curNum) + "/" + juce::String (curDen), false);
        }
    }

    juce::TextEditor ed;
    int curNum = 4, curDen = 4;
};

// ------------------------------------------------------------------ схема барабанной установки
class KitView : public juce::Component
{
public:
    void flash (int lane)
    {
        flashes[(size_t) lane] = 1.0f;
        repaint();
    }

    void decay()
    {
        bool any = false;
        for (auto& f : flashes)
            if (f > 0.0f)
            {
                f = juce::jmax (0.0f, f - 0.10f);
                any = true;
            }
        if (any)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xFFF2F2F2));

        sx = (float) getWidth() / 609.0f;
        sy = (float) getHeight() / 394.0f;

        // стойки
        g.setColour (juce::Colour (0xFF9A9A9A));
        g.drawLine (86.0f * sx, 170.0f * sy, 86.0f * sx, 370.0f * sy, 3.0f);
        g.drawLine (146.0f * sx, 60.0f * sy, 146.0f * sx, 200.0f * sy, 3.0f);
        g.drawLine (430.0f * sx, 60.0f * sy, 430.0f * sx, 200.0f * sy, 3.0f);

        // тарелки
        cymbal (g, 146, 42, 54, 34);     // краш слева
        cymbal (g,  86, 160, 50, 28);    // хет
        cymbal (g, 430, 38, 58, 34);     // краш справа
        cymbal (g, 466, 128, 56, 34);    // райд
        cymbal (g, 512, 180, 50, 30);    // чайна/сплэш

        // тома и снейр
        drum (g, 228, 112, 56, 42, Drums::toms);
        drum (g, 351, 112, 56, 42, Drums::toms);
        drum (g, 424, 262, 66, 50, Drums::toms);
        drum (g, 172, 250, 52, 38, Drums::snare);

        // бочка
        drum (g, 283, 286, 90, 74, Drums::kick);

        g.setColour (juce::Colours::black);
        g.drawRect (getLocalBounds(), 3);
    }

private:
    enum class Drums { toms, snare, kick };

    juce::Rectangle<float> box (float cx, float cy, float rx, float ry) const
    {
        return { (cx - rx) * sx, (cy - ry) * sy, rx * 2.0f * sx, ry * 2.0f * sy };
    }

    void overlay (juce::Graphics& g, juce::Rectangle<float> b, int lane) const
    {
        const float f = flashes[(size_t) lane];
        if (f > 0.0f)
        {
            g.setColour (kRed.withAlpha (f * 0.6f));
            g.fillEllipse (b);
        }
    }

    void cymbal (juce::Graphics& g, float cx, float cy, float rx, float ry) const
    {
        const auto b = box (cx, cy, rx, ry);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFFF0D585), b.getX(), b.getY(),
                                                 juce::Colour (0xFFB48A2F), b.getRight(), b.getBottom(), false));
        g.fillEllipse (b);
        g.setColour (juce::Colour (0xFF7A5C18));
        g.drawEllipse (b, 1.5f);
        g.fillEllipse (b.getCentreX() - 4.0f, b.getCentreY() - 3.0f, 8.0f, 6.0f);
        overlay (g, b, (int) drums::Lane::Cymbals);
    }

    void drum (juce::Graphics& g, float cx, float cy, float rx, float ry, Drums kind) const
    {
        const auto b = box (cx, cy, rx, ry);
        g.setColour (kind == Drums::snare ? juce::Colour (0xFF9AA0A6) : juce::Colour (0xFF4A4C50));
        g.fillEllipse (b);

        const auto head = b.reduced (b.getWidth() * 0.07f, b.getHeight() * 0.09f);
        g.setColour (juce::Colour (0xFFFAFAFA));
        g.fillEllipse (head);
        g.setColour (juce::Colour (0xFFBBBBBB));
        g.drawEllipse (head.reduced (head.getWidth() * 0.18f, head.getHeight() * 0.18f), 1.5f);

        const auto lane = kind == Drums::kick ? drums::Lane::Kick
                        : (kind == Drums::snare ? drums::Lane::Snare : drums::Lane::Toms);
        overlay (g, b, (int) lane);
    }

    std::array<float, 4> flashes {};
    mutable float sx = 1.0f, sy = 1.0f;
};

// ------------------------------------------------------------------ превью на 4 такта
class PreviewView : public juce::Component
{
public:
    std::function<juce::File()> getDragFile;

    explicit PreviewView (DrUmsProcessor& p) : proc (p)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& p = proc.getShownPattern();
        const auto& map = proc.currentMap();

        const int bars = p.bars;
        const float gap = 8.0f;
        const float tileW = ((float) getWidth() - gap * (float) (bars - 1)) / (float) bars;
        const float H = (float) getHeight();

        std::vector<int> notes;
        for (const auto& h : p.hits)
        {
            const int n = map.noteFor (h.art);
            if (n >= 0)
                notes.push_back (n);
        }
        std::sort (notes.begin(), notes.end());
        notes.erase (std::unique (notes.begin(), notes.end()), notes.end());

        const int rows = juce::jmax (1, (int) notes.size());
        const float rowH = (H - 6.0f) / (float) rows;
        const int spb = p.stepsPerBar();
        const double bpb = p.beatsPerBar();
        const double playBeat = proc.getPlayBeat();

        for (int b = 0; b < bars; ++b)
        {
            const juce::Rectangle<float> tile ((float) b * (tileW + gap), 0.0f, tileW, H);

            juce::Path shape;
            shape.addRoundedRectangle (tile.getX(), tile.getY(), tile.getWidth(), tile.getHeight(),
                                       12.0f, 12.0f, false, true, false, true);
            g.setColour (juce::Colour (0xFFB7B7B7));
            g.fillPath (shape);

            g.saveState();
            g.reduceClipRegion (shape, juce::AffineTransform());

            g.setColour (juce::Colour (0x22000000));
            for (int k = 1; k < (int) std::ceil (bpb); ++k)
                g.fillRect (tile.getX() + tile.getWidth() * (float) ((double) k / bpb) - 0.5f, 0.0f, 1.0f, H);

            g.setColour (kRed);
            const float noteW = juce::jmax (2.0f, tile.getWidth() / (float) spb * 0.8f);
            for (const auto& h : p.hits)
            {
                if (h.step / spb != b)
                    continue;
                const int n = map.noteFor (h.art);
                if (n < 0)
                    continue;

                const int idx = (int) (std::lower_bound (notes.begin(), notes.end(), n) - notes.begin());
                const float x = tile.getX() + tile.getWidth() * (float) (h.step % spb) / (float) spb;
                const float y = 3.0f + (float) (rows - 1 - idx) * rowH;
                g.fillRect (x, y, noteW, juce::jmax (2.0f, rowH - 1.0f));
            }

            if (playBeat >= 0.0 && (int) (playBeat / bpb) == b)
            {
                const double inBar = playBeat - (double) b * bpb;
                g.setColour (juce::Colours::white);
                g.fillRect (tile.getX() + tile.getWidth() * (float) (inBar / bpb) - 1.0f, 0.0f, 2.0f, H);
            }

            g.restoreState();
        }
    }

    void mouseDown (const juce::MouseEvent&) override { dragging = false; }
    void mouseUp (const juce::MouseEvent&) override   { dragging = false; }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 8 || ! getDragFile)
            return;

        const auto f = getDragFile();
        if (! f.existsAsFile())
            return;

        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this);
        dragging = false;
    }

private:
    DrUmsProcessor& proc;
    bool dragging = false;
};

// ------------------------------------------------------------------ вкладка GROOVES
class GroovesView : public juce::Component,
                    private juce::TableListBoxModel
{
public:
    std::function<void (const GrooveEntry&)> onSelect, onLoad;

    explicit GroovesView (DrUmsProcessor& p) : proc (p), table (juce::String(), this)
    {
        addAndMakeVisible (table);
        table.setRowHeight (28);
        table.setMultipleSelectionEnabled (false);
        table.setOutlineThickness (3);
        table.setColour (juce::ListBox::backgroundColourId, juce::Colours::white);
        table.setColour (juce::ListBox::outlineColourId, juce::Colours::black);

        auto& h = table.getHeader();
        h.setColour (juce::TableHeaderComponent::backgroundColourId, juce::Colours::black);
        h.setColour (juce::TableHeaderComponent::textColourId, juce::Colours::white);
        h.setColour (juce::TableHeaderComponent::outlineColourId, juce::Colours::white.withAlpha (0.4f));
        h.setColour (juce::TableHeaderComponent::highlightColourId, juce::Colour (0xFF333333));

        const int colFlags = juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable
                        | juce::TableHeaderComponent::sortable;
        h.addColumn (ru ("Стиль"), 1, 120, 60, 300, colFlags);
        h.addColumn (ru ("Часть песни"), 2, 120, 60, 300, colFlags);
        h.addColumn (ru ("Название паттерна"), 3, 250, 100, 500, colFlags);
        h.addColumn (ru ("Размер"), 4, 80, 50, 200, colFlags);
        h.addColumn (ru ("Дата сохранения"), 5, 170, 80, 300, colFlags);
        h.setStretchToFitActive (true);
        h.setSortColumnId (5, false);   // по умолчанию — по дате, новые сверху
    }

    void refresh()
    {
        rebuildOrder();
        table.updateContent();
        table.repaint();
    }

    void resized() override { table.setBounds (getLocalBounds()); }

private:
    const GrooveEntry* entryAt (int row) const
    {
        const auto& es = proc.getLibrary().getEntries();
        if (row < 0 || row >= (int) order.size() || order[(size_t) row] >= (int) es.size())
            return nullptr;
        return &es[(size_t) order[(size_t) row]];
    }

    void rebuildOrder()
    {
        const auto& es = proc.getLibrary().getEntries();
        order.resize (es.size());
        std::iota (order.begin(), order.end(), 0);

        std::stable_sort (order.begin(), order.end(), [&] (int a, int b)
        {
            const auto& x = es[(size_t) a];
            const auto& y = es[(size_t) b];
            int c = 0;
            switch (sortCol)
            {
                case 1:  c = (int) x.pattern.style - (int) y.pattern.style; break;
                case 2:  c = (int) x.pattern.part - (int) y.pattern.part; break;
                case 3:  c = x.name.compareNatural (y.name); break;
                case 4:  c = x.pattern.beatsPerBar() < y.pattern.beatsPerBar() ? -1
                           : (x.pattern.beatsPerBar() > y.pattern.beatsPerBar() ? 1 : 0); break;
                default: c = x.savedMs < y.savedMs ? -1 : (x.savedMs > y.savedMs ? 1 : 0); break;
            }
            return sortForwards ? c < 0 : c > 0;
        });
    }

    int getNumRows() override { return (int) order.size(); }

    void paintRowBackground (juce::Graphics& g, int row, int, int, bool selected) override
    {
        g.fillAll (selected ? juce::Colours::black
                            : (row % 2 != 0 ? juce::Colour (0xFFF3F3F3) : juce::Colours::white));
    }

    void paintCell (juce::Graphics& g, int row, int col, int w, int h, bool selected) override
    {
        const auto* e = entryAt (row);
        if (e == nullptr)
            return;

        juce::String text;
        switch (col)
        {
            case 1: text = juce::String (drums::styleName (e->pattern.style)); break;
            case 2: text = juce::String (drums::partName (e->pattern.part)); break;
            case 3: text = e->name; break;
            case 4: text = e->pattern.timeSigText(); break;
            case 5: text = juce::Time (e->savedMs).formatted ("%d.%m.%Y  %H:%M"); break;
            default: break;
        }

        g.setColour (selected ? juce::Colours::white : juce::Colours::black);
        g.setFont (plainFont (14.0f));
        g.drawText (text, 10, 0, w - 14, h, juce::Justification::centredLeft, true);
    }

    void sortOrderChanged (int newSortColumnId, bool isForwards) override
    {
        sortCol = newSortColumnId;
        sortForwards = isForwards;
        refresh();
    }

    void selectedRowsChanged (int lastRowSelected) override
    {
        if (const auto* e = entryAt (lastRowSelected))
            if (onSelect)
                onSelect (*e);
    }

    void cellDoubleClicked (int row, int, const juce::MouseEvent&) override
    {
        if (const auto* e = entryAt (row))
            if (onLoad)
                onLoad (*e);
    }

    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        if (rows.size() == 0)
            return {};
        if (const auto* e = entryAt (rows[0]))
            return "groove:" + juce::String (e->id);
        return {};
    }

    DrUmsProcessor& proc;
    juce::TableListBox table;
    std::vector<int> order;
    int sortCol = 5;
    bool sortForwards = false;
};
} // namespace ui
