#include "Generator.h"
#include "Corpus.h"
#include <cstring>

namespace drums
{
namespace
{
    using HitVec = std::vector<Hit>;
    using Lanes = std::array<HitVec, (size_t) kNumLanes>;

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
            const int unit = 16 / den;
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

    // ---------------------------------------------------------------- мелочи
    int jit (juce::Random& rng, int v, int spread = 5)
    {
        return juce::jlimit (1, 127, v + rng.nextInt (spread * 2 + 1) - spread);
    }

    template <typename T>
    T pickWeighted (const std::vector<std::pair<T, float>>& t, juce::Random& rng)
    {
        float sum = 0.0f;
        for (const auto& e : t)
            sum += e.second;

        float r = rng.nextFloat() * sum;
        for (const auto& e : t)
        {
            if (r < e.second)
                return e.first;
            r -= e.second;
        }
        return t.back().first;
    }

    bool hasAt (const HitVec& v, int step)
    {
        for (const auto& h : v)
            if (h.step == step)
                return true;
        return false;
    }

    void eraseAt (HitVec& v, int step)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [&] (const Hit& h) { return h.step == step; }), v.end());
    }

    void eraseRange (HitVec& v, int from, int to)   // [from, to)
    {
        v.erase (std::remove_if (v.begin(), v.end(), [&] (const Hit& h) { return h.step >= from && h.step < to; }),
                 v.end());
    }

    // ---------------------------------------------------------------- профиль стиля/части
    enum class KickMode { Chug, Gallop, Rock, TwoStep };

    struct Prof
    {
        KickMode kick = KickMode::Rock;
        float kickKeep = 1.0f;       // шанс оставить запланированный кик (разреженные части)
        float kickDouble = 0.0f;     // шанс добавить второй кик на следующей 1/16
        float crash1 = 0.0f;         // крэш на первой доле 1-го такта
        float crash3 = 0.0f;         // крэш на первой доле 3-го такта
        float endAccent = 0.0f;      // сплэш/чайна перед следующей секцией
        float accentOnKick = 0.0f;   // чайна/крэш в унисон с кик-ударами (брейкдауны)
        float fill = 0.0f;           // шанс филла в последнем такте
        bool fillPart = false;       // часть "Fill": филлы в 3-м и 4-м тактах
        bool tomGroove = false;      // грув на томах
    };

    Prof makeProfile (Style style, Part part)
    {
        Prof f;
        switch (style)
        {
            case Style::Djent:       f.kick = KickMode::Chug;    f.kickDouble = 0.12f; f.fill = 0.30f; break;
            case Style::Metalcore:   f.kick = KickMode::Gallop;  f.kickDouble = 0.30f; f.fill = 0.40f; break;
            case Style::IndieRock:   f.kick = KickMode::Rock;                          f.fill = 0.25f; break;
            case Style::DrumAndBass: f.kick = KickMode::TwoStep;                       f.fill = 0.30f; break;
        }

        switch (part)
        {
            case Part::Intro:
                f.kickKeep = 0.5f;  f.kickDouble = 0.0f;
                f.crash1 = 0.5f;    f.crash3 = 0.1f;  f.fill = 0.10f;
                break;
            case Part::Verse:
                f.fill *= 0.7f;     f.crash1 = 0.35f; f.crash3 = 0.10f;
                break;
            case Part::Bridge:
                f.kickKeep = 0.6f;  f.kickDouble *= 0.3f;
                f.crash1 = 0.2f;    f.crash3 = 0.05f; f.fill = 0.45f;
                break;
            case Part::Chorus:
                f.kickDouble *= 1.3f;
                f.crash1 = 0.95f;   f.crash3 = 0.55f; f.endAccent = 0.25f; f.fill = 0.5f;
                break;
            case Part::Fill:
                f.fill = 1.0f;      f.fillPart = true; f.crash1 = 0.3f;
                break;
            case Part::Breakdown:
                f.crash1 = 0.9f;    f.crash3 = 0.6f;  f.endAccent = 0.3f; f.fill = 0.3f;
                if (style == Style::Djent)       { f.kickDouble = 0.35f; f.accentOnKick = 0.35f; }
                if (style == Style::Metalcore)   { f.kick = KickMode::Chug; f.kickDouble = 0.30f; f.accentOnKick = 0.30f; }
                if (style == Style::IndieRock)   { f.tomGroove = true; f.kickKeep = 0.7f; }
                break;
        }
        return f;
    }

    // ---------------------------------------------------------------- кик
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

    void mutateKicks (std::vector<int>& k, const Grid& g, juce::Random& rng, int amount)
    {
        for (int i = 0; i < amount; ++i)
        {
            if (rng.nextFloat() < 0.6f)
            {
                const int s = rng.nextInt (g.spb);
                if (s != 0 && std::find (k.begin(), k.end(), s) == k.end())
                    k.push_back (s);
            }
            else if (k.size() > 1)
            {
                k.erase (k.begin() + 1 + rng.nextInt ((int) k.size() - 1));
            }
        }
        std::sort (k.begin(), k.end());
    }

    // ---------------------------------------------------------------- снейр: варианты
    enum class SnVar { Back, BackGhost, BackGhostDense, Half, HalfGhost, Cross, Double };

    std::vector<std::pair<SnVar, float>> snareTable (Style s, Part p)
    {
        using V = SnVar;
        switch (s)
        {
            case Style::Djent:
                switch (p)
                {
                    case Part::Intro:     return { { V::Cross, .35f }, { V::Back, .25f }, { V::Half, .20f }, { V::BackGhost, .20f } };
                    case Part::Verse:     return { { V::Half, .30f }, { V::HalfGhost, .15f }, { V::Back, .25f }, { V::BackGhost, .30f } };
                    case Part::Bridge:    return { { V::Half, .35f }, { V::HalfGhost, .30f }, { V::Cross, .20f }, { V::BackGhost, .15f } };
                    case Part::Chorus:    return { { V::Back, .45f }, { V::BackGhost, .30f }, { V::Half, .15f }, { V::Double, .10f } };
                    case Part::Fill:      return { { V::Back, .50f }, { V::BackGhost, .30f }, { V::Half, .20f } };
                    case Part::Breakdown: return { { V::Half, .65f }, { V::HalfGhost, .25f }, { V::Back, .10f } };
                }
                break;
            case Style::Metalcore:
                switch (p)
                {
                    case Part::Intro:     return { { V::Cross, .30f }, { V::Back, .30f }, { V::Half, .20f }, { V::BackGhost, .20f } };
                    case Part::Verse:     return { { V::Back, .30f }, { V::BackGhost, .30f }, { V::Half, .15f }, { V::Double, .10f }, { V::BackGhostDense, .15f } };
                    case Part::Bridge:    return { { V::Half, .35f }, { V::HalfGhost, .20f }, { V::Cross, .20f }, { V::BackGhost, .25f } };
                    case Part::Chorus:    return { { V::Back, .40f }, { V::BackGhost, .25f }, { V::Double, .20f }, { V::Half, .15f } };
                    case Part::Fill:      return { { V::Back, .50f }, { V::BackGhost, .30f }, { V::Double, .20f } };
                    case Part::Breakdown: return { { V::Half, .70f }, { V::HalfGhost, .20f }, { V::Back, .10f } };
                }
                break;
            case Style::IndieRock:
                switch (p)
                {
                    case Part::Intro:     return { { V::Cross, .50f }, { V::Back, .30f }, { V::BackGhost, .20f } };
                    case Part::Verse:     return { { V::Back, .30f }, { V::BackGhost, .30f }, { V::Cross, .25f }, { V::BackGhostDense, .15f } };
                    case Part::Bridge:    return { { V::Cross, .45f }, { V::Half, .25f }, { V::BackGhost, .30f } };
                    case Part::Chorus:    return { { V::Back, .40f }, { V::BackGhost, .30f }, { V::BackGhostDense, .25f }, { V::Double, .05f } };
                    case Part::Fill:      return { { V::Back, .50f }, { V::BackGhost, .30f }, { V::BackGhostDense, .20f } };
                    case Part::Breakdown: return { { V::Half, .50f }, { V::Back, .30f }, { V::BackGhostDense, .20f } };
                }
                break;
            case Style::DrumAndBass:
                switch (p)
                {
                    case Part::Intro:     return { { V::Back, .50f }, { V::BackGhost, .30f }, { V::Cross, .20f } };
                    case Part::Verse:     return { { V::Back, .30f }, { V::BackGhost, .35f }, { V::BackGhostDense, .35f } };
                    case Part::Bridge:    return { { V::Back, .35f }, { V::BackGhost, .35f }, { V::Half, .30f } };
                    case Part::Chorus:    return { { V::BackGhostDense, .40f }, { V::BackGhost, .35f }, { V::Back, .25f } };
                    case Part::Fill:      return { { V::Back, .40f }, { V::BackGhost, .30f }, { V::BackGhostDense, .30f } };
                    case Part::Breakdown: return { { V::Half, .50f }, { V::HalfGhost, .30f }, { V::Back, .20f } };
                }
                break;
        }
        return { { V::Back, 1.0f } };
    }

    // Гост-ноты вокруг основного удара снейра: позиции 16-х до и после него.
    void addGhostsAround (HitVec& snare, const HitVec& kicks, int mainStep, int barStart, int barEnd,
                          float p, int velBase, juce::Random& rng)
    {
        for (int d : { -3, -1, 1, 3 })
        {
            const int gs = mainStep + d;
            if (gs < barStart || gs >= barEnd || hasAt (snare, gs))
                continue;
            if (hasAt (kicks, gs) && rng.nextFloat() < 0.6f)
                continue;
            if (rng.nextFloat() < p)
                snare.push_back ({ gs, Art::Snare, jit (rng, velBase, 7) });
        }
    }

    void snareBar (HitVec& snare, const HitVec& kicks, SnVar v, const Grid& g, int off, juce::Random& rng)
    {
        const int n = g.groups();
        std::vector<int> mains;

        if (v == SnVar::Half || v == SnVar::HalfGhost)
            mains.push_back (g.starts[(size_t) (n / 2)]);
        else if (v == SnVar::Double)
            for (int i = 0; i < n; ++i)
                mains.push_back (g.starts[(size_t) i]);
        else
            for (int i = 1; i < n; i += 2)
                mains.push_back (g.starts[(size_t) i]);

        if (mains.empty())
            mains.push_back (g.spb / 2);

        const bool half = v == SnVar::Half || v == SnVar::HalfGhost;
        const bool cross = v == SnVar::Cross;

        for (int s : mains)
            snare.push_back ({ off + s, cross ? Art::SnareCross : Art::Snare,
                               jit (rng, cross ? 92 : (half ? 120 : 112), 5) });

        float p = 0.0f;
        if (v == SnVar::BackGhost || v == SnVar::HalfGhost) p = 0.22f;
        if (v == SnVar::BackGhostDense)                    p = 0.45f;

        if (p > 0.0f)
            for (int s : mains)
                addGhostsAround (snare, kicks, off + s, off, off + g.spb, p,
                                 v == SnVar::BackGhostDense ? 42 : 36, rng);
    }

    // ---------------------------------------------------------------- тарелки: ведущие голоса
    enum class Lead { Hat8, Hat8Open, Hat16, HatQuarter, Ride8, RideQuarter, RideBell, CrashLead, ChinaLead, OpenHatLead };

    std::vector<std::pair<Lead, float>> leadTable (Style s, Part p)
    {
        using L = Lead;
        switch (s)
        {
            case Style::Djent:
                switch (p)
                {
                    case Part::Intro:     return { { L::HatQuarter, .30f }, { L::RideQuarter, .25f }, { L::RideBell, .20f }, { L::Hat8, .15f }, { L::Hat8Open, .10f } };
                    case Part::Verse:     return { { L::Hat8, .28f }, { L::Hat8Open, .22f }, { L::Hat16, .10f }, { L::Ride8, .15f }, { L::RideBell, .10f }, { L::ChinaLead, .08f }, { L::HatQuarter, .07f } };
                    case Part::Bridge:    return { { L::RideQuarter, .30f }, { L::RideBell, .30f }, { L::HatQuarter, .20f }, { L::Ride8, .20f } };
                    case Part::Chorus:    return { { L::Ride8, .22f }, { L::CrashLead, .25f }, { L::OpenHatLead, .20f }, { L::ChinaLead, .12f }, { L::Hat8Open, .21f } };
                    case Part::Fill:      return { { L::Hat8, .45f }, { L::Ride8, .30f }, { L::HatQuarter, .25f } };
                    case Part::Breakdown: return { { L::ChinaLead, .32f }, { L::CrashLead, .20f }, { L::HatQuarter, .15f }, { L::RideBell, .15f }, { L::OpenHatLead, .18f } };
                }
                break;
            case Style::Metalcore:
                switch (p)
                {
                    case Part::Intro:     return { { L::HatQuarter, .25f }, { L::RideQuarter, .25f }, { L::RideBell, .20f }, { L::Hat8Open, .15f }, { L::Hat8, .15f } };
                    case Part::Verse:     return { { L::Hat8, .22f }, { L::Hat16, .15f }, { L::Hat8Open, .20f }, { L::Ride8, .18f }, { L::CrashLead, .10f }, { L::OpenHatLead, .15f } };
                    case Part::Bridge:    return { { L::RideBell, .30f }, { L::RideQuarter, .30f }, { L::HatQuarter, .20f }, { L::Ride8, .20f } };
                    case Part::Chorus:    return { { L::CrashLead, .30f }, { L::Ride8, .18f }, { L::OpenHatLead, .22f }, { L::ChinaLead, .15f }, { L::Hat8Open, .15f } };
                    case Part::Fill:      return { { L::Hat8, .45f }, { L::Ride8, .30f }, { L::Hat8Open, .25f } };
                    case Part::Breakdown: return { { L::ChinaLead, .30f }, { L::CrashLead, .25f }, { L::OpenHatLead, .15f }, { L::RideBell, .15f }, { L::HatQuarter, .15f } };
                }
                break;
            case Style::IndieRock:
                switch (p)
                {
                    case Part::Intro:     return { { L::HatQuarter, .40f }, { L::Hat8, .30f }, { L::RideQuarter, .30f } };
                    case Part::Verse:     return { { L::Hat8, .30f }, { L::Hat16, .18f }, { L::HatQuarter, .14f }, { L::Ride8, .20f }, { L::Hat8Open, .18f } };
                    case Part::Bridge:    return { { L::HatQuarter, .30f }, { L::RideQuarter, .30f }, { L::RideBell, .20f }, { L::Ride8, .20f } };
                    case Part::Chorus:    return { { L::OpenHatLead, .28f }, { L::Ride8, .22f }, { L::CrashLead, .20f }, { L::Hat8Open, .30f } };
                    case Part::Fill:      return { { L::Hat8, .50f }, { L::Ride8, .30f }, { L::HatQuarter, .20f } };
                    case Part::Breakdown: return { { L::RideBell, .30f }, { L::CrashLead, .25f }, { L::Ride8, .25f }, { L::OpenHatLead, .20f } };
                }
                break;
            case Style::DrumAndBass:
                switch (p)
                {
                    case Part::Intro:     return { { L::HatQuarter, .40f }, { L::Hat8, .40f }, { L::Hat16, .20f } };
                    case Part::Verse:     return { { L::Hat8, .28f }, { L::Hat16, .30f }, { L::Hat8Open, .30f }, { L::Ride8, .12f } };
                    case Part::Bridge:    return { { L::Ride8, .40f }, { L::RideQuarter, .30f }, { L::HatQuarter, .30f } };
                    case Part::Chorus:    return { { L::Hat16, .32f }, { L::Hat8Open, .25f }, { L::Ride8, .20f }, { L::CrashLead, .13f }, { L::OpenHatLead, .10f } };
                    case Part::Fill:      return { { L::Hat8, .40f }, { L::Hat16, .30f }, { L::Ride8, .30f } };
                    case Part::Breakdown: return { { L::Hat8, .28f }, { L::RideQuarter, .28f }, { L::HatQuarter, .24f }, { L::ChinaLead, .20f } };
                }
                break;
        }
        return { { Lead::Hat8, 1.0f } };
    }

    // Креши чередуются: левый, правый, дальний левый; один и тот же дважды подряд не берём.
    struct CrashSeq
    {
        int prev = -1;

        Art next (juce::Random& rng)
        {
            static const Art pool[3] = { Art::CrashLeft, Art::CrashRight, Art::CrashFarLeft };
            for (;;)
            {
                const float r = rng.nextFloat();
                const int i = r < 0.42f ? 0 : (r < 0.84f ? 1 : 2);
                if (i != prev)
                {
                    prev = i;
                    return pool[i];
                }
            }
        }
    };

    struct LeadCfg
    {
        Lead lead = Lead::Hat8;
        int dens = 0;   // для крэша/чайны: 0 доли, 1 восьмые, 2 каждая вторая доля, 3 только "и"
    };

    LeadCfg makeLeadCfg (Lead l, juce::Random& rng)
    {
        LeadCfg c;
        c.lead = l;
        if (l == Lead::CrashLead)
            c.dens = pickWeighted<int> ({ { 0, .50f }, { 1, .20f }, { 2, .15f }, { 3, .15f } }, rng);
        if (l == Lead::ChinaLead)
            c.dens = pickWeighted<int> ({ { 0, .45f }, { 1, .20f }, { 2, .15f }, { 3, .20f } }, rng);
        return c;
    }

    void layLead (HitVec& out, const LeadCfg& c, const Grid& g, int off, int bar, juce::Random& rng, CrashSeq& cs)
    {
        const int spb = g.spb;
        auto add = [&] (int s, Art a, int v) { out.push_back ({ off + s, a, jit (rng, v) }); };

        switch (c.lead)
        {
            case Lead::Hat8:
            case Lead::Hat8Open:
                for (int s = 0; s < spb; s += 2)
                {
                    const bool acc = g.isStart (s);
                    Art a = Art::HatClosedTip;
                    int v = acc ? 100 : 82;
                    if (c.lead == Lead::Hat8Open && s % 4 == 2 && rng.nextFloat() < 0.22f)
                    {
                        a = rng.nextBool() ? Art::HatOpen1 : Art::HatOpen2;
                        v = 96;
                    }
                    else if (acc && rng.nextFloat() < 0.2f)
                    {
                        a = Art::HatClosedEdge;
                    }
                    add (s, a, v);
                }
                if (c.lead == Lead::Hat8Open && bar % 2 == 1 && spb >= 4 && rng.nextFloat() < 0.5f)
                {
                    eraseAt (out, off + spb - 2);
                    add (spb - 2, Art::HatOpen1, 98);
                }
                break;

            case Lead::Hat16:
                for (int s = 0; s < spb; ++s)
                {
                    const bool open = s % 8 == 6 && rng.nextFloat() < 0.15f;
                    add (s, open ? Art::HatOpen1 : Art::HatClosedTip,
                         open ? 96 : (g.isStart (s) ? 104 : (s % 2 == 0 ? 88 : 62)));
                }
                break;

            case Lead::HatQuarter:
                for (int s : g.starts)
                    add (s, Art::HatClosedTip, s == 0 ? 105 : 92);
                if (spb >= 4 && rng.nextFloat() < 0.3f)
                    add (spb - 2, Art::HatOpen1, 92);
                break;

            case Lead::Ride8:
                for (int s = 0; s < spb; s += 2)
                {
                    const bool acc = g.isStart (s);
                    const bool bell = s == 0 && rng.nextFloat() < 0.12f;
                    add (s, bell ? Art::RideBell : Art::RideBow, bell ? 100 : (acc ? 102 : 84));
                }
                break;

            case Lead::RideQuarter:
                for (int s : g.starts)
                    add (s, s == 0 && rng.nextFloat() < 0.25f ? Art::RideBell : Art::RideBow, 96);
                break;

            case Lead::RideBell:
                for (int s : g.starts)
                    add (s, Art::RideBell, 100);
                for (int s = 2; s < spb; s += 4)
                    if (rng.nextFloat() < 0.3f)
                        add (s, Art::RideBow, 70);
                break;

            case Lead::CrashLead:
            case Lead::ChinaLead:
            {
                std::vector<int> steps;
                if (c.dens == 1)
                    for (int s = 0; s < spb; s += 2) steps.push_back (s);
                else if (c.dens == 2)
                    for (int i = 0; i < g.groups(); i += 2) steps.push_back (g.starts[(size_t) i]);
                else if (c.dens == 3)
                    for (int i = 0; i < g.groups(); ++i) steps.push_back (g.starts[(size_t) i] + g.lens[(size_t) i] / 2);
                else
                    steps = g.starts;

                Art cur = Art::CrashLeft;
                for (int s : steps)
                {
                    const bool start = g.isStart (s);
                    if (c.lead == Lead::CrashLead && (start || cur == Art::CrashLeft))
                        cur = cs.next (rng);
                    add (s, c.lead == Lead::ChinaLead ? Art::China : cur, start ? 108 : 92);
                }
                break;
            }

            case Lead::OpenHatLead:
                for (int s = 0; s < spb; s += 2)
                {
                    if (g.isStart (s))
                        add (s, Art::HatOpen1, 98);
                    else if (rng.nextFloat() < 0.85f)
                        add (s, Art::HatClosedTip, 78);
                }
                break;
        }
    }

    // Тарелки на всю фразу: ведущий голос + акценты (крэши, чайна, сплэш).
    HitVec buildCymbals (Style style, Part part, const Grid& g, int bars, const Prof& f,
                         const std::vector<std::vector<int>>& kicks, juce::Random& rng)
    {
        const int spb = g.spb;
        HitVec out;
        CrashSeq cs;

        const auto table = leadTable (style, part);
        const LeadCfg a = makeLeadCfg (pickWeighted (table, rng), rng);
        LeadCfg b = makeLeadCfg (pickWeighted (table, rng), rng);
        const bool useB = b.lead != a.lead && rng.nextFloat() < 0.4f;

        for (int bar = 0; bar < bars; ++bar)
            layLead (out, (useB && bar >= 2) ? b : a, g, bar * spb, bar, rng, cs);

        const bool intense = part == Part::Chorus || part == Part::Breakdown;

        auto accent = [&] (int step, Art art, int vel)
        {
            eraseAt (out, step);
            out.push_back ({ step, art, jit (rng, vel, 3) });
        };

        if (rng.nextFloat() < f.crash1)
            accent (0, (intense && rng.nextFloat() < 0.25f) ? Art::China : cs.next (rng), 120);

        if (bars > 2 && rng.nextFloat() < f.crash3)
            accent (2 * spb, (intense && rng.nextFloat() < 0.2f) ? Art::China : cs.next (rng), 118);

        // сплэш или чайна на "и" последней доли
        if (rng.nextFloat() < f.endAccent && spb >= 4)
            accent ((bars - 1) * spb + spb - 2, rng.nextBool() ? Art::Splash : Art::China, 108);

        // акценты в унисон с кик-ударами
        if (f.accentOnKick > 0.0f)
        {
            Art alt = Art::China;
            for (int bar = 0; bar < bars; ++bar)
                for (int s : kicks[(size_t) bar])
                    if (s != 0 && s % 2 == 0 && rng.nextFloat() < f.accentOnKick * 0.5f)
                    {
                        accent (bar * spb + s, alt, 110);
                        alt = alt == Art::China ? cs.next (rng) : Art::China;
                    }
        }

        return out;
    }

    // ---------------------------------------------------------------- филлы
    void applyFill (Lanes& L, const Grid& g, int bar, int start, juce::Random& rng)
    {
        const int spb = g.spb;
        const int from = bar * spb + start;
        const int to = (bar + 1) * spb;

        for (auto& lane : L)
            eraseRange (lane, from, to);

        static const Art ladder[] = { Art::Snare, Art::TomRack1, Art::TomRack2, Art::TomFloor1, Art::TomFloor2 };
        const int sub = rng.nextBool() ? 1 : 2;

        std::vector<int> steps;
        for (int s = from; s < to; s += sub)
            steps.push_back (s);

        const int n = (int) steps.size();
        for (int i = 0; i < n; ++i)
        {
            if (i != 0 && i != n - 1 && rng.nextFloat() < 0.2f)
                continue;
            const float prog = n > 1 ? (float) i / (float) (n - 1) : 0.0f;
            const int idx = juce::jlimit (0, 4, (int) (prog * 4.99f));
            const Art a = ladder[idx];
            L[(size_t) laneOf (a)].push_back ({ steps[(size_t) i], a, jit (rng, 84 + (int) (prog * 36.0f), 4) });
        }

        if (! steps.empty() && rng.nextFloat() < 0.6f) L[(size_t) Lane::Kick].push_back ({ steps.front(), Art::Kick, jit (rng, 110) });
        if (! steps.empty() && rng.nextFloat() < 0.5f) L[(size_t) Lane::Kick].push_back ({ steps.back(),  Art::Kick, jit (rng, 112) });
    }

    // ---------------------------------------------------------------- процедурный генератор
    Lanes proceduralLanes (Style style, Part part, const Grid& g, int bars, const Prof& f, juce::Random& rng)
    {
        Lanes L;
        const int spb = g.spb;

        const auto baseKick = kickSteps (f, g, rng);
        std::vector<std::vector<int>> kicks ((size_t) bars, baseKick);
        for (int bar = 2; bar < bars; ++bar)
            mutateKicks (kicks[(size_t) bar], g, rng, bar == bars - 1 ? 2 : 1);

        const SnVar sv = pickWeighted (snareTable (style, part), rng);

        for (int bar = 0; bar < bars; ++bar)
        {
            HitVec kickAbs;
            for (int s : kicks[(size_t) bar])
                kickAbs.push_back ({ bar * spb + s, Art::Kick, jit (rng, s == 0 ? 114 : 102, 6) });

            snareBar (L[(size_t) Lane::Snare], kickAbs, sv, g, bar * spb, rng);
            for (const auto& h : kickAbs)
                L[(size_t) Lane::Kick].push_back (h);
        }

        L[(size_t) Lane::Cymbals] = buildCymbals (style, part, g, bars, f, kicks, rng);

        if (f.tomGroove)
            for (int bar = 0; bar < bars; ++bar)
                for (int s = 0; s < spb; s += 2)
                {
                    const bool st = g.isStart (s);
                    if (! st && rng.nextFloat() < 0.15f)
                        continue;
                    L[(size_t) Lane::Toms].push_back ({ bar * spb + s, s % 4 == 0 ? Art::TomFloor1 : Art::TomFloor2,
                                                        jit (rng, st ? 105 : 85) });
                }

        if (f.fillPart)
        {
            if (bars > 2 && rng.nextFloat() < 0.6f)
                applyFill (L, g, 2, spb / 2, rng);
            applyFill (L, g, bars - 1, rng.nextBool() ? 0 : spb / 4, rng);
        }
        else if (rng.nextFloat() < f.fill)
        {
            applyFill (L, g, bars - 1, rng.nextFloat() < 0.7f ? spb / 2 : (spb * 3) / 4, rng);
        }

        return L;
    }

    // ---------------------------------------------------------------- фразы из присланных грувов
    const char* kB64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    int b64 (char c)
    {
        const char* p = std::strchr (kB64, c);
        return p != nullptr ? (int) (p - kB64) : 0;
    }

    void decodePhrase (const CorpusPhrase& ph, int barOffset, Lanes& L, juce::Random& rng)
    {
        for (const char* s = ph.data; s[0] != 0 && s[1] != 0 && s[2] != 0; s += 3)
        {
            Hit h;
            h.step = barOffset * 16 + b64 (s[0]);
            h.art = (Art) juce::jlimit (0, kNumArts - 1, b64 (s[1]));
            h.velocity = jit (rng, juce::jmax (1, b64 (s[2]) * 2), 4);
            L[(size_t) laneOf (h.art)].push_back (h);
        }
    }

    const CorpusPhrase* pickPhrase (Part part, Style style, int bars, juce::Random& rng)
    {
        const int bit = style == Style::Djent ? 1 : 2;
        std::vector<const CorpusPhrase*> pool;
        for (int i = 0; i < kCorpusSize; ++i)
        {
            const auto& p = kCorpus[i];
            if (p.part == (int) part && (p.styleMask & bit) != 0 && (bars < 0 || p.bars == bars))
                pool.push_back (&p);
        }
        return pool.empty() ? nullptr : pool[(size_t) rng.nextInt ((int) pool.size())];
    }

    bool corpusUsable (Style style, int num, int den)
    {
        return num == 4 && den == 4 && (style == Style::Djent || style == Style::Metalcore) && kCorpusSize > 0;
    }

    bool isGhost (const Hit& h) { return h.art == Art::Snare && h.velocity < 75; }

    // Режим гост-нот: оставить как в исходнике / убрать / добавить.
    void applyGhostMode (HitVec& snare, const HitVec& kicks, int bars, int spb, int mode, juce::Random& rng)
    {
        if (mode == 1)
        {
            snare.erase (std::remove_if (snare.begin(), snare.end(), isGhost), snare.end());
        }
        else if (mode == 2)
        {
            snare.erase (std::remove_if (snare.begin(), snare.end(), isGhost), snare.end());
            const HitVec mains = snare;
            const float p = rng.nextFloat() < 0.35f ? 0.45f : 0.22f;
            for (const auto& m : mains)
                if (m.step < bars * spb)
                    addGhostsAround (snare, kicks, m.step, (m.step / spb) * spb, (m.step / spb + 1) * spb, p, 38, rng);
        }
    }

    Lanes corpusLanes (Style style, Part part, juce::Random& rng, bool& ok)
    {
        Lanes L;
        ok = false;

        const Part src = part == Part::Fill ? Part::Verse : part;
        const auto* main = pickPhrase (src, style, 4, rng);
        if (main == nullptr)
            return L;

        decodePhrase (*main, 0, L, rng);
        ok = true;

        // тарелки: свои / из другой фразы / придуманные генератором
        const float r = rng.nextFloat();
        if (r < 0.28f)
        {
            if (const auto* other = pickPhrase (src, style, 4, rng))
            {
                Lanes tmp;
                decodePhrase (*other, 0, tmp, rng);
                L[(size_t) Lane::Cymbals] = tmp[(size_t) Lane::Cymbals];
            }
        }
        else if (r < 0.58f)
        {
            const Grid g = makeGrid (4, 4);
            const Prof f = makeProfile (style, part);
            std::vector<std::vector<int>> kicks (4);
            for (const auto& h : L[(size_t) Lane::Kick])
                kicks[(size_t) (h.step / 16)].push_back (h.step % 16);
            L[(size_t) Lane::Cymbals] = buildCymbals (style, part, g, 4, f, kicks, rng);
        }

        // снейр: гост-ноты оставить / убрать / добавить, иногда рим-клик
        const bool intenseP = part == Part::Chorus || part == Part::Breakdown;
        const float rg = rng.nextFloat();
        const int mode = rg < (intenseP ? 0.55f : 0.40f) ? 0 : (rg < (intenseP ? 0.85f : 0.65f) ? 1 : 2);
        applyGhostMode (L[(size_t) Lane::Snare], L[(size_t) Lane::Kick], 4, 16, mode, rng);

        const float crossP = part == Part::Intro ? 0.35f : (part == Part::Bridge ? 0.25f : 0.04f);
        if (rng.nextFloat() < crossP)
            for (auto& h : L[(size_t) Lane::Snare])
                if (h.art == Art::Snare && h.velocity >= 75)
                {
                    h.art = Art::SnareCross;
                    h.velocity = juce::jmin (h.velocity, 96);
                }

        // филл в конце
        const Prof f = makeProfile (style, part);
        if (rng.nextFloat() < f.fill)
            if (const auto* fl = pickPhrase (Part::Fill, style, -1, rng))
            {
                const int nb = fl->bars;
                const int firstBar = 4 - nb;
                for (auto& lane : L)
                    eraseRange (lane, firstBar * 16, 64);
                decodePhrase (*fl, firstBar, L, rng);
            }

        return L;
    }

    // ---------------------------------------------------------------- сборка
    Pattern assemble (const Lanes& L, Style style, Part part, int num, int den)
    {
        Pattern p;
        p.numerator = num;
        p.denominator = den;
        p.style = style;
        p.part = part;
        p.bars = 4;

        for (const auto& lane : L)
            for (const auto& h : lane)
                if (h.step >= 0 && h.step < p.totalSteps())
                    p.hits.push_back (h);

        sortHits (p.hits);
        return p;
    }

    Lanes splitLanes (const Pattern& p)
    {
        Lanes L;
        for (const auto& h : p.hits)
            L[(size_t) laneOf (h.art)].push_back (h);
        return L;
    }

    // Начало филла в последнем такте (если он там есть): по томам.
    int detectFillStart (const Pattern& p)
    {
        const int spb = p.stepsPerBar();
        const int last = (p.bars - 1) * spb;
        int count = 0, first = -1;
        for (const auto& h : p.hits)
            if (h.step >= last && laneOf (h.art) == Lane::Toms)
            {
                ++count;
                if (first < 0 || h.step < first)
                    first = h.step;
            }
        return count >= 2 ? first : -1;
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

    Lanes L;
    bool done = false;

    if (corpusUsable (style, num, den) && rng.nextFloat() < 0.88f)
        L = corpusLanes (style, part, rng, done);

    if (! done)
    {
        const Grid g = makeGrid (num, den);
        L = proceduralLanes (style, part, g, 4, makeProfile (style, part), rng);
    }

    return assemble (L, style, part, num, den);
}

