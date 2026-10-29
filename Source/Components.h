#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <numeric>
#include "PluginProcessor.h"

// Интерфейс dr. Ums: тёмная тема, фиолетовый акцент, у каждой дорожки свой цвет.
// Кубики и замки оставлены как в референсе (белые плитки с чёрной графикой).
namespace ui
{
inline juce::String ru (const char* utf8) { return juce::String::fromUTF8 (utf8); }

inline juce::Font boldFont (float h, float kerning = 0.0f)
{
    juce::Font f (juce::FontOptions (h, juce::Font::bold));
    return kerning > 0.0f ? f.withExtraKerningFactor (kerning) : f;
}
inline juce::Font plainFont (float h) { return juce::Font (juce::FontOptions (h)); }

namespace col
{
    inline const juce::Colour bg0      { 0xFF0C0F15 };
    inline const juce::Colour bg1      { 0xFF161B26 };
    inline const juce::Colour panel    { 0xFF1A202C };
    inline const juce::Colour panelHi  { 0xFF242C3C };
    inline const juce::Colour line     { 0xFF2D3648 };
    inline const juce::Colour text     { 0xFFE9EDF5 };
    inline const juce::Colour dim      { 0xFF8590A6 };
    inline const juce::Colour accent   { 0xFF7C6CF5 };
    inline const juce::Colour accentHi { 0xFF9D90FF };
    inline const juce::Colour heart    { 0xFFFF4D5E };

    inline juce::Colour lane (drums::Lane l)
    {
        switch (l)
        {
            case drums::Lane::Kick:    return juce::Colour (0xFFFF5D6C);
            case drums::Lane::Snare:   return juce::Colour (0xFFFFC857);
            case drums::Lane::Toms:    return juce::Colour (0xFF36D6A0);
            case drums::Lane::Cymbals: return juce::Colour (0xFF4FB3FF);
        }
        return text;
    }
}

class ReferenceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReferenceLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::panel);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::headerTextColourId, col::dim);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::ScrollBar::thumbColourId, col::line);
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

inline void drawGear (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour colour)
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

    g.setColour (colour);
    g.fillPath (p);
}

