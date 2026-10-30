#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <functional>
#include <map>
#include <numeric>
#include <BinaryData.h>
#include "PluginProcessor.h"

// Интерфейс dr. Ums: тёмная тема, фиолетовый акцент, у каждой дорожки свой цвет.
// Кубики и замки оставлены как в референсе (белые плитки с чёрной графикой).
namespace ui
{
inline juce::String ru (const char* utf8) { return juce::String::fromUTF8 (utf8); }
inline juce::String tr (const char* ru, const char* en) { return loc::tr (ru, en); }

// Названия частей песни для интерфейса (в именах файлов остаются английские).
inline juce::String partLabel (drums::Part p)
{
    switch (p)
    {
        case drums::Part::Intro:     return tr ("интро", "intro");
        case drums::Part::Verse:     return tr ("куплет", "verse");
        case drums::Part::Bridge:    return tr ("бридж", "bridge");
        case drums::Part::Chorus:    return tr ("припев", "chorus");
        case drums::Part::Fill:      return tr ("филл", "fill");
        case drums::Part::Breakdown: return tr ("брейкдаун", "breakdown");
    }
    return {};
}

// Уменьшение картинки вдвое по шагам: билинейная выборка точно посередине между пикселями
// даёт усреднение 2x2, поэтому тонкие линии не рассыпаются при сильном уменьшении.
inline juce::Image downscaled (juce::Image src, int w, int h)
{
    while (src.getWidth() >= w * 2 && src.getHeight() >= h * 2)
        src = src.rescaled (src.getWidth() / 2, src.getHeight() / 2, juce::Graphics::mediumResamplingQuality);
    return src.rescaled (w, h, juce::Graphics::highResamplingQuality);
}

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

// Логотип-крыса (Resources/rat.png) хранится как маска; цвет задаётся кистью.
inline const juce::Image& ratImage()
{
    static const juce::Image img = juce::ImageCache::getFromMemory (BinaryData::rat_png, BinaryData::rat_pngSize);
    return img;
}

// Кнопка блокировки: не нажата — чёрная крыса на белом, нажата — белая крыса на чёрном.
inline void drawLock (juce::Graphics& g, juce::Rectangle<float> r, bool locked, juce::Colour tint)
{
    const auto bg = locked ? juce::Colour (0xFF0B0D12) : juce::Colours::white;
    const auto fg = locked ? juce::Colours::white : juce::Colours::black;
    const auto edge = locked ? tint : juce::Colours::black;

    g.setColour (bg);
    g.fillRoundedRectangle (r.reduced (1.5f), 10.0f);
    g.setColour (edge);
    g.drawRoundedRectangle (r.reduced (1.5f), 10.0f, 3.0f);

    const auto& rat = ratImage();
    if (rat.isValid())
    {
        const auto area = r.reduced (r.getWidth() * 0.10f);
        const float px = g.getInternalContext().getPhysicalPixelScaleFactor();
        const int side = juce::jmax (8, juce::roundToInt (juce::jmin (area.getWidth(), area.getHeight()) * px));

        static std::map<int, juce::Image> cache;      // размер в пикселях -> готовая маска
        auto it = cache.find (side);
        if (it == cache.end())
        {
            if (cache.size() > 8)
                cache.clear();
            it = cache.emplace (side, downscaled (rat, side, side)).first;
        }

        g.setColour (fg);
        g.drawImage (it->second, area, juce::RectanglePlacement::centred, true);
    }
}

inline void drawHeart (juce::Graphics& g, juce::Rectangle<float> r, bool over)
{
    // строго круглая кнопка: берём квадрат по центру области
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    const auto circle = juce::Rectangle<float> (d, d).withCentre (r.getCentre());

    g.setColour (col::heart.withAlpha (over ? 0.34f : 0.20f));
    g.fillEllipse (circle.expanded (3.0f));
    g.setGradientFill (juce::ColourGradient::vertical (col::heart.brighter (0.2f), circle.getY(),
                                                       col::heart.darker (0.2f), circle.getBottom()));
    g.fillEllipse (circle);

    // сердце по параметрической кривой: симметрично относительно вертикальной оси
    juce::Path h;
    const int n = 160;
    for (int i = 0; i < n; ++i)
    {
        const float t = juce::MathConstants<float>::twoPi * (float) i / (float) n;
        const float x = 16.0f * std::pow (std::sin (t), 3.0f);
        const float y = -(13.0f * std::cos (t) - 5.0f * std::cos (2.0f * t) - 2.0f * std::cos (3.0f * t) - std::cos (4.0f * t));
        if (i == 0)
            h.startNewSubPath (x, y);
        else
            h.lineTo (x, y);
    }
    h.closeSubPath();

    const auto box = circle.reduced (d * 0.215f).translated (0.0f, -d * 0.01f);
    h.scaleToFit (box.getX(), box.getY(), box.getWidth(), box.getHeight(), true);
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
    void setCaption (const juce::String& c) { if (caption != c) { caption = c; repaint(); } }

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

    void setCaption (const juce::String& c) { if (caption != c) { caption = c; repaint(); } }

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

// ------------------------------------------------------------------ барабанная установка (фото)
// Картинка лежит в Resources/kit.png и вшивается в плагин. Во время Play поверх неё
// полупрозрачно подсвечиваются те барабаны и тарелки, которые сейчас играют.
class KitView : public juce::Component
{
public:
    KitView()
    {
        image = juce::ImageCache::getFromMemory (BinaryData::kit_png, BinaryData::kit_pngSize);
    }

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

        juce::Path panel;
        panel.addRoundedRectangle (b.reduced (0.5f), 18.0f);
        g.setColour (juce::Colour (0xFF0D0D10));
        g.fillPath (panel);

        if (image.isValid())
        {
            // картинка вписывается с обрезкой по краям, чтобы не было полос
            const float iw = (float) image.getWidth(), ih = (float) image.getHeight();
            const float s = std::max (b.getWidth() / iw, b.getHeight() / ih);
            const float dw = iw * s, dh = ih * s;
            const float ox = (b.getWidth() - dw) * 0.5f, oy = (b.getHeight() - dh) * 0.5f;

            // масштаб под реальные пиксели экрана, чтобы картинка не мылилась
            const float px = g.getInternalContext().getPhysicalPixelScaleFactor();
            const int tw = juce::jmax (1, juce::roundToInt (dw * px));
            const int th = juce::jmax (1, juce::roundToInt (dh * px));
            if (! scaled.isValid() || scaled.getWidth() != tw || scaled.getHeight() != th)
                scaled = downscaled (image, tw, th);

            g.saveState();
            g.reduceClipRegion (panel, juce::AffineTransform());
            g.drawImage (scaled, juce::Rectangle<float> (ox, oy, dw, dh));

            g.addTransform (juce::AffineTransform::scale (s).translated (ox, oy));
            drawHighlights (g);
            g.restoreState();
        }

        g.setColour (col::line);
        g.drawRoundedRectangle (b.reduced (0.5f), 18.0f, 1.0f);
    }

private:
    using A = drums::Art;

    // Области элементов в координатах картинки (1104 x 695).
    struct Ell { float cx, cy, rx, ry; };

    static constexpr Ell kCrashFarLeft { 178.0f, 78.0f, 134.0f, 40.0f };
    static constexpr Ell kCrashLeft    { 390.0f, 41.0f, 104.0f, 31.0f };
    static constexpr Ell kSplash       { 558.0f, 27.0f, 55.0f, 16.0f };
    static constexpr Ell kChina        { 684.0f, 103.0f, 66.0f, 21.0f };
    static constexpr Ell kCrashRight   { 884.0f, 65.0f, 129.0f, 39.0f };
    static constexpr Ell kRide         { 978.0f, 200.0f, 116.0f, 37.0f };
    static constexpr Ell kHat          { 108.0f, 280.0f, 112.0f, 38.0f };
    static constexpr Ell kKick         { 545.0f, 493.0f, 147.0f, 133.0f };

    float fl (A a) const { return flashes[(size_t) a]; }

    float flAny (std::initializer_list<A> l) const
    {
        float m = 0.0f;
        for (auto a : l)
            m = juce::jmax (m, fl (a));
        return m;
    }

    static juce::Rectangle<float> box (Ell e) { return { e.cx - e.rx, e.cy - e.ry, e.rx * 2.0f, e.ry * 2.0f }; }

    // Мягкое свечение вокруг элемента: эллиптический радиальный градиент без резких краёв.
    static void halo (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c, float f)
    {
        const auto m = b.getCentre();
        const float R = b.getWidth() * 0.72f;
        const float ratio = juce::jlimit (0.3f, 1.0f, b.getHeight() / b.getWidth());

        g.saveState();
        g.addTransform (juce::AffineTransform::scale (1.0f, ratio, m.x, m.y));
        g.setGradientFill (juce::ColourGradient (c.withAlpha (0.50f * f), m.x, m.y,
                                                 c.withAlpha (0.0f), m.x + R, m.y, true));
        g.fillEllipse (m.x - R, m.y - R, R * 2.0f, R * 2.0f);
        g.restoreState();
    }

    static void glowDisc (juce::Graphics& g, Ell e, A art, float f)
    {
        if (f <= 0.0f)
            return;
        const auto c = col::lane (drums::laneOf (art));
        halo (g, box (e), c, f);
        g.setColour (c.withAlpha (0.42f * f));
        g.fillEllipse (box (e));
    }

    // Барабан: пластик сверху и корпус под ним.
    static void glowDrum (juce::Graphics& g, Ell head, float depth, A art, float f)
    {
        if (f <= 0.0f)
            return;
        const auto c = col::lane (drums::laneOf (art));
        halo (g, juce::Rectangle<float> (head.rx * 2.0f, head.ry * 2.0f + depth).withCentre ({ head.cx, head.cy + depth * 0.5f }), c, f);

        juce::Path p;
        p.startNewSubPath (head.cx + head.rx, head.cy);
        p.lineTo (head.cx + head.rx, head.cy + depth);
        p.addArc (head.cx - head.rx, head.cy + depth - head.ry, head.rx * 2.0f, head.ry * 2.0f,
                  juce::MathConstants<float>::halfPi, juce::MathConstants<float>::pi * 1.5f, false);
        p.lineTo (head.cx - head.rx, head.cy);
        p.closeSubPath();
        p.addEllipse (box (head));

        g.setGradientFill (juce::ColourGradient::vertical (c.withAlpha (0.44f * f), head.cy - head.ry,
                                                           c.withAlpha (0.24f * f), head.cy + depth + head.ry));
        g.fillPath (p);
        g.setColour (c.withAlpha (0.22f * f));
        g.fillEllipse (box (head));
    }

    void drawHighlights (juce::Graphics& g) const
    {
        glowDisc (g, kCrashFarLeft, A::CrashFarLeft, fl (A::CrashFarLeft));
        glowDisc (g, kCrashLeft,    A::CrashLeft,    fl (A::CrashLeft));
        glowDisc (g, kSplash,       A::Splash,       fl (A::Splash));
        glowDisc (g, kChina,        A::China,        fl (A::China));
        glowDisc (g, kCrashRight,   A::CrashRight,   fl (A::CrashRight));
        glowDisc (g, kRide,         A::RideBow,      flAny ({ A::RideBow, A::RideBell }));
        glowDisc (g, kHat,          A::HatClosedTip, flAny ({ A::HatClosedTip, A::HatClosedEdge, A::HatOpen1,
                                                              A::HatOpen2, A::HatOpen3, A::HatPedal }));

        glowDrum (g, { 420.0f, 210.0f, 92.0f, 38.0f },  140.0f, A::TomRack1,  fl (A::TomRack1));
        glowDrum (g, { 669.0f, 212.0f, 97.0f, 40.0f },  140.0f, A::TomRack2,  fl (A::TomRack2));
        glowDrum (g, { 275.0f, 352.0f, 108.0f, 34.0f }, 118.0f, A::Snare,     flAny ({ A::Snare, A::SnareCross }));
        glowDrum (g, { 985.0f, 404.0f, 78.0f, 31.0f },  205.0f, A::TomFloor2, fl (A::TomFloor2));
        glowDrum (g, { 806.0f, 380.0f, 103.0f, 37.0f }, 230.0f, A::TomFloor1, fl (A::TomFloor1));

        glowDisc (g, kKick, A::Kick, fl (A::Kick));
    }

    juce::Image image, scaled;
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
        h.addColumn (tr ("Стиль", "Style"), 1, 120, 60, 300, colFlags);
        h.addColumn (tr ("Часть песни", "Song part"), 2, 120, 60, 300, colFlags);
        h.addColumn (tr ("Название паттерна", "Pattern name"), 3, 250, 100, 500, colFlags);
        h.addColumn (tr ("Размер", "Time sig."), 4, 80, 50, 200, colFlags);
        h.addColumn (tr ("Дата сохранения", "Saved"), 5, 170, 80, 300, colFlags);
        h.setSortColumnId (5, false);   // по умолчанию — по дате, новые сверху
    }

    void refresh()
    {
        rebuildOrder();
        table.updateContent();
        table.repaint();
    }

    void refreshLanguage()
    {
        auto& h = table.getHeader();
        h.setColumnName (1, tr ("Стиль", "Style"));
        h.setColumnName (2, tr ("Часть песни", "Song part"));
        h.setColumnName (3, tr ("Название паттерна", "Pattern name"));
        h.setColumnName (4, tr ("Размер", "Time sig."));
        h.setColumnName (5, tr ("Дата сохранения", "Saved"));
        table.repaint();
        repaint();
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
            g.drawFittedText (tr ("Здесь появятся паттерны, сохранённые сердечком на вкладке EDIT",
                                  "Patterns saved with the heart button on the EDIT tab will appear here"),
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
            case 2: text = partLabel (e->pattern.part); break;
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