Pattern regenerateLane (const Pattern& cur, Lane lane, juce::Random& rng)
{
    const Grid g = makeGrid (cur.numerator, cur.denominator);
    const Prof f = makeProfile (cur.style, cur.part);
    const int spb = g.spb;
    const int bars = cur.bars;
    const int fillStart = detectFillStart (cur);

    Lanes L = splitLanes (cur);
    HitVec& mine = L[(size_t) lane];

    // что в последнем такте после начала филла, оставляем как есть
    HitVec keep;
    if (fillStart >= 0 && lane != Lane::Toms)
        for (const auto& h : mine)
            if (h.step >= fillStart)
                keep.push_back (h);

    const int limit = fillStart >= 0 ? fillStart : bars * spb;

    HitVec fresh;

    if (lane == Lane::Snare)
    {
        const SnVar sv = pickWeighted (snareTable (cur.style, cur.part), rng);
        for (int bar = 0; bar < bars; ++bar)
        {
            if (bar * spb >= limit)
                break;
            HitVec kicksInBar;
            for (const auto& h : L[(size_t) Lane::Kick])
                if (h.step / spb == bar)
                    kicksInBar.push_back (h);
            snareBar (fresh, kicksInBar, sv, g, bar * spb, rng);
        }
    }
    else if (lane == Lane::Cymbals)
    {
        bool fromCorpus = false;
        if (corpusUsable (cur.style, cur.numerator, cur.denominator) && rng.nextFloat() < 0.45f)
        {
            const Part src = cur.part == Part::Fill ? Part::Verse : cur.part;
            if (const auto* ph = pickPhrase (src, cur.style, 4, rng))
            {
                Lanes tmp;
                decodePhrase (*ph, 0, tmp, rng);
                fresh = tmp[(size_t) Lane::Cymbals];
                fromCorpus = true;
            }
        }

        if (! fromCorpus)
        {
            std::vector<std::vector<int>> kicks ((size_t) bars);
            for (const auto& h : L[(size_t) Lane::Kick])
                kicks[(size_t) juce::jlimit (0, bars - 1, h.step / spb)].push_back (h.step % spb);
            fresh = buildCymbals (cur.style, cur.part, g, bars, f, kicks, rng);
        }
    }
    else if (lane == Lane::Kick)
    {
        bool fromCorpus = false;
        if (corpusUsable (cur.style, cur.numerator, cur.denominator) && rng.nextFloat() < 0.5f)
        {
            const Part src = cur.part == Part::Fill ? Part::Verse : cur.part;
            if (const auto* ph = pickPhrase (src, cur.style, 4, rng))
            {
                Lanes tmp;
                decodePhrase (*ph, 0, tmp, rng);
                fresh = tmp[(size_t) Lane::Kick];
                fromCorpus = true;
            }
        }

        if (! fromCorpus)
        {
            const auto base = kickSteps (f, g, rng);
            for (int bar = 0; bar < bars; ++bar)
            {
                auto k = base;
                if (bar >= 2)
                    mutateKicks (k, g, rng, bar == bars - 1 ? 2 : 1);
                for (int s : k)
                    fresh.push_back ({ bar * spb + s, Art::Kick, jit (rng, s == 0 ? 114 : 102, 6) });
            }
        }
    }
    else // Toms
    {
        const float r = rng.nextFloat();
        if (r < 0.70f)
        {
            // томовый филл в конце фразы
            static const Art toms[] = { Art::TomRack1, Art::TomRack2, Art::TomFloor1, Art::TomFloor2 };
            const int from = (bars - 1) * spb + (rng.nextFloat() < 0.6f ? spb / 2 : (spb * 3) / 4);
            const int sub = rng.nextBool() ? 1 : 2;
            std::vector<int> steps;
            for (int s = from; s < bars * spb; s += sub)
                steps.push_back (s);
            const int n = (int) steps.size();
            for (int i = 0; i < n; ++i)
            {
                if (i != 0 && i != n - 1 && rng.nextFloat() < 0.2f)
                    continue;
                const float prog = n > 1 ? (float) i / (float) (n - 1) : 0.0f;
                fresh.push_back ({ steps[(size_t) i], toms[juce::jlimit (0, 3, (int) (prog * 3.99f))],
                                   jit (rng, 88 + (int) (prog * 32.0f), 4) });
            }
        }
        else if (r < 0.90f)
        {
            // акценты томами на сильных долях
            for (int bar = 0; bar < bars; ++bar)
                for (int s = 0; s < spb; s += 2)
                {
                    if (! g.isStart (s) && rng.nextFloat() < 0.4f)
                        continue;
                    fresh.push_back ({ bar * spb + s, s % 4 == 0 ? Art::TomFloor1 : Art::TomFloor2,
                                       jit (rng, g.isStart (s) ? 105 : 85) });
                }
        }
        // иначе томов нет
    }

    HitVec merged = keep;
    for (const auto& h : fresh)
        if (lane == Lane::Toms || h.step < limit)
            merged.push_back (h);

    mine = merged;
    return assemble (L, cur.style, cur.part, cur.numerator, cur.denominator);
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