inline void drawLock (juce::Graphics& g, juce::Rectangle<float> r, bool locked, juce::Colour tint)
{
    const auto bg = locked ? juce::Colour (0xFF0B0D12) : juce::Colours::white;
    const auto fg = locked ? juce::Colours::white : juce::Colours::black;
    const auto edge = locked ? tint : juce::Colours::black;

    g.setColour (bg);
    g.fillRoundedRectangle (r.reduced (1.5f), 10.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (r.reduced (1.5f), 10.0f, 3.0f);

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

inline void drawHeart (juce::Graphics& g, juce::Rectangle<float> r, bool over)
{
    g.setColour (col::heart.withAlpha (over ? 0.35f : 0.20f));
    g.fillEllipse (r.expanded (4.0f));
    g.setGradientFill (juce::ColourGradient::vertical (col::heart.brighter (0.2f), r.getY(), col::heart.darker (0.2f), r.getBottom()));
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

inline void drawPlay (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool over)
{
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    const auto circle = juce::Rectangle<float> (d, d).withCentre (r.getCentre());

    g.setColour (col::accent.withAlpha (on ? 0.35f : (over ? 0.28f : 0.16f)));
    g.fillEllipse (circle.expanded (5.0f));
    g.setGradientFill (juce::ColourGradient::vertical (col::accentHi, circle.getY(), col::accent.darker (0.15f), circle.getBottom()));
    g.fillEllipse (circle);

    const auto in = circle.reduced (d * 0.31f);
    g.setColour (juce::Colours::white);
    if (on)
    {
        g.fillRoundedRectangle (in.reduced (in.getWidth() * 0.04f), 2.5f);
    }
    else
    {
        juce::Path t;
        t.addTriangle (in.getX() + in.getWidth() * 0.12f, in.getY(),
                       in.getX() + in.getWidth() * 0.12f, in.getBottom(),
                       in.getRight() + in.getWidth() * 0.06f, in.getCentreY());
        g.fillPath (juce::Path (t));
    }
}

inline void drawGearButton (juce::Graphics& g, juce::Rectangle<float> r, bool over)
{
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    const auto circle = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    g.setColour (over ? col::panelHi : col::panel);
    g.fillEllipse (circle);
    g.setColour (col::line);
    g.drawEllipse (circle.reduced (0.5f), 1.0f);
    drawGear (g, circle.reduced (d * 0.25f), over ? col::text : col::dim);
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

    void setTint (juce::Colour c) { tint = c; repaint(); }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat();
        const float shrink = down ? 3.0f : (over ? 1.0f : 2.0f);

        switch (icon)
        {
            case Icon::Dice:  drawDice (g, r.reduced (shrink + 3.0f)); break;
            case Icon::Lock:  drawLock (g, r.reduced (shrink), getToggleState(), tint); break;
            case Icon::Gear:  drawGearButton (g, r.reduced (down ? 2.0f : 0.0f), over); break;
            case Icon::Play:  drawPlay (g, r.reduced (down ? 7.0f : 5.0f), getToggleState(), over); break;
            case Icon::Heart: drawHeart (g, r.reduced (down ? 6.0f : 4.0f), over); break;
        }
    }

private:
    Icon icon;
    juce::Colour tint { juce::Colours::white };
};

// Выпадающий список: мелкая подпись сверху и значение крупно (библиотека, стиль).
class DropButton : public juce::Button
{
public:
    explicit DropButton (const juce::String& captionText) : juce::Button (juce::String()), caption (captionText)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setValue (const juce::String& v) { if (value != v) { value = v; repaint(); } }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? col::bg1 : (over ? col::panelHi : col::panel));
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (over ? col::accent.withAlpha (0.7f) : col::line);
        g.drawRoundedRectangle (r, 12.0f, 1.0f);

        g.setColour (col::dim);
        g.setFont (boldFont (9.5f, 0.08f));
        g.drawText (caption, juce::Rectangle<float> (r.getX() + 13.0f, r.getY() + 6.0f, r.getWidth() - 30.0f, 12.0f),
                    juce::Justification::centredLeft, false);

        g.setColour (col::text);
        g.setFont (boldFont (15.0f));
        g.drawFittedText (value, juce::Rectangle<int> ((int) r.getX() + 13, (int) r.getY() + 19,
                                                       (int) r.getWidth() - 36, (int) r.getHeight() - 24),
                          juce::Justification::centredLeft, 1);

        juce::Path chev;
        const float cx = r.getRight() - 17.0f, cy = r.getCentreY() + 2.0f;
        chev.startNewSubPath (cx - 4.5f, cy - 2.0f);
        chev.lineTo (cx, cy + 2.5f);
        chev.lineTo (cx + 4.5f, cy - 2.0f);
        g.setColour (over ? col::text : col::dim);
        g.strokePath (chev, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    juce::String caption, value;
};

// Сегментированный переключатель в виде "таблетки" (части песни, EDIT / GROOVES).
class SegmentedBox : public juce::Component
{
public:
    std::function<void (int)> onChange;
    float fontSize = 14.0f;

    SegmentedBox() { setMouseCursor (juce::MouseCursor::PointingHandCursor); }

    void setItems (const juce::StringArray& s) { items = s; repaint(); }
    void setSelected (int i) { if (sel != i) { sel = i; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const float rad = r.getHeight() * 0.5f;

        g.setColour (col::panel);
        g.fillRoundedRectangle (r, rad);
        g.setColour (col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.0f);

        const auto inner = r.reduced (4.0f);
        float x = inner.getX();
        const float tw = totalWeight();

        for (int i = 0; i < items.size(); ++i)
        {
            const float w = inner.getWidth() * weight (i) / tw;
            const juce::Rectangle<float> seg (x, inner.getY(), w, inner.getHeight());

            if (i == sel)
            {
                g.setGradientFill (juce::ColourGradient::vertical (col::accentHi, seg.getY(), col::accent, seg.getBottom()));
                g.fillRoundedRectangle (seg, inner.getHeight() * 0.5f);
            }
            else if (i == hover)
            {
                g.setColour (col::panelHi);
                g.fillRoundedRectangle (seg, inner.getHeight() * 0.5f);
            }

            g.setColour (i == sel ? juce::Colours::white : (i == hover ? col::text : col::dim));
            g.setFont (boldFont (fontSize));
            g.drawFittedText (items[i], seg.toNearestInt().reduced (3, 1), juce::Justification::centred, 1);
            x += w;
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int i = indexAt (e.position.x);
        if (i != hover)
        {
            hover = i;
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hover = -1;
        repaint();
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
        if (items.isEmpty())
            return -1;
        x -= 4.0f;
        float acc = 0.0f;
        for (int i = 0; i < items.size(); ++i)
        {
            acc += ((float) getWidth() - 8.0f) * weight (i) / totalWeight();
            if (x < acc)
                return i;
        }
        return items.size() - 1;
    }

    juce::StringArray items;
    int sel = 0;
    int hover = -1;
};

// Поле музыкального размера с ручным вводом.
class SigBox : public juce::Component
{
public:
    std::function<void (int, int)> onApply;

    explicit SigBox (const juce::String& captionText) : caption (captionText)
    {
        addAndMakeVisible (ed);
        ed.setJustification (juce::Justification::centred);
        ed.setFont (boldFont (17.0f));
        ed.setInputRestrictions (5, "0123456789/");
        ed.setSelectAllWhenFocused (true);
        ed.setBorder (juce::BorderSize<int> (0));
        ed.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        ed.setColour (juce::TextEditor::textColourId, col::text);
        ed.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        ed.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        ed.setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.5f));
        ed.setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        ed.setColour (juce::CaretComponent::caretColourId, col::accentHi);
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
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (col::line);
        g.drawRoundedRectangle (r, 12.0f, 1.0f);

        g.setColour (col::dim);
        g.setFont (boldFont (9.5f, 0.08f));
        g.drawText (caption, juce::Rectangle<float> (r.getX(), r.getY() + 6.0f, r.getWidth(), 12.0f),
                    juce::Justification::centred, false);
    }

    void resized() override { ed.setBounds (6, 18, getWidth() - 12, getHeight() - 22); }

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

    juce::String caption;
    juce::TextEditor ed;
    int curNum = 4, curDen = 4;
};

// ------------------------------------------------------------------ барабанная установка
class KitView : public juce::Component
{
public:
    void flash (int art)
    {
        if (art >= 0 && art < drums::kNumArts)
        {
            flashes[(size_t) art] = 1.0f;
            repaint();
        }
    }

    void decay()
    {
        bool any = false;
        for (auto& f : flashes)
            if (f > 0.0f)
            {
                f = juce::jmax (0.0f, f - 0.08f);
                any = true;
            }
        if (any)
            repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();

        // сцена
        juce::Path panel;
        panel.addRoundedRectangle (b.reduced (0.5f), 18.0f);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xFF1D2433), b.getY(), juce::Colour (0xFF0F131B), b.getBottom()));
        g.fillPath (panel);

        g.saveState();
        g.reduceClipRegion (panel, juce::AffineTransform());

        g.setGradientFill (juce::ColourGradient (juce::Colour (0x30A99CFF), b.getCentreX(), b.getY() + 40.0f,
                                                 juce::Colour (0x00A99CFF), b.getCentreX(), b.getY() + 40.0f + b.getWidth() * 0.62f, true));
        g.fillRect (b);

        // пол: эллипс света под установкой
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x40000000), b.getCentreX(), b.getBottom() - 70.0f,
                                                 juce::Colour (0x00000000), b.getCentreX() + b.getWidth() * 0.5f, b.getBottom() - 70.0f, true));
        g.fillEllipse (b.getCentreX() - b.getWidth() * 0.5f, b.getBottom() - 150.0f, b.getWidth(), 170.0f);

        const float s = std::min (b.getWidth() / kDesignW, b.getHeight() / kDesignH);
        g.addTransform (juce::AffineTransform::scale (s).translated ((b.getWidth() - kDesignW * s) * 0.5f,
                                                                     (b.getHeight() - kDesignH * s) * 0.5f));
        g.addTransform (juce::AffineTransform::translation (0.0f, -12.0f));
        drawKit (g);
        g.restoreState();

        g.setColour (col::line);
        g.drawRoundedRectangle (b.reduced (0.5f), 18.0f, 1.0f);
    }

