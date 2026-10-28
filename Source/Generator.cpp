#include "Generator.h"

namespace drums
{
namespace
{
    // ---------------------------------------------------------------- сетка такта
    struct Grid
    {
        int spb = 16;                       // шагов (1/16) в такте
        std::vector<int> starts, lens;      // группы долей: начало и длина в шагах

        int groups() const { return (int) starts.size(); }
        bool isStart (int s) const { return std::find (starts.begin(), starts.end(), s) != starts.end(); }
    };

    // Делит такт на группы долей: 4/4 -> 4x4 шага, 7/8 -> 2+2+3 восьмых и т.д.
    Grid makeGrid (int num, int den)
    {
        Grid g;
        g.spb = num * 16 / den;

        std::vector<int> lens;
        if (den == 4)
            lens.assign ((size_t) num, 4);
        else if (den == 2)
            lens.assign ((size_t) num * 2, 4);
        else
        {
            const int unit = 16 / den;      // шагов в одной доле знаменателя
            std::vector<int> u;
            if (den == 16 && num % 4 == 0)  u.assign ((size_t) (num / 4), 4);
            else if (num < 3)               u.push_back (num);
            else if (num % 3 == 0)          u.assign ((size_t) (num / 3), 3);
            else if (num % 2 == 0)          u.assign ((size_t) (num / 2), 2);
            else
            {
                u.assign ((size_t) ((num - 3) / 2), 2);
                u.push_back (3);
            }
            for (int x : u)
                lens.push_back (x * unit);
        }

        int s = 0;
        for (int l : lens)
        {
            g.starts.push_back (s);
            g.lens.push_back (l);
            s += l;
        }
        return g;
    }

    // ---------------------------------------------------------------- профиль стиля/части
    enum class KickMode  { Chug, Gallop, Rock, TwoStep };
    enum class SnareMode { BackBeat, HalfTime };
    enum class CymMode   { HatQuarters, HatEighths, HatSixteenths, RideQuarters, RideEighths };

    struct Prof
    {
        KickMode kick = KickMode::Rock;
        float kickKeep = 1.0f;          // шанс оставить запланированный кик (разреженные части)
        float kickDouble = 0.0f;        // шанс добавить второй кик на следующей 1/16
        SnareMode snare = SnareMode::BackBeat;
        float ghost = 0.0f;             // призрачные ноты снейра
        float cross = 0.0f;             // шанс заменить снейр на рим-клик
        CymMode cym = CymMode::HatEighths;
        float openHat = 0.0f;
        float crashOn1 = 0.0f;          // краш/чайна на первой доле
        Art crashArt = Art::CrashLeft;
        float accentOnKick = 0.0f;      // чайна в унисон с кик-ударами (брейкдауны)
        float fill = 0.0f;              // шанс филла в последнем такте
        bool fillPart = false;          // часть "Fill": филлы в 3-м и 4-м тактах
        bool tomGroove = false;         // грув на томах
    };

