#include "Model.h"

namespace drums
{
Style styleFromVar (const juce::var& v)
{
    if (v.isString())
    {
        const auto name = v.toString();
        for (int i = 0; i < kNumStyles; ++i)
            if (name == styleSlug ((Style) i))
                return (Style) i;
        return Style::Djent;
    }

    switch ((int) v)
    {
        case 0:  return Style::Djent;
        case 1:  return Style::Metalcore;
        case 2:  return Style::IndieRock;
        case 3:  return Style::IndieRock;
        case 4:  return Style::PostMetal;
        default: return Style::Djent;
    }
}

juce::var patternToVar (const Pattern& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("num", p.numerator);
    o->setProperty ("den", p.denominator);
    o->setProperty ("bars", p.bars);
    o->setProperty ("style", juce::String (styleSlug (p.style)));
    o->setProperty ("part", (int) p.part);

    juce::Array<juce::var> hits;
    for (const auto& h : p.hits)
    {
        juce::Array<juce::var> t;
        t.add (h.step);
        t.add ((int) h.art);
        t.add (h.velocity);
        hits.add (juce::var (t));
    }
    o->setProperty ("hits", hits);
    return juce::var (o);
}

Pattern patternFromVar (const juce::var& v)
{
    Pattern p;
    if (! v.isObject())
        return p;

    p.numerator   = (int) v.getProperty ("num", 4);
    p.denominator = (int) v.getProperty ("den", 4);
    if (! validTimeSig (p.numerator, p.denominator))
    {
        p.numerator = 4;
        p.denominator = 4;
    }

    p.bars  = juce::jlimit (1, 8, (int) v.getProperty ("bars", 4));
    p.style = styleFromVar (v.getProperty ("style", juce::var (0)));
    p.part  = (Part)  juce::jlimit (0, kNumParts - 1,  (int) v.getProperty ("part", 1));

    const auto hitsVar = v.getProperty ("hits", juce::var());
    if (const auto* arr = hitsVar.getArray())
    {
        const int total = p.totalSteps();
        for (const auto& e : *arr)
        {
            const auto* t = e.getArray();
            if (t == nullptr || t->size() < 3)
                continue;

            Hit h;
            h.step = (int) (*t)[0];
            const int art = (int) (*t)[1];
            h.velocity = juce::jlimit (1, 127, (int) (*t)[2]);
            if (h.step < 0 || h.step >= total || art < 0 || art >= kNumArts)
                continue;
            h.art = (Art) art;
            p.hits.push_back (h);
        }
    }
    return p;
}
} // namespace drums