private:
    using A = drums::Art;
    using Pt = juce::Point<float>;
    static constexpr float kDesignW = 586.0f, kDesignH = 380.0f;

    float fl (A a) const { return flashes[(size_t) a]; }

    float flAny (std::initializer_list<A> l) const
    {
        float m = 0.0f;
        for (auto a : l)
            m = juce::jmax (m, fl (a));
        return m;
    }

    void halo (juce::Graphics& g, juce::Rectangle<float> b, A a, float f) const
    {
        if (f <= 0.0f)
            return;
        const auto c = col::lane (drums::laneOf (a));
        const auto m = b.getCentre();
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.55f * f), m.x, m.y,
                                                 c.withAlpha (0.0f), m.x + b.getWidth() * 0.78f, m.y, true));
        g.fillEllipse (b.expanded (b.getWidth() * 0.5f, b.getHeight() * 0.9f));
    }

    static void pole (juce::Graphics& g, Pt a, Pt b, float w = 3.0f)
    {
        g.setColour (juce::Colour (0x66000000));
        g.drawLine (juce::Line<float> (a.translated (2.0f, 1.0f), b.translated (2.0f, 1.0f)), w);
        g.setColour (juce::Colour (0xFF8E98AB));
        g.drawLine (juce::Line<float> (a, b), w);
        g.setColour (juce::Colour (0x55FFFFFF));
        g.drawLine (juce::Line<float> (a.translated (-0.8f, 0.0f), b.translated (-0.8f, 0.0f)), 1.0f);
    }

    static void tripod (juce::Graphics& g, Pt base, float spread = 36.0f)
    {
        pole (g, base, base.translated (-spread, 14.0f), 2.2f);
        pole (g, base, base.translated (spread, 14.0f), 2.2f);
        pole (g, base, base.translated (0.0f, 20.0f), 2.2f);
    }

    void cymbal (juce::Graphics& g, Pt c, float rx, float ry, A colourArt, float f, bool brass = false) const
    {
        const auto bounds = juce::Rectangle<float> (rx * 2.0f, ry * 2.0f).withCentre (c);
        halo (g, bounds, colourArt, f);

        g.setColour (juce::Colours::black.withAlpha (0.30f));
        g.fillEllipse (bounds.translated (4.0f, 8.0f));

        const auto light = brass ? juce::Colour (0xFFF3D78E) : juce::Colour (0xFFFAEAA8);
        const auto mid   = brass ? juce::Colour (0xFFC2902F) : juce::Colour (0xFFD7A83C);
        const auto edge  = brass ? juce::Colour (0xFF704A12) : juce::Colour (0xFF8C6217);

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (1.0f, ry / rx, c.x, c.y));

        juce::ColourGradient grad (light, c.x - rx * 0.25f, c.y - rx * 0.30f, edge, c.x + rx * 0.95f, c.y + rx * 0.95f, true);
        grad.addColour (0.42, mid);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - rx, c.y - rx, rx * 2.0f, rx * 2.0f);

        // проточка
        for (int k = 1; k <= 4; ++k)
        {
            const float q = 0.2f + 0.18f * (float) k;
            g.setColour (k % 2 ? juce::Colour (0x28000000) : juce::Colour (0x1CFFFFFF));
            g.drawEllipse (c.x - rx * q, c.y - rx * q, rx * q * 2.0f, rx * q * 2.0f, 0.9f);
        }

        // купол
        const float br = rx * 0.19f;
        g.setGradientFill (juce::ColourGradient (light, c.x - br * 0.4f, c.y - br * 0.5f, mid.darker (0.2f), c.x + br, c.y + br, true));
        g.fillEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f);
        g.setColour (juce::Colour (0x55000000));
        g.drawEllipse (c.x - br, c.y - br, br * 2.0f, br * 2.0f, 0.8f);
        g.setColour (juce::Colour (0xAA3A2608));
        g.fillEllipse (c.x - 2.0f, c.y - 2.0f, 4.0f, 4.0f);

        // блик и кромка
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, rx * 0.93f, rx * 0.93f, 0.0f, -2.5f, -0.9f, true);
        g.setColour (juce::Colour (0x66FFFFFF));
        g.strokePath (arc, juce::PathStrokeType (1.6f));
        g.setColour (edge);
        g.drawEllipse (c.x - rx, c.y - rx, rx * 2.0f, rx * 2.0f, 1.6f);

        if (f > 0.0f)
        {
            g.setColour (col::lane (drums::Lane::Cymbals).withAlpha (0.38f * f));
            g.fillEllipse (c.x - rx, c.y - rx, rx * 2.0f, rx * 2.0f);
        }
        g.restoreState();
    }

    void drum (juce::Graphics& g, Pt c, float rx, float ry, float depth, A a, float f, bool metal, bool legs) const
    {
        const auto whole = juce::Rectangle<float> (rx * 2.0f, ry * 2.0f + depth).withCentre ({ c.x, c.y + depth * 0.5f });
        halo (g, whole, a, f);

        // тень на полу
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillEllipse (c.x - rx * 1.05f, c.y + depth - ry * 0.35f, rx * 2.1f, ry * 1.5f);

        if (legs)
            for (float dx : { -0.78f, 0.0f, 0.78f })
            {
                const Pt top (c.x + dx * rx, c.y + depth * 0.6f + (dx == 0.0f ? ry * 0.7f : 0.0f));
                pole (g, top, Pt (c.x + dx * rx * 1.12f, c.y + depth + ry + 6.0f), 3.0f);
            }

        // корпус
        juce::Path body;
        body.startNewSubPath (c.x + rx, c.y);
        body.lineTo (c.x + rx, c.y + depth);
        body.addArc (c.x - rx, c.y + depth - ry, rx * 2.0f, ry * 2.0f, juce::MathConstants<float>::halfPi,
                     juce::MathConstants<float>::pi * 1.5f, false);
        body.lineTo (c.x - rx, c.y);
        body.closeSubPath();

        juce::ColourGradient shell (metal ? juce::Colour (0xFF8D96A8) : juce::Colour (0xFF1B2232), c.x - rx, c.y, metal ? juce::Colour (0xFF5C6577) : juce::Colour (0xFF0C1018), c.x + rx, c.y, false);
        shell.addColour (0.30, metal ? juce::Colour (0xFFE6EAF2) : juce::Colour (0xFF39455E));
        shell.addColour (0.62, metal ? juce::Colour (0xFFA7B0C1) : juce::Colour (0xFF1A2132));
        g.setGradientFill (shell);
        g.fillPath (body);

        // болты (натяжные)
        const int lugs = metal ? 7 : 6;
        for (int i = 0; i < lugs; ++i)
        {
            const float th = juce::MathConstants<float>::pi * (0.12f + 0.76f * (float) i / (float) (lugs - 1));
            const float lx = c.x + rx * std::cos (th) * 0.98f;
            const float ly = c.y + ry * std::sin (th) + depth * 0.40f;
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xFFEFF2F8), ly - 5.0f, juce::Colour (0xFF7C8598), ly + 5.0f));
            g.fillRoundedRectangle (lx - 2.6f, ly - 5.0f, 5.2f, 10.0f, 1.6f);
        }

        // обод
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFFF4F6FA), c.x - rx, c.y - ry, juce::Colour (0xFF7A8396), c.x + rx, c.y + ry, false));
        g.fillEllipse (c.x - rx, c.y - ry, rx * 2.0f, ry * 2.0f);
        g.setColour (juce::Colour (0xFF4A5264));
        g.drawEllipse (c.x - rx + 2.5f, c.y - ry + 2.5f, rx * 2.0f - 5.0f, ry * 2.0f - 5.0f, 1.0f);

        // пластик
        const float hx = rx * 0.90f, hy = ry * 0.88f;
        juce::ColourGradient head (juce::Colour (0xFFFFFFFF), c.x - hx * 0.25f, c.y - hy * 0.35f, juce::Colour (0xFFC9CED9), c.x + hx, c.y + hy, true);
        g.setGradientFill (head);
        g.fillEllipse (c.x - hx, c.y - hy, hx * 2.0f, hy * 2.0f);
        g.setColour (juce::Colour (0x2E000000));
        g.drawEllipse (c.x - hx * 0.62f, c.y - hy * 0.62f, hx * 1.24f, hy * 1.24f, 1.0f);

        if (metal)   // рычаг подструнника
        {
            g.setColour (juce::Colour (0xFF6B7487));
            g.fillRoundedRectangle (c.x + rx - 3.0f, c.y + ry * 0.2f, 12.0f, 6.0f, 2.0f);
        }

        if (f > 0.0f)
        {
            g.setColour (col::lane (drums::laneOf (a)).withAlpha (0.50f * f));
            g.fillEllipse (c.x - hx, c.y - hy, hx * 2.0f, hy * 2.0f);
        }
    }

    void kick (juce::Graphics& g, Pt c, float R, float f) const
    {
        halo (g, juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), A::Kick, f);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - R * 1.02f, c.y + R * 0.62f, R * 2.04f, R * 0.7f);

        // сошки
        for (float side : { -1.0f, 1.0f })
        {
            pole (g, Pt (c.x + side * R * 0.95f, c.y + R * 0.25f), Pt (c.x + side * (R + 26.0f), c.y + R * 0.92f), 4.0f);
            g.setColour (juce::Colour (0xFF39414F));
            g.fillRoundedRectangle (c.x + side * (R + 26.0f) - 7.0f, c.y + R * 0.92f - 2.0f, 14.0f, 5.0f, 2.0f);
        }

        // корпус
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF2B3550), c.x - R * 0.4f, c.y - R * 0.5f, juce::Colour (0xFF0A0D14), c.x + R, c.y + R, true));
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);

        // обод
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFFF4F6FA), c.x - R, c.y - R, juce::Colour (0xFF6F7890), c.x + R, c.y + R, false));
        g.fillEllipse (c.x - R * 0.97f, c.y - R * 0.97f, R * 1.94f, R * 1.94f);

        // болты по кругу
        for (int i = 0; i < 12; ++i)
        {
            const float th = juce::MathConstants<float>::twoPi * (float) i / 12.0f;
            const Pt p (c.x + std::cos (th) * R * 0.955f, c.y + std::sin (th) * R * 0.955f);
            g.setColour (juce::Colour (0xFF39414F));
            g.fillEllipse (p.x - 2.6f, p.y - 2.6f, 5.2f, 5.2f);
            g.setColour (juce::Colour (0xFFD5DAE4));
            g.fillEllipse (p.x - 1.4f, p.y - 1.4f, 2.8f, 2.8f);
        }

        // пластик
        const float hr = R * 0.86f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xFFFFFFFF), c.x - hr * 0.3f, c.y - hr * 0.4f, juce::Colour (0xFFC3C9D6), c.x + hr, c.y + hr, true));
        g.fillEllipse (c.x - hr, c.y - hr, hr * 2.0f, hr * 2.0f);
        g.setColour (juce::Colour (0x26000000));
        g.drawEllipse (c.x - hr * 0.72f, c.y - hr * 0.72f, hr * 1.44f, hr * 1.44f, 1.0f);

        // логотип
        g.setColour (juce::Colour (0xFF1A2132));
        g.setFont (juce::Font (juce::FontOptions (26.0f, juce::Font::bold | juce::Font::italic)));
        g.drawText ("dr. Ums", juce::Rectangle<float> (c.x - hr, c.y - 16.0f, hr * 2.0f, 32.0f), juce::Justification::centred, false);
        g.setColour (col::accent);
        g.fillRoundedRectangle (c.x - hr * 0.38f, c.y + 17.0f, hr * 0.76f, 3.0f, 1.5f);

        if (f > 0.0f)
        {
            g.setColour (col::lane (drums::Lane::Kick).withAlpha (0.50f * f));
            g.fillEllipse (c.x - hr, c.y - hr, hr * 2.0f, hr * 2.0f);
        }

        // педаль
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xFF6B7487), c.y + R - 4.0f, juce::Colour (0xFF39414F), c.y + R + 22.0f));
        g.fillRoundedRectangle (c.x - 15.0f, c.y + R - 6.0f, 30.0f, 28.0f, 5.0f);
        g.setColour (juce::Colour (0xFF9AA3B5));
        g.fillRoundedRectangle (c.x - 12.0f, c.y + R + 2.0f, 24.0f, 5.0f, 2.0f);
    }

    void drawKit (juce::Graphics& g) const
    {
        // стойки
        pole (g, Pt (92.0f, 70.0f),  Pt (104.0f, 338.0f)); tripod (g, Pt (104.0f, 338.0f), 40.0f);
        pole (g, Pt (205.0f, 50.0f), Pt (214.0f, 300.0f));
        pole (g, Pt (300.0f, 40.0f), Pt (300.0f, 150.0f));
        pole (g, Pt (372.0f, 82.0f), Pt (372.0f, 160.0f));
        pole (g, Pt (484.0f, 58.0f), Pt (486.0f, 330.0f)); tripod (g, Pt (486.0f, 330.0f), 40.0f);
        pole (g, Pt (528.0f, 140.0f), Pt (532.0f, 336.0f)); tripod (g, Pt (532.0f, 336.0f), 38.0f);
        pole (g, Pt (52.0f, 186.0f), Pt (52.0f, 348.0f), 3.0f); tripod (g, Pt (52.0f, 348.0f), 34.0f);

        // тарелки
        cymbal (g, Pt (92.0f, 66.0f),   60.0f, 22.0f, A::CrashFarLeft, fl (A::CrashFarLeft));
        cymbal (g, Pt (205.0f, 46.0f),  54.0f, 19.0f, A::CrashLeft,    fl (A::CrashLeft));
        cymbal (g, Pt (300.0f, 34.0f),  27.0f, 9.0f,  A::Splash,       fl (A::Splash), true);
        cymbal (g, Pt (372.0f, 80.0f),  38.0f, 14.0f, A::China,        fl (A::China), true);
        cymbal (g, Pt (484.0f, 54.0f),  62.0f, 22.0f, A::CrashRight,   fl (A::CrashRight));
        cymbal (g, Pt (528.0f, 134.0f), 58.0f, 21.0f, A::RideBow,      flAny ({ A::RideBow, A::RideBell }));

        // стойка рэк-томов
        pole (g, Pt (293.0f, 150.0f), Pt (293.0f, 215.0f), 5.0f);
        pole (g, Pt (236.0f, 142.0f), Pt (350.0f, 142.0f), 4.0f);

        // тома
        drum (g, Pt (228.0f, 150.0f), 52.0f, 34.0f, 38.0f, A::TomRack1, fl (A::TomRack1), false, false);
        drum (g, Pt (358.0f, 150.0f), 52.0f, 34.0f, 38.0f, A::TomRack2, fl (A::TomRack2), false, false);
        drum (g, Pt (524.0f, 262.0f), 46.0f, 30.0f, 52.0f, A::TomFloor2, fl (A::TomFloor2), false, true);
        drum (g, Pt (436.0f, 268.0f), 58.0f, 38.0f, 56.0f, A::TomFloor1, fl (A::TomFloor1), false, true);

        // бочка
        kick (g, Pt (293.0f, 268.0f), 80.0f, fl (A::Kick));

        // снейр на стойке
        tripod (g, Pt (140.0f, 290.0f), 38.0f);
        pole (g, Pt (140.0f, 276.0f), Pt (140.0f, 290.0f), 4.0f);
        drum (g, Pt (140.0f, 240.0f), 52.0f, 32.0f, 28.0f, A::Snare, flAny ({ A::Snare, A::SnareCross }), true, false);

        // хай-хэт
        const float hat = flAny ({ A::HatClosedTip, A::HatClosedEdge, A::HatOpen1, A::HatOpen2, A::HatOpen3, A::HatPedal });
        cymbal (g, Pt (52.0f, 190.0f), 44.0f, 13.0f, A::HatClosedTip, hat, true);
        cymbal (g, Pt (52.0f, 178.0f), 44.0f, 13.0f, A::HatClosedTip, hat);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xFF6B7487), 338.0f, juce::Colour (0xFF39414F), 366.0f));
        g.fillRoundedRectangle (30.0f, 342.0f, 46.0f, 20.0f, 4.0f);
    }

    std::array<float, (size_t) drums::kNumArts> flashes {};
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

        // строки: сверху тарелки, потом тома, снейр, кик (как в колонке слева)
        static const drums::Lane order[4] = { drums::Lane::Cymbals, drums::Lane::Toms, drums::Lane::Snare, drums::Lane::Kick };
        int rowOf[drums::kNumArts];
        for (int& r : rowOf)
            r = -1;

        bool used[drums::kNumArts] = {};
        for (const auto& h : p.hits)
            if (map.noteFor (h.art) >= 0)
                used[(int) h.art] = true;

        int rows = 0;
        std::vector<int> laneEndRow;
        for (auto lane : order)
        {
            for (int a = 0; a < drums::kNumArts; ++a)
                if (used[a] && drums::laneOf ((drums::Art) a) == lane)
                    rowOf[a] = rows++;
            laneEndRow.push_back (rows);
        }
        rows = juce::jmax (1, rows);

        const float rowH = juce::jmin (9.0f, (H - 12.0f) / (float) rows);
        const float top = (H - rowH * (float) rows) * 0.5f;
        const int spb = p.stepsPerBar();
        const double bpb = p.beatsPerBar();
        const double playBeat = proc.getPlayBeat();

        for (int b = 0; b < bars; ++b)
        {
            const juce::Rectangle<float> tile ((float) b * (tileW + gap), 0.0f, tileW, H);

            g.setColour (col::panel);
            g.fillRoundedRectangle (tile, 10.0f);

            g.saveState();
            juce::Path clip;
            clip.addRoundedRectangle (tile, 10.0f);
            g.reduceClipRegion (clip, juce::AffineTransform());

            // сетка по долям
            for (int k = 1; k < (int) std::ceil (bpb); ++k)
            {
                g.setColour (col::line.withAlpha (0.9f));
                g.fillRect (tile.getX() + tile.getWidth() * (float) ((double) k / bpb) - 0.5f, 0.0f, 1.0f, H);
            }

            // разделители дорожек
            for (size_t i = 0; i + 1 < laneEndRow.size(); ++i)
                if (laneEndRow[i] > 0 && laneEndRow[i] < rows)
                {
                    g.setColour (col::line.withAlpha (0.55f));
                    g.fillRect (tile.getX(), top + rowH * (float) laneEndRow[i] - 0.5f, tile.getWidth(), 1.0f);
                }

            const float noteW = juce::jmax (2.5f, tile.getWidth() / (float) spb * 0.78f);
            for (const auto& h : p.hits)
            {
                if (h.step / spb != b)
                    continue;
                const int row = rowOf[(int) h.art];
                if (row < 0)
                    continue;

                const float x = tile.getX() + tile.getWidth() * (float) (h.step % spb) / (float) spb;
                const float y = top + (float) row * rowH;
                g.setColour (col::lane (drums::laneOf (h.art)).withAlpha (0.38f + 0.62f * (float) h.velocity / 127.0f));
                g.fillRoundedRectangle (x, y + 0.5f, noteW, juce::jmax (2.0f, rowH - 1.0f), 1.5f);
            }

            g.setColour (col::dim.withAlpha (0.55f));
            g.setFont (boldFont (9.0f));
            g.drawText (juce::String (b + 1), juce::Rectangle<float> (tile.getX() + 7.0f, tile.getY() + 2.0f, 16.0f, 11.0f),
                        juce::Justification::centredLeft, false);

            if (playBeat >= 0.0 && (int) (playBeat / bpb) == b)
            {
                const double inBar = playBeat - (double) b * bpb;
                const float x = tile.getX() + tile.getWidth() * (float) (inBar / bpb);
                g.setColour (juce::Colours::white.withAlpha (0.18f));
                g.fillRect (x - 3.0f, 0.0f, 6.0f, H);
                g.setColour (juce::Colours::white);
                g.fillRect (x - 1.0f, 0.0f, 2.0f, H);
            }

            g.restoreState();

            g.setColour (col::line);
            g.drawRoundedRectangle (tile.reduced (0.5f), 10.0f, 1.0f);
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
        table.setRowHeight (30);
        table.setMultipleSelectionEnabled (false);
        table.setOutlineThickness (0);
        table.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        table.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);

        auto& h = table.getHeader();
        h.setColour (juce::TableHeaderComponent::backgroundColourId, col::panelHi);
        h.setColour (juce::TableHeaderComponent::textColourId, col::dim);
        h.setColour (juce::TableHeaderComponent::outlineColourId, col::line);
        h.setColour (juce::TableHeaderComponent::highlightColourId, col::accent.withAlpha (0.35f));
        h.setStretchToFitActive (true);

        const int colFlags = juce::TableHeaderComponent::visible | juce::TableHeaderComponent::resizable
                           | juce::TableHeaderComponent::sortable;
        h.addColumn (ru ("Стиль"), 1, 120, 60, 300, colFlags);
        h.addColumn (ru ("Часть песни"), 2, 120, 60, 300, colFlags);
        h.addColumn (ru ("Название паттерна"), 3, 250, 100, 500, colFlags);
        h.addColumn (ru ("Размер"), 4, 80, 50, 200, colFlags);
        h.addColumn (ru ("Дата сохранения"), 5, 170, 80, 300, colFlags);
        h.setSortColumnId (5, false);   // по умолчанию — по дате, новые сверху
    }

    void refresh()
    {
        rebuildOrder();
        table.updateContent();
        table.repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 16.0f);
        g.setColour (col::line);
        g.drawRoundedRectangle (r, 16.0f, 1.0f);

        if (order.empty())
        {
            g.setColour (col::dim);
            g.setFont (plainFont (14.0f));
            g.drawFittedText (ru ("Здесь появятся паттерны, сохранённые сердечком на вкладке EDIT"),
                              getLocalBounds().withTrimmedTop (80).reduced (40, 0),
                              juce::Justification::centredTop, 2);
        }
    }

    void resized() override { table.setBounds (getLocalBounds().reduced (10)); }

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

    void paintRowBackground (juce::Graphics& g, int row, int w, int h, bool selected) override
    {
        if (selected)
        {
            g.setColour (col::accent.withAlpha (0.28f));
            g.fillRect (0, 0, w, h);
            g.setColour (col::accentHi);
            g.fillRect (0, 0, 3, h);
        }
        else if (row % 2 != 0)
        {
            g.setColour (juce::Colours::white.withAlpha (0.03f));
            g.fillRect (0, 0, w, h);
        }
    }

    void paintCell (juce::Graphics& g, int row, int colId, int w, int h, bool selected) override
    {
        const auto* e = entryAt (row);
        if (e == nullptr)
            return;

        juce::String text;
        switch (colId)
        {
            case 1: text = juce::String (drums::styleName (e->pattern.style)); break;
            case 2: text = juce::String (drums::partName (e->pattern.part)); break;
            case 3: text = e->name; break;
            case 4: text = e->pattern.timeSigText(); break;
            case 5: text = juce::Time (e->savedMs).formatted ("%d.%m.%Y  %H:%M"); break;
            default: break;
        }

        g.setColour (selected ? juce::Colours::white : (colId == 3 ? col::text : col::dim.brighter (0.25f)));
        g.setFont (plainFont (14.0f));
        g.drawText (text, 12, 0, w - 16, h, juce::Justification::centredLeft, true);
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