    Prof makeProfile (Style style, Part part)
    {
        Prof f;
        switch (style)
        {
            case Style::Djent:
                f.kick = KickMode::Chug;   f.kickDouble = 0.12f; f.snare = SnareMode::HalfTime;
                f.cym = CymMode::HatEighths; f.openHat = 0.12f;  f.fill = 0.30f;
                break;
            case Style::Metalcore:
                f.kick = KickMode::Gallop; f.kickDouble = 0.30f; f.snare = SnareMode::BackBeat;
                f.cym = CymMode::HatEighths; f.openHat = 0.10f;  f.fill = 0.40f;
                break;
            case Style::IndieRock:
                f.kick = KickMode::Rock;   f.snare = SnareMode::BackBeat; f.ghost = 0.12f;
                f.cym = CymMode::HatEighths; f.openHat = 0.05f;  f.fill = 0.25f;
                break;
            case Style::DrumAndBass:
                f.kick = KickMode::TwoStep; f.snare = SnareMode::BackBeat; f.ghost = 0.35f;
                f.cym = CymMode::HatEighths; f.openHat = 0.20f;  f.fill = 0.30f;
                break;
        }

        const bool metal = style == Style::Djent || style == Style::Metalcore;

        switch (part)
        {
            case Part::Intro:
                f.kickKeep = 0.5f;  f.kickDouble = 0.0f;
                f.cym = CymMode::HatQuarters;  f.openHat = 0.0f;
                f.snare = SnareMode::BackBeat; f.cross = 0.8f;
                f.ghost *= 0.3f;    f.fill = 0.10f;
                break;

            case Part::Verse:
                f.fill *= 0.7f;
                if (style == Style::IndieRock) f.cross = 0.3f;
                break;

            case Part::Bridge:
                f.kickKeep = 0.6f;  f.kickDouble *= 0.3f;
                f.snare = SnareMode::HalfTime;
                f.cym = style == Style::DrumAndBass ? CymMode::RideEighths : CymMode::RideQuarters;
                f.openHat = 0.0f;   f.ghost += 0.10f;
                f.cross = style == Style::IndieRock ? 0.8f : 0.0f;
                f.fill = 0.45f;
                break;

            case Part::Chorus:
                f.kickDouble *= 1.3f; f.snare = SnareMode::BackBeat;
                f.crashOn1 = 1.0f;    f.fill = 0.5f;
                f.cym = style == Style::DrumAndBass ? CymMode::HatSixteenths : CymMode::RideEighths;
                if (style == Style::IndieRock) { f.cym = CymMode::HatEighths; f.openHat = 0.5f; }
                break;

            case Part::Fill:
                f.fill = 1.0f;  f.fillPart = true;
                break;

            case Part::Breakdown:
                f.snare = SnareMode::HalfTime;
                f.cym = CymMode::HatQuarters;  f.openHat = 0.0f;
                f.crashOn1 = 1.0f;  f.fill = 0.3f;  f.ghost = 0.0f;
                if (style == Style::Djent)       { f.kickDouble = 0.35f; f.accentOnKick = 0.30f; }
                if (style == Style::Metalcore)   { f.kick = KickMode::Chug; f.kickDouble = 0.30f; f.accentOnKick = 0.25f; }
                if (style == Style::IndieRock)   { f.tomGroove = true; f.kickKeep = 0.7f; }
                if (style == Style::DrumAndBass) { f.ghost = 0.20f; }
                break;
        }

        f.crashArt = (part == Part::Breakdown && metal) ? Art::China : Art::CrashLeft;
        return f;
    }

    // ---------------------------------------------------------------- вспомогательное
    int jit (juce::Random& rng, int v, int spread)
    {
        return juce::jlimit (1, 127, v + rng.nextInt (spread * 2 + 1) - spread);
    }

    bool hasHit (const std::vector<Hit>& v, int step, Lane lane)
    {
        for (const auto& h : v)
            if (h.step == step && laneOf (h.art) == lane)
                return true;
        return false;
    }

    void eraseHits (std::vector<Hit>& v, int step, Lane lane)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [&] (const Hit& h)
                                 { return h.step == step && laneOf (h.art) == lane; }),
                 v.end());
    }

    // Случайное деление такта на куски по 2-4 шага (с уклоном в тройки) — "чаг" джента.
    std::vector<int> randomSplit (int total, juce::Random& rng)
    {
        std::vector<int> pos;
        int s = 0;
        while (s < total)
        {
            pos.push_back (s);
            const float r = rng.nextFloat();
            s += r < 0.15f ? 2 : (r < 0.65f ? 3 : 4);
        }
        return pos;
    }

    std::vector<int> kickSteps (const Prof& f, const Grid& g, juce::Random& rng)
    {
        const int spb = g.spb;
        std::vector<int> out;

        switch (f.kick)
        {
            case KickMode::Chug:
                out = randomSplit (spb, rng);
                break;

            case KickMode::Gallop:
                for (int s = 0; s < spb; s += 2)
                    if (s == 0 || rng.nextFloat() < 0.85f)
                        out.push_back (s);
                break;

            case KickMode::Rock:
                for (int i = 0; i < g.groups(); ++i)
                {
                    const int st = g.starts[(size_t) i];
                    if (i == 0 || (i % 2 == 0 && rng.nextFloat() < 0.9f))
                        out.push_back (st);

                    const int off = st + g.lens[(size_t) i] / 2;
                    if (off < spb && rng.nextFloat() < 0.30f)
                        out.push_back (off);
                }
                break;

            case KickMode::TwoStep:
                out.push_back (0);
                out.push_back ((spb * 5) / 8);
                if (rng.nextFloat() < 0.35f) out.push_back ((spb * 3) / 16);
                if (rng.nextFloat() < 0.20f) out.push_back ((spb * 7) / 16);
                break;
        }

        std::vector<int> res;
        for (int s : out)
            if (s == 0 || rng.nextFloat() < f.kickKeep)
                res.push_back (s);

        if (f.kickDouble > 0.0f)
        {
            const auto base = res;
            for (int s : base)
                if (s + 1 < spb && rng.nextFloat() < f.kickDouble)
                    res.push_back (s + 1);
        }

        std::sort (res.begin(), res.end());
        res.erase (std::unique (res.begin(), res.end()), res.end());
        return res;
    }

    std::vector<int> snareSteps (const Prof& f, const Grid& g)
    {
        std::vector<int> out;
        const int n = g.groups();

        if (f.snare == SnareMode::HalfTime)
            out.push_back (g.starts[(size_t) (n / 2)]);
        else
            for (int i = 1; i < n; i += 2)
                out.push_back (g.starts[(size_t) i]);

        if (out.empty())
            out.push_back (g.spb / 2);
        return out;
    }

    void addCymbals (std::vector<Hit>& bar, const Prof& f, const Grid& g, juce::Random& rng)
    {
        const int spb = g.spb;

        switch (f.cym)
        {
            case CymMode::HatQuarters:
                for (int s : g.starts)
                    bar.push_back ({ s, Art::HatClosedTip, jit (rng, s == 0 ? 105 : 92, 5) });
                break;

            case CymMode::HatEighths:
                for (int s = 0; s < spb; s += 2)
                {
                    const bool open = (s % 4 == 2) && rng.nextFloat() < f.openHat;
                    const bool accent = g.isStart (s);
                    Art a = Art::HatClosedTip;
                    if (open)                                       a = Art::HatOpen1;
                    else if (accent && rng.nextFloat() < 0.25f)     a = Art::HatClosedEdge;
                    bar.push_back ({ s, a, jit (rng, open ? 96 : (accent ? 100 : 80), 5) });
                }
                break;

            case CymMode::HatSixteenths:
                for (int s = 0; s < spb; ++s)
                {
                    const bool open = (s % 4 == 2) && rng.nextFloat() < f.openHat * 0.5f;
                    const int base = g.isStart (s) ? 104 : (s % 2 == 0 ? 88 : 62);
                    bar.push_back ({ s, open ? Art::HatOpen1 : Art::HatClosedTip, jit (rng, base, 5) });
                }
                break;

            case CymMode::RideQuarters:
                for (int s : g.starts)
                    bar.push_back ({ s, Art::RideBow, jit (rng, 96, 5) });
                break;

            case CymMode::RideEighths:
                for (int s = 0; s < spb; s += 2)
                {
                    const bool accent = g.isStart (s);
                    const bool bell = accent && rng.nextFloat() < 0.12f;
                    bar.push_back ({ s, bell ? Art::RideBell : Art::RideBow,
                                     jit (rng, bell ? 100 : (accent ? 102 : 84), 5) });
                }
                break;
        }
    }

    std::vector<Hit> makeBar (const Prof& f, const Grid& g, juce::Random& rng)
    {
        std::vector<Hit> bar;
        const int spb = g.spb;

        const auto kicks = kickSteps (f, g, rng);
        for (int s : kicks)
            bar.push_back ({ s, Art::Kick, jit (rng, s == 0 ? 114 : 102, 6) });

        const auto snares = snareSteps (f, g);
        for (int s : snares)
        {
            const bool cross = rng.nextFloat() < f.cross;
            const int base = cross ? 90 : (f.snare == SnareMode::HalfTime ? 120 : 112);
            bar.push_back ({ s, cross ? Art::SnareCross : Art::Snare, jit (rng, base, 5) });
        }

        if (f.ghost > 0.0f)
            for (int s : snares)
                for (int d : { -1, 2, 3 })
                {
                    const int gs = s + d;
                    if (gs < 0 || gs >= spb || hasHit (bar, gs, Lane::Snare))
                        continue;
                    if (rng.nextFloat() < f.ghost * 0.6f)
                        bar.push_back ({ gs, Art::Snare, jit (rng, 38, 8) });
                }

        addCymbals (bar, f, g, rng);

        if (f.accentOnKick > 0.0f)
            for (int s : kicks)
                if (s % 2 == 0 && rng.nextFloat() < f.accentOnKick)
                {
                    eraseHits (bar, s, Lane::Cymbals);
                    bar.push_back ({ s, Art::China, jit (rng, 112, 5) });
                }

        if (f.tomGroove)
            for (int s = 0; s < spb; s += 2)
            {
                const bool st = g.isStart (s);
                if (! st && rng.nextFloat() < 0.15f)
                    continue;
                bar.push_back ({ s, s % 4 == 0 ? Art::TomFloor1 : Art::TomFloor2, jit (rng, st ? 105 : 85, 5) });
            }

        return bar;
    }

    // Небольшие изменения в повторяющихся тактах, чтобы паттерн не звучал как копипаста.
    void mutateBar (std::vector<Hit>& bar, const Prof& f, const Grid& g, juce::Random& rng, int amount)
    {
        const int spb = g.spb;

        for (int i = 0; i < amount; ++i)
        {
            const float r = rng.nextFloat();

            if (r < 0.40f)
            {
                const int s = rng.nextInt (spb);
                if (s != 0 && ! hasHit (bar, s, Lane::Kick))
                    bar.push_back ({ s, Art::Kick, jit (rng, 96, 5) });
            }
            else if (r < 0.60f)
            {
                std::vector<size_t> idx;
                for (size_t k = 0; k < bar.size(); ++k)
                    if (bar[k].art == Art::Kick && bar[k].step != 0)
                        idx.push_back (k);
                if (! idx.empty())
                    bar.erase (bar.begin() + (std::ptrdiff_t) idx[(size_t) rng.nextInt ((int) idx.size())]);
            }
            else if (r < 0.80f)
            {
                for (auto& h : bar)
                    if (h.step == spb - 2 && h.art == Art::HatClosedTip)
                        h.art = Art::HatOpen1;
            }
            else if (f.ghost > 0.0f)
            {
                const int s = rng.nextInt (spb);
                if (! hasHit (bar, s, Lane::Snare))
                    bar.push_back ({ s, Art::Snare, jit (rng, 36, 6) });
            }
        }
    }

    // Филл: с позиции start всё заменяется на снейр + тома сверху вниз.
    void applyFill (std::vector<Hit>& bar, const Grid& g, int start, juce::Random& rng)
    {
        const int spb = g.spb;
        bar.erase (std::remove_if (bar.begin(), bar.end(), [&] (const Hit& h) { return h.step >= start; }),
                   bar.end());

        static const Art ladder[] = { Art::Snare, Art::TomRack1, Art::TomRack2, Art::TomFloor1, Art::TomFloor2 };
        const int sub = rng.nextBool() ? 1 : 2;

        std::vector<int> steps;
        for (int s = start; s < spb; s += sub)
            steps.push_back (s);

        const int n = (int) steps.size();
        for (int i = 0; i < n; ++i)
        {
            if (i != 0 && i != n - 1 && rng.nextFloat() < 0.2f)
                continue;
            const float prog = n > 1 ? (float) i / (float) (n - 1) : 0.0f;
            const int idx = juce::jlimit (0, 4, (int) (prog * 4.99f));
            bar.push_back ({ steps[(size_t) i], ladder[idx], jit (rng, 84 + (int) (prog * 36.0f), 4) });
        }

        if (! steps.empty() && rng.nextFloat() < 0.6f) bar.push_back ({ steps.front(), Art::Kick, jit (rng, 110, 5) });
        if (! steps.empty() && rng.nextFloat() < 0.5f) bar.push_back ({ steps.back(),  Art::Kick, jit (rng, 112, 5) });
    }
} // namespace

// -------------------------------------------------------------------- публичное API
void sortHits (std::vector<Hit>& hits)
{
    std::sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b)
               { return a.step != b.step ? a.step < b.step : (int) a.art < (int) b.art; });
    hits.erase (std::unique (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b)
                             { return a.step == b.step && a.art == b.art; }),
                hits.end());
}

Pattern generate (Style style, Part part, int num, int den, juce::Random& rng)
{
    if (! validTimeSig (num, den))
    {
        num = 4;
        den = 4;
    }

    Pattern p;
    p.numerator = num;
    p.denominator = den;
    p.style = style;
    p.part = part;
    p.bars = 4;

    const Grid g = makeGrid (num, den);
    const Prof f = makeProfile (style, part);
    const auto base = makeBar (f, g, rng);

    for (int bar = 0; bar < p.bars; ++bar)
    {
        auto hits = base;

        if (bar >= 2)
            mutateBar (hits, f, g, rng, bar == 3 ? 2 : 1);

        if ((bar == 0 || (bar == 2 && rng.nextBool())) && rng.nextFloat() < f.crashOn1)
        {
            eraseHits (hits, 0, Lane::Cymbals);
            hits.push_back ({ 0, f.crashArt, jit (rng, 120, 3) });
        }

        int fillStart = -1;
        if (f.fillPart)
        {
            if (bar == 2 && rng.nextFloat() < 0.6f)  fillStart = g.spb / 2;
            else if (bar == 3)                       fillStart = rng.nextBool() ? 0 : g.spb / 4;
        }
        else if (bar == 3 && rng.nextFloat() < f.fill)
        {
            fillStart = rng.nextFloat() < 0.7f ? g.spb / 2 : (g.spb * 3) / 4;
        }

        if (fillStart >= 0)
            applyFill (hits, g, fillStart, rng);

        for (const auto& h : hits)
            p.hits.push_back ({ bar * g.spb + h.step, h.art, h.velocity });
    }

    sortHits (p.hits);
    return p;
}

Pattern withLane (const Pattern& base, const Pattern& source, Lane lane)
{
    Pattern r = base;
    r.hits.erase (std::remove_if (r.hits.begin(), r.hits.end(),
                                  [&] (const Hit& h) { return laneOf (h.art) == lane; }),
                  r.hits.end());

    const int total = r.totalSteps();
    for (const auto& h : source.hits)
        if (laneOf (h.art) == lane && h.step < total)
            r.hits.push_back (h);

    sortHits (r.hits);
    return r;
}

Pattern mergeLocked (const Pattern& fresh, const Pattern& old, const std::array<bool, (size_t) kNumLanes>& locked)
{
    Pattern r = fresh;
    for (int l = 0; l < kNumLanes; ++l)
        if (locked[(size_t) l])
            r = withLane (r, old, (Lane) l);
    return r;
}
} // namespace drums
