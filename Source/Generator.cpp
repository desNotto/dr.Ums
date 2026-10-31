#include "Generator.h"
#include "Corpus.h"
#include <cstring>

namespace drums
{
namespace
{
    using HitVec = std::vector<Hit>;
    using Lanes = std::array<HitVec, (size_t) kNumLanes>;

    // ================================================================ сетка такта
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

    // ================================================================ мелочи
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

    // ================================================================ профиль стиля и части
    struct Prof
    {
        float kickKeep = 1.0f;       // шанс оставить запланированный кик (разреженные части)
        float kickDouble = 0.0f;     // шанс добавить второй кик на следующей 1/16
        float crash1 = 0.0f;         // крэш на первой доле 1-го такта
        float crash3 = 0.0f;         // крэш на первой доле 3-го такта
        float crashEvery = 0.0f;     // шанс крэша на первой доле каждого такта (в песнях это очень частый приём)
        float endAccent = 0.0f;      // сплэш/чайна перед следующей секцией
        float accentOnKick = 0.0f;   // чайна/крэш в унисон с кик-ударами (брейкдауны)
        float fill = 0.0f;           // шанс филла в последнем такте
        float pause = 0.0f;          // шанс паузы в конце фразы перед следующей секцией
        bool fillPart = false;       // часть "Fill": филлы в 3-м и 4-м тактах
    };

    Prof makeProfile (Style style, Part part)
    {
        Prof f;
        switch (style)
        {
            case Style::Djent:      f.kickDouble = 0.12f; f.fill = 0.30f; break;
            case Style::Metalcore:  f.kickDouble = 0.30f; f.fill = 0.40f; break;
            case Style::Deathcore:  f.kickDouble = 0.25f; f.fill = 0.40f; break;
            case Style::BlackMetal: f.kickDouble = 0.10f; f.fill = 0.35f; f.pause = 0.12f; break;
            case Style::PostMetal:  f.kickDouble = 0.0f;  f.fill = 0.35f; f.pause = 0.08f; break;
            case Style::IndieRock:  f.kickDouble = 0.0f;  f.fill = 0.25f; break;
        }

        const bool metalish = style == Style::Djent || style == Style::Metalcore || style == Style::Deathcore;
        const bool bm = style == Style::BlackMetal;
        const bool post = style == Style::PostMetal;

        switch (part)
        {
            case Part::Intro:
                f.kickKeep = 0.5f;  f.kickDouble = 0.0f;
                f.crash1 = 0.5f;    f.crash3 = 0.1f;  f.fill = 0.10f;
                f.crashEvery = metalish ? 0.20f : (post ? 0.10f : 0.05f);
                if (post) f.crash1 = 0.6f;
                break;
            case Part::Verse:
                f.fill *= 0.7f;     f.crash1 = 0.35f; f.crash3 = 0.10f;
                f.crashEvery = metalish ? 0.25f : (bm ? 0.12f : (post ? 0.15f : 0.05f));
                break;
            case Part::Bridge:
                f.kickKeep = 0.6f;  f.kickDouble *= 0.3f;
                f.crash1 = 0.2f;    f.crash3 = 0.05f; f.fill = 0.45f;
                f.crashEvery = metalish ? 0.10f : (post ? 0.10f : 0.03f);
                break;
            case Part::Chorus:
                f.kickDouble *= 1.3f;
                f.crash1 = 0.95f;   f.crash3 = 0.55f; f.endAccent = 0.25f; f.fill = 0.5f;
                f.crashEvery = metalish ? 0.60f : (bm ? 0.35f : (post ? 0.70f : 0.30f));
                break;
            case Part::Fill:
                f.fill = 1.0f;      f.fillPart = true; f.crash1 = 0.3f;
                f.crashEvery = metalish ? 0.30f : 0.05f;
                break;
            case Part::Breakdown:
                f.crash1 = 0.9f;    f.crash3 = 0.6f;  f.endAccent = 0.3f; f.fill = 0.3f;
                f.crashEvery = (metalish || post) ? 0.55f : 0.30f;
                f.kickDouble = style == Style::Djent ? 0.35f : (style == Style::Deathcore ? 0.30f : (post ? 0.15f : 0.20f));
                if (style == Style::Djent || style == Style::Metalcore || style == Style::Deathcore)
                    f.accentOnKick = style == Style::Deathcore ? 0.35f : 0.30f;
                if (post) f.accentOnKick = 0.25f;
                if (style == Style::Deathcore) f.pause = 0.10f;
                break;
        }
        return f;
    }

    // ================================================================ кик
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

    enum class KickMode { Chug, Gallop, Rock, Slow, HcTwoStep, Pulse16, Bounce, Sparse };

    std::vector<int> kickSteps (KickMode mode, const Prof& f, const Grid& g, juce::Random& rng)
    {
        const int spb = g.spb;
        std::vector<int> out;

        switch (mode)
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

            case KickMode::Slow:
            {
                out.push_back (0);
                const int extras[] = { (spb * 3) / 8, (spb * 5) / 8, (spb * 7) / 8, (spb * 3) / 16, (spb * 7) / 16 };
                int added = 0;
                for (int e : extras)
                    if (added < 2 && rng.nextFloat() < 0.28f)
                    {
                        out.push_back (e);
                        ++added;
                    }
                break;
            }

            case KickMode::HcTwoStep:   // хардкорный two-step: упор на 2 и 4, кик на "и" второй доли
                out.push_back (0);
                if (rng.nextFloat() < 0.80f) out.push_back ((spb * 3) / 8);
                if (rng.nextFloat() < 0.50f) out.push_back (spb / 2);
                if (rng.nextFloat() < 0.40f) out.push_back ((spb * 5) / 8);
                if (rng.nextFloat() < 0.30f) out.push_back ((spb * 7) / 8);
                break;

            case KickMode::Pulse16:     // двойная бочка шестнадцатыми
                for (int s = 0; s < spb; ++s)
                    if (s == 0 || rng.nextFloat() < (s % 2 == 0 ? 0.93f : 0.85f))
                        out.push_back (s);
                break;

            case KickMode::Bounce:      // брейкдаун в стиле deathcore: чаги + короткие очереди шестнадцатых
            {
                out = randomSplit (spb, rng);
                const int bursts = 1 + rng.nextInt (2);
                for (int b = 0; b < bursts; ++b)
                {
                    const int len = 3 + rng.nextInt (2);
                    const int at = rng.nextFloat() < 0.6f ? spb - len : rng.nextInt (juce::jmax (1, spb - len));
                    for (int k = 0; k < len; ++k)
                        if (at + k < spb)
                            out.push_back (at + k);
                }
                break;
            }

            case KickMode::Sparse:
                out.push_back (0);
                if (rng.nextFloat() < 0.30f)
                    out.push_back (spb / 2);
                break;
        }

        std::vector<int> res;
        for (int s : out)
            if (s == 0 || rng.nextFloat() < f.kickKeep)
                res.push_back (s);

        if (f.kickDouble > 0.0f && mode != KickMode::Pulse16)
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

    // ================================================================ снейр
    enum class SnVar { Back, BackGhost, BackGhostDense, Half, HalfGhost, Cross, Double, None };

    // Гост-ноты вокруг основного удара снейра. В песнях чаще всего они стоят за 3 шестнадцатых
    // до или после основного удара, реже за 2, совсем редко за 1.
    void addGhostsAround (HitVec& snare, const HitVec& kicks, int mainStep, int barStart, int barEnd,
                          float p, int velBase, juce::Random& rng)
    {
        static const std::pair<int, float> offsets[] = { { -3, 1.0f }, { 3, 0.95f }, { -2, 0.55f }, { 2, 0.40f },
                                                          { 1, 0.33f }, { -4, 0.40f } };
        for (const auto& o : offsets)
        {
            const int gs = mainStep + o.first;
            if (gs < barStart || gs >= barEnd || hasAt (snare, gs))
                continue;
            if (hasAt (kicks, gs) && rng.nextFloat() < 0.6f)
                continue;
            if (rng.nextFloat() < p * o.second * 1.25f)
                snare.push_back ({ gs, Art::Snare, jit (rng, velBase, 7) });
        }
    }

    void snareBar (HitVec& snare, const HitVec& kicks, SnVar v, const Grid& g, int off, juce::Random& rng)
    {
        if (v == SnVar::None)
            return;

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

    bool isHalfVar (SnVar v) { return v == SnVar::Half || v == SnVar::HalfGhost || v == SnVar::None; }

    // ================================================================ грув-рецепты ("фиды")
    // Каждый рецепт описывает целиком манеру игры: кик, снейр, ведущие тарелки и приёмы.
    enum class Feel
    {
        Backbeat,    // рок-бэкбит восьмыми
        DoubleBass,  // двойная бочка шестнадцатыми + бэкбит или полутайм
        Gallop,      // галопы метал-кора
        Chug,        // синкопированные "чаги" (djent, брейкдауны)
        TwoStep,     // хардкорный two-step
        HalfTime,    // полутайм: снейр на третьей доле
        Bounce,      // deathcore: полутайм + очереди бочки, чайна четвертями
        Doom,        // медленно и тяжело: редкий кик, крэш/колокол как «стена»
        Skank,       // скэнк (thrash beat): кик на долях, снейр на «и», тарелка на каждый удар
        BlastTrad,   // традиционный бласт: кик и снейр по очереди, тарелка вместе с киком
        BlastHammer, // хаммер-бласт: кик, снейр и тарелка вместе
        BlastBomb,   // бомб-бласт: хаммер, но кик вдвое чаще
        Tribal,      // «племенной» томовый грув (post-metal)
        Layered,     // слои: кик, затем томы, затем тарелки, затем всё вместе
        BuildUp,     // нарастание: ролл снейра с усилением громкости
        Atmos        // атмосфера: свеллы тарелок, редкие томы
    };

    // Какие рецепты характерны для каждого жанра и части песни. Источники: разбор присланных песен
    // и описания жанров (breakdown: полутайм и чайна/крэш четвертями; бласты: традиционный, хаммер, бомб, скэнк;
    // post-metal: томовые остинато, нарастание плотности, свеллы).
    std::vector<std::pair<Feel, float>> feelTable (Style s, Part p)
    {
        using F = Feel;
        switch (s)
        {
            case Style::Djent:
                switch (p)
                {
                    case Part::Intro:     return { { F::Atmos, .22f }, { F::Layered, .14f }, { F::HalfTime, .16f }, { F::Backbeat, .16f }, { F::BuildUp, .16f }, { F::Chug, .16f } };
                    case Part::Verse:     return { { F::Chug, .36f }, { F::Backbeat, .14f }, { F::HalfTime, .15f }, { F::DoubleBass, .10f }, { F::Bounce, .10f }, { F::TwoStep, .10f }, { F::Gallop, .05f } };
                    case Part::Bridge:    return { { F::HalfTime, .28f }, { F::Atmos, .18f }, { F::BuildUp, .20f }, { F::Chug, .14f }, { F::Tribal, .12f }, { F::Layered, .08f } };
                    case Part::Chorus:    return { { F::Backbeat, .28f }, { F::DoubleBass, .20f }, { F::Chug, .20f }, { F::Gallop, .10f }, { F::HalfTime, .10f }, { F::TwoStep, .12f } };
                    case Part::Fill:      return { { F::Backbeat, .40f }, { F::Chug, .30f }, { F::DoubleBass, .30f } };
                    case Part::Breakdown: return { { F::Chug, .40f }, { F::Bounce, .25f }, { F::HalfTime, .20f }, { F::DoubleBass, .15f } };
                }
                break;
            case Style::Metalcore:
                switch (p)
                {
                    case Part::Intro:     return { { F::Atmos, .18f }, { F::BuildUp, .20f }, { F::Layered, .12f }, { F::Backbeat, .18f }, { F::HalfTime, .16f }, { F::TwoStep, .16f } };
                    case Part::Verse:     return { { F::Gallop, .28f }, { F::Backbeat, .20f }, { F::DoubleBass, .20f }, { F::TwoStep, .16f }, { F::Skank, .10f }, { F::Chug, .06f } };
                    case Part::Bridge:    return { { F::HalfTime, .28f }, { F::BuildUp, .26f }, { F::Atmos, .14f }, { F::TwoStep, .14f }, { F::Backbeat, .18f } };
                    case Part::Chorus:    return { { F::Backbeat, .28f }, { F::DoubleBass, .24f }, { F::Gallop, .20f }, { F::TwoStep, .16f }, { F::Skank, .12f } };
                    case Part::Fill:      return { { F::Backbeat, .40f }, { F::DoubleBass, .30f }, { F::Gallop, .30f } };
                    case Part::Breakdown: return { { F::Chug, .36f }, { F::Bounce, .22f }, { F::HalfTime, .26f }, { F::TwoStep, .16f } };
                }
                break;
            case Style::Deathcore:
                switch (p)
                {
                    case Part::Intro:     return { { F::Atmos, .20f }, { F::BuildUp, .24f }, { F::Bounce, .20f }, { F::Chug, .20f }, { F::HalfTime, .16f } };
                    case Part::Verse:     return { { F::DoubleBass, .24f }, { F::BlastTrad, .14f }, { F::BlastBomb, .16f }, { F::Gallop, .14f }, { F::Bounce, .14f }, { F::Chug, .10f }, { F::Skank, .08f } };
                    case Part::Bridge:    return { { F::HalfTime, .24f }, { F::BuildUp, .24f }, { F::Bounce, .26f }, { F::Atmos, .14f }, { F::Chug, .12f } };
                    case Part::Chorus:    return { { F::DoubleBass, .24f }, { F::BlastHammer, .14f }, { F::BlastBomb, .14f }, { F::Gallop, .14f }, { F::Backbeat, .16f }, { F::TwoStep, .18f } };
                    case Part::Fill:      return { { F::DoubleBass, .40f }, { F::BlastTrad, .30f }, { F::Chug, .30f } };
                    case Part::Breakdown: return { { F::Bounce, .46f }, { F::Chug, .24f }, { F::HalfTime, .20f }, { F::DoubleBass, .10f } };
                }
                break;
            case Style::BlackMetal:
                switch (p)
                {
                    case Part::Intro:     return { { F::Atmos, .34f }, { F::BuildUp, .14f }, { F::Doom, .18f }, { F::Tribal, .10f }, { F::Skank, .14f }, { F::BlastTrad, .10f } };
                    case Part::Verse:     return { { F::BlastTrad, .30f }, { F::BlastHammer, .20f }, { F::Skank, .20f }, { F::DoubleBass, .10f }, { F::Gallop, .10f }, { F::BlastBomb, .10f } };
                    case Part::Bridge:    return { { F::Doom, .30f }, { F::Atmos, .24f }, { F::HalfTime, .14f }, { F::Tribal, .10f }, { F::Skank, .12f }, { F::BuildUp, .10f } };
                    case Part::Chorus:    return { { F::BlastHammer, .26f }, { F::BlastTrad, .24f }, { F::BlastBomb, .10f }, { F::Skank, .20f }, { F::DoubleBass, .20f } };
                    case Part::Fill:      return { { F::BlastTrad, .40f }, { F::DoubleBass, .30f }, { F::Skank, .30f } };
                    case Part::Breakdown: return { { F::Doom, .36f }, { F::HalfTime, .22f }, { F::Atmos, .20f }, { F::Bounce, .10f }, { F::Chug, .12f } };
                }
                break;
            case Style::PostMetal:
                switch (p)
                {
                    case Part::Intro:     return { { F::Layered, .30f }, { F::Atmos, .30f }, { F::Tribal, .24f }, { F::BuildUp, .16f } };
                    case Part::Verse:     return { { F::HalfTime, .30f }, { F::Tribal, .20f }, { F::Doom, .20f }, { F::Layered, .10f }, { F::Backbeat, .10f }, { F::Atmos, .10f } };
                    case Part::Bridge:    return { { F::Tribal, .30f }, { F::Atmos, .24f }, { F::BuildUp, .24f }, { F::Layered, .22f } };
                    case Part::Chorus:    return { { F::Doom, .30f }, { F::HalfTime, .24f }, { F::Chug, .20f }, { F::Tribal, .10f }, { F::Backbeat, .16f } };
                    case Part::Fill:      return { { F::Tribal, .40f }, { F::HalfTime, .30f }, { F::Backbeat, .30f } };
                    case Part::Breakdown: return { { F::Doom, .36f }, { F::Chug, .24f }, { F::HalfTime, .24f }, { F::Tribal, .16f } };
                }
                break;
            case Style::IndieRock:
                switch (p)
                {
                    case Part::Intro:     return { { F::Backbeat, .30f }, { F::Layered, .20f }, { F::BuildUp, .16f }, { F::HalfTime, .20f }, { F::Atmos, .14f } };
                    case Part::Verse:     return { { F::Backbeat, .50f }, { F::HalfTime, .14f }, { F::Layered, .08f }, { F::Skank, .10f }, { F::TwoStep, .18f } };
                    case Part::Bridge:    return { { F::HalfTime, .28f }, { F::BuildUp, .24f }, { F::Atmos, .18f }, { F::Layered, .16f }, { F::Tribal, .14f } };
                    case Part::Chorus:    return { { F::Backbeat, .50f }, { F::Skank, .14f }, { F::TwoStep, .16f }, { F::HalfTime, .10f }, { F::BuildUp, .10f } };
                    case Part::Fill:      return { { F::Backbeat, .60f }, { F::Tribal, .20f }, { F::Skank, .20f } };
                    case Part::Breakdown: return { { F::HalfTime, .34f }, { F::Tribal, .30f }, { F::Backbeat, .20f }, { F::Layered, .16f } };
                }
                break;
        }
        return { { Feel::Backbeat, 1.0f } };
    }

    // Варианты снейра для рецепта (с гост-нотами и без).
    std::vector<std::pair<SnVar, float>> snareFamilyRaw (Feel feel, Part part, Style style)
    {
        using V = SnVar;
        const bool soft = part == Part::Intro || part == Part::Bridge || style == Style::IndieRock;
        const float cross = soft ? 0.14f : 0.03f;

        switch (feel)
        {
            case Feel::Backbeat:   return { { V::Back, .38f }, { V::BackGhost, .30f }, { V::BackGhostDense, .16f }, { V::Cross, cross }, { V::Double, .04f } };
            case Feel::DoubleBass: return { { V::Back, .48f }, { V::BackGhost, .20f }, { V::Half, .24f }, { V::HalfGhost, .08f } };
            case Feel::Gallop:     return { { V::Back, .58f }, { V::BackGhost, .24f }, { V::Double, .18f } };
            case Feel::Chug:       return { { V::Half, .46f }, { V::Back, .28f }, { V::HalfGhost, .14f }, { V::BackGhost, .12f } };
            case Feel::TwoStep:    return { { V::Back, .66f }, { V::BackGhost, .34f } };
            case Feel::HalfTime:   return { { V::Half, .56f }, { V::HalfGhost, .26f }, { V::Cross, cross }, { V::None, .04f } };
            case Feel::Bounce:     return { { V::Half, .74f }, { V::HalfGhost, .26f } };
            case Feel::Doom:       return { { V::Half, .78f }, { V::HalfGhost, .12f }, { V::None, .10f } };
            case Feel::Tribal:     return { { V::None, .50f }, { V::Half, .28f }, { V::Cross, .12f }, { V::HalfGhost, .10f } };
            case Feel::Layered:    return { { V::Half, .60f }, { V::Cross, .20f }, { V::HalfGhost, .20f } };
            case Feel::Atmos:      return { { V::None, .66f }, { V::Cross, .16f }, { V::Half, .18f } };
            case Feel::Skank:
            case Feel::BlastTrad:
            case Feel::BlastHammer:
            case Feel::BlastBomb:
            case Feel::BuildUp:    break;
        }
        return { { V::Back, 1.0f } };
    }

    // В брейкдаунах снейр почти всегда в полутайме (на третьей доле), в мостах он тоже чаще такой.
    std::vector<std::pair<SnVar, float>> snareFamily (Feel feel, Part part, Style style)
    {
        auto fam = snareFamilyRaw (feel, part, style);
        const float k = part == Part::Breakdown ? 3.0f : (part == Part::Bridge ? 1.5f : 1.0f);
        if (k != 1.0f)
            for (auto& e : fam)
                if (e.first == SnVar::Half || e.first == SnVar::HalfGhost)
                    e.second *= k;
        return fam;
    }

    // Для «Random снейра», когда рецепт неизвестен: берём семейство по текущему ощущению.
    std::vector<std::pair<SnVar, float>> snareFamilyFor (bool half, Part part, Style style)
    {
        return snareFamily (half ? Feel::HalfTime : Feel::Backbeat, part, style);
    }

    // ================================================================ тарелки: ведущие голоса
    enum class Lead { Hat8, Hat8Open, Hat16, HatQuarter, Ride8, RideQuarter, RideBell, CrashLead, ChinaLead, OpenHatLead, CrashSwell };
    using LeadTable = std::vector<std::pair<Lead, float>>;

    // Веса подобраны по разбору ~70 тысяч тактов из присланных песен: в металле ведущим
    // чаще всего бывает крэш (в четверти или в восьмые), потом хэт, чайна заметно реже,
    // райд встречается редко.
    LeadTable metalLeads (Part p, float china, float hat16)
    {
        using L = Lead;
        switch (p)
        {
            case Part::Intro:     return { { L::HatQuarter, .28f }, { L::RideQuarter, .18f }, { L::RideBell, .14f }, { L::Hat8, .14f }, { L::Hat8Open, .10f }, { L::CrashSwell, .10f }, { L::CrashLead, .06f } };
            case Part::Verse:     return { { L::Hat8, .22f }, { L::Hat8Open, .20f }, { L::Hat16, .08f * hat16 }, { L::HatQuarter, .08f }, { L::CrashLead, .22f }, { L::ChinaLead, .04f * china }, { L::Ride8, .04f }, { L::OpenHatLead, .08f }, { L::RideBell, .04f } };
            case Part::Bridge:    return { { L::RideQuarter, .20f }, { L::RideBell, .20f }, { L::HatQuarter, .18f }, { L::Ride8, .12f }, { L::Hat8, .10f }, { L::CrashLead, .10f }, { L::CrashSwell, .10f } };
            case Part::Chorus:    return { { L::CrashLead, .38f }, { L::Hat8Open, .16f }, { L::OpenHatLead, .10f }, { L::Hat8, .10f }, { L::Ride8, .06f }, { L::ChinaLead, .10f * china }, { L::HatQuarter, .04f }, { L::Hat16, .04f * hat16 } };
            case Part::Fill:      return { { L::Hat8, .40f }, { L::CrashLead, .20f }, { L::Ride8, .20f }, { L::HatQuarter, .20f } };
            case Part::Breakdown: return { { L::CrashLead, .36f }, { L::ChinaLead, .24f * china }, { L::HatQuarter, .12f }, { L::OpenHatLead, .10f }, { L::Hat8, .08f }, { L::RideBell, .06f }, { L::Ride8, .04f } };
        }
        return { { L::Hat8, 1.0f } };
    }

    LeadTable leadTable (Style s, Part p)
    {
        using L = Lead;
        switch (s)
        {
            case Style::Djent:     return metalLeads (p, 1.3f, 1.0f);
            case Style::Metalcore: return metalLeads (p, 1.0f, 1.3f);
            case Style::Deathcore: return metalLeads (p, 1.7f, 0.7f);
            case Style::BlackMetal:
                switch (p)
                {
                    case Part::Intro:     return { { L::RideBell, .25f }, { L::CrashSwell, .30f }, { L::RideQuarter, .20f }, { L::Hat8, .10f }, { L::HatQuarter, .15f } };
                    case Part::Verse:     return { { L::Ride8, .35f }, { L::Hat8, .20f }, { L::Hat16, .15f }, { L::CrashLead, .15f }, { L::ChinaLead, .05f }, { L::OpenHatLead, .10f } };
                    case Part::Bridge:    return { { L::RideBell, .30f }, { L::CrashSwell, .25f }, { L::RideQuarter, .20f }, { L::HatQuarter, .10f }, { L::CrashLead, .15f } };
                    case Part::Chorus:    return { { L::CrashLead, .35f }, { L::Ride8, .30f }, { L::ChinaLead, .10f }, { L::OpenHatLead, .15f }, { L::Hat16, .10f } };
                    case Part::Fill:      return { { L::Ride8, .40f }, { L::Hat8, .30f }, { L::CrashLead, .30f } };
                    case Part::Breakdown: return { { L::RideBell, .30f }, { L::CrashLead, .30f }, { L::ChinaLead, .20f }, { L::CrashSwell, .10f }, { L::Ride8, .10f } };
                }
                break;
            case Style::PostMetal:
                switch (p)
                {
                    case Part::Intro:     return { { L::CrashSwell, .30f }, { L::RideBell, .25f }, { L::RideQuarter, .20f }, { L::HatQuarter, .15f }, { L::Hat8Open, .10f } };
                    case Part::Verse:     return { { L::RideQuarter, .25f }, { L::RideBell, .25f }, { L::HatQuarter, .15f }, { L::Hat8Open, .15f }, { L::Ride8, .10f }, { L::CrashLead, .10f } };
                    case Part::Bridge:    return { { L::RideBell, .35f }, { L::RideQuarter, .25f }, { L::CrashSwell, .20f }, { L::HatQuarter, .10f }, { L::Hat8Open, .10f } };
                    case Part::Chorus:    return { { L::CrashLead, .40f }, { L::ChinaLead, .12f }, { L::OpenHatLead, .13f }, { L::RideBell, .12f }, { L::Ride8, .13f }, { L::CrashSwell, .10f } };
                    case Part::Fill:      return { { L::RideBell, .40f }, { L::Hat8, .30f }, { L::CrashLead, .30f } };
                    case Part::Breakdown: return { { L::CrashLead, .35f }, { L::ChinaLead, .30f }, { L::RideBell, .15f }, { L::HatQuarter, .10f }, { L::OpenHatLead, .10f } };
                }
                break;
            case Style::IndieRock:
                switch (p)
                {
                    case Part::Intro:     return { { L::HatQuarter, .40f }, { L::Hat8, .30f }, { L::RideQuarter, .30f } };
                    case Part::Verse:     return { { L::Hat8, .30f }, { L::Hat16, .18f }, { L::HatQuarter, .14f }, { L::Ride8, .20f }, { L::Hat8Open, .18f } };
                    case Part::Bridge:    return { { L::HatQuarter, .30f }, { L::RideQuarter, .30f }, { L::RideBell, .20f }, { L::Ride8, .20f } };
                    case Part::Chorus:    return { { L::OpenHatLead, .26f }, { L::Ride8, .20f }, { L::CrashLead, .22f }, { L::Hat8Open, .30f }, { L::ChinaLead, .02f } };
                    case Part::Fill:      return { { L::Hat8, .50f }, { L::Ride8, .30f }, { L::HatQuarter, .20f } };
                    case Part::Breakdown: return { { L::RideBell, .28f }, { L::CrashLead, .27f }, { L::Ride8, .25f }, { L::OpenHatLead, .20f } };
                }
                break;
        }
        return { { Lead::Hat8, 1.0f } };
    }

    // Для отдельных рецептов набор тарелок свой.
    LeadTable leadTableFor (Feel feel, Style style, Part part)
    {
        using L = Lead;
        switch (feel)
        {
            case Feel::Bounce:     return { { L::ChinaLead, .40f }, { L::CrashLead, .35f }, { L::HatQuarter, .10f }, { L::RideBell, .10f }, { L::OpenHatLead, .05f } };
            case Feel::Doom:       return { { L::RideBell, .30f }, { L::CrashLead, .30f }, { L::ChinaLead, .15f }, { L::RideQuarter, .15f }, { L::CrashSwell, .10f } };
            case Feel::DoubleBass: return { { L::CrashLead, .30f }, { L::Ride8, .25f }, { L::Hat8, .20f }, { L::ChinaLead, .10f }, { L::Hat8Open, .15f } };
            case Feel::Gallop:     return { { L::Hat8, .25f }, { L::Hat8Open, .20f }, { L::CrashLead, .25f }, { L::Ride8, .15f }, { L::OpenHatLead, .15f } };
            case Feel::TwoStep:    return { { L::CrashLead, .35f }, { L::Ride8, .30f }, { L::Hat8Open, .20f }, { L::OpenHatLead, .15f } };
            case Feel::Chug:
            case Feel::Backbeat:
            case Feel::HalfTime:
            case Feel::Skank:
            case Feel::BlastTrad:
            case Feel::BlastHammer:
            case Feel::BlastBomb:
            case Feel::Tribal:
            case Feel::Layered:
            case Feel::BuildUp:
            case Feel::Atmos:      break;
        }
        return leadTable (style, part);
    }

    // Креши чередуются. В песнях правый крэш (57) встречается подряд ~79% случаев, левый (49)
    // почти всегда сменяется правым; здесь смены сделаны чаще, чем в песнях, чтобы разнообразить звук.
    struct CrashSeq
    {
        int prev = -1;   // 0 левый, 1 правый, 2 дальний левый

        Art next (juce::Random& rng)
        {
            static const Art pool[3] = { Art::CrashLeft, Art::CrashRight, Art::CrashFarLeft };
            int i;
            const float r = rng.nextFloat();

            if (prev < 0)         i = r < 0.55f ? 1 : (r < 0.88f ? 0 : 2);
            else if (r < 0.08f)   i = prev == 2 ? 1 : 2;
            else if (prev == 1)   i = rng.nextFloat() < 0.42f ? 0 : 1;
            else if (prev == 0)   i = rng.nextFloat() < 0.78f ? 1 : 0;
            else                  i = rng.nextBool() ? 0 : 1;

            prev = i;
            return pool[i];
        }
    };

    struct LeadCfg
    {
        Lead lead = Lead::Hat8;
        int dens = 0;   // для крэша/чайны: 0 доли, 1 восьмые, 2 каждая вторая доля, 3 только "и"
    };

    // В полутайме крэш чаще бьют в четверти, в бэкбите чаще в восьмые.
    LeadCfg makeLeadCfg (Lead l, bool halfFeel, juce::Random& rng)
    {
        LeadCfg c;
        c.lead = l;
        if (l == Lead::CrashLead)
            c.dens = halfFeel ? pickWeighted<int> ({ { 0, .58f }, { 1, .22f }, { 2, .12f }, { 3, .08f } }, rng)
                              : pickWeighted<int> ({ { 0, .30f }, { 1, .50f }, { 2, .10f }, { 3, .10f } }, rng);
        if (l == Lead::ChinaLead)
            c.dens = halfFeel ? pickWeighted<int> ({ { 0, .60f }, { 1, .12f }, { 2, .13f }, { 3, .15f } }, rng)
                              : pickWeighted<int> ({ { 0, .50f }, { 1, .20f }, { 2, .12f }, { 3, .18f } }, rng);
        return c;
    }

    void layLead (HitVec& out, const LeadCfg& c, const Grid& g, int off, int bar, int bars, juce::Random& rng, CrashSeq& cs)
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
                    // открытый хэт в песнях чаще на долях, но и на "и" тоже
                    const float openP = acc ? (s == 0 ? 0.0f : 0.08f) : 0.14f;
                    if (c.lead == Lead::Hat8Open && rng.nextFloat() < openP)
                    {
                        a = rng.nextFloat() < 0.7f ? Art::HatOpen1 : Art::HatOpen2;
                        v = 96;
                    }
                    else if (acc && rng.nextFloat() < 0.2f)
                    {
                        a = Art::HatClosedEdge;
                    }
                    add (s, a, v);
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
                bool have = false;
                for (int s : steps)
                {
                    const bool start = g.isStart (s);
                    if (c.lead == Lead::CrashLead && (start || ! have))
                    {
                        cur = cs.next (rng);
                        have = true;
                    }
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

            case Lead::CrashSwell:
            {
                // крэши на долях с нарастающей громкостью по всей фразе
                const int n = g.groups();
                for (int i = 0; i < n; ++i)
                {
                    const float t = (float) (bar * n + i + 1) / (float) (bars * n);
                    add (g.starts[(size_t) i], cs.next (rng), 38 + (int) (82.0f * t));
                }
                break;
            }
        }
    }

    // Тарелки на всю фразу: ведущий голос + акценты (крэши, чайна, сплэш).
    HitVec buildCymbals (const LeadTable& table, Part part, const Grid& g, int bars, const Prof& f, bool halfFeel,
                         const std::vector<std::vector<int>>& kicks, juce::Random& rng)
    {
        const int spb = g.spb;
        HitVec out;
        CrashSeq cs;

        const LeadCfg a = makeLeadCfg (pickWeighted (table, rng), halfFeel, rng);
        LeadCfg b = makeLeadCfg (pickWeighted (table, rng), halfFeel, rng);
        const bool useB = b.lead != a.lead && rng.nextFloat() < 0.4f;

        for (int bar = 0; bar < bars; ++bar)
        {
            const auto& cfg = (useB && bar >= 2) ? b : a;
            // в песнях в ~15% тактов тарелок почти нет (паузы, акценты): иногда пропускаем ведущий голос
            if (bar > 0 && cfg.lead != Lead::CrashSwell && rng.nextFloat() < 0.10f)
                continue;
            layLead (out, cfg, g, bar * spb, bar, bars, rng, cs);
        }

        const bool intense = part == Part::Chorus || part == Part::Breakdown;

        auto accent = [&] (int step, Art art, int vel)
        {
            eraseAt (out, step);
            out.push_back ({ step, art, jit (rng, vel, 3) });
        };

        auto downbeat = [&] (bool allowChina)
        {
            if (allowChina && intense && rng.nextFloat() < (halfFeel ? 0.22f : 0.12f))
                return Art::China;
            return cs.next (rng);
        };

        if (rng.nextFloat() < f.crash1)
            accent (0, downbeat (true), 120);

        for (int bar = 1; bar < bars; ++bar)
        {
            const float p = bar == 2 ? juce::jmax (f.crash3, f.crashEvery) : f.crashEvery;
            if (rng.nextFloat() < p)
                accent (bar * spb, downbeat (true), 116);
        }

        // сплэш (редко) или чайна на "и" последней доли
        if (rng.nextFloat() < f.endAccent && spb >= 4)
            accent ((bars - 1) * spb + spb - 2, rng.nextFloat() < 0.2f ? Art::Splash : Art::China, 108);

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

    // ================================================================ томы, филлы, нарастания
    // Томовые клише post-metal: 1 "сердцебиение": напольный том на каждой доле; 2 "племенной": восьмые по двум томам;
    // 3 "тресильо": кусочки по 2-4 шага по кругу томов.
    void addTribalToms (HitVec& toms, const Grid& g, int bars, int kind, float vol, juce::Random& rng)
    {
        const int spb = g.spb;
        static const Art circle[] = { Art::TomFloor1, Art::TomRack2, Art::TomFloor1, Art::TomFloor2, Art::TomRack1 };
        auto v = [&] (int x) { return jit (rng, (int) ((float) x * vol)); };

        for (int bar = 0; bar < bars; ++bar)
        {
            const int off = bar * spb;

            if (kind == 1)
            {
                for (int s : g.starts)
                    toms.push_back ({ off + s, (s == g.starts.back() && bar % 2 == 1) ? Art::TomFloor2 : Art::TomFloor1,
                                      v (s == 0 ? 104 : 88) });
            }
            else if (kind == 2)
            {
                for (int s = 0; s < spb; s += 2)
                {
                    if (! g.isStart (s) && rng.nextFloat() < 0.12f)
                        continue;
                    const bool low = (s / 2) % 2 == 0;
                    toms.push_back ({ off + s, low ? Art::TomFloor1 : Art::TomRack2, v (g.isStart (s) ? 106 : 84) });
                }
            }
            else
            {
                const auto pos = randomSplit (spb, rng);
                int k = 0;
                for (int s : pos)
                    toms.push_back ({ off + s, circle[(size_t) (k++ % 5)], v (s == 0 ? 108 : 92) });
            }

            // в последнем такте томы нарастают
            if (bar == bars - 1 && spb >= 8 && rng.nextFloat() < 0.6f)
                for (int s = spb - 4; s < spb; ++s)
                    toms.push_back ({ off + s, circle[(size_t) (s % 5)], juce::jlimit (1, 127, (int) ((float) (70 + (s - (spb - 4)) * 14) * vol)) });
        }
    }

    // Филл: с позиции start всё заменяется на снейр + тома сверху вниз.
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

    // Завершение фразы: филл в конце, иногда пауза перед следующей секцией.
    void finishPhrase (Lanes& L, const Grid& g, int bars, const Prof& f, juce::Random& rng)
    {
        const int spb = g.spb;
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
        else if (rng.nextFloat() < f.pause)
        {
            for (auto& lane : L)
                eraseRange (lane, (bars - 1) * spb + (spb * 3) / 4, bars * spb);
        }
    }

    // ================================================================ рецепты: сборка дорожек
    // Бласты. Традиционный: кик и снейр по очереди, тарелка вместе с киком; хаммер: всё вместе;
    // бомб: кик вдвое чаще остальных; скэнк: кик на долях, снейр на "и", тарелка на каждый удар.
    Lanes blastLanes (Feel feel, const Grid& g, int bars, const Prof& f, juce::Random& rng)
    {
        Lanes L;
        const int spb = g.spb;
        CrashSeq cs;

        // голос тарелки на всю фразу: райд, хэт, крэш («стена звука») или чайна
        const int voice = pickWeighted<int> ({ { 0, .50f }, { 1, .20f }, { 2, .20f }, { 3, .10f } }, rng);

        for (int bar = 0; bar < bars; ++bar)
        {
            Feel k = feel;
            if (bar == bars - 1 && rng.nextFloat() < 0.25f)   // смена бласта в последнем такте
                k = feel == Feel::BlastTrad ? Feel::BlastHammer : (feel == Feel::BlastHammer ? Feel::BlastBomb
                                                                  : (feel == Feel::BlastBomb ? Feel::BlastHammer : Feel::BlastTrad));

            Art crashCur = Art::CrashLeft;
            for (int s = 0; s < spb; ++s)
            {
                const int abs = bar * spb + s;
                bool kick = false, snare = false, cym = false;

                switch (k)
                {
                    case Feel::BlastTrad:   kick = s % 2 == 0; snare = s % 2 == 1; cym = kick; break;
                    case Feel::BlastHammer: kick = snare = cym = s % 2 == 0; break;
                    case Feel::BlastBomb:   kick = true; snare = cym = s % 2 == 0; break;
                    case Feel::Skank:
                    case Feel::Backbeat:
                    case Feel::DoubleBass:
                    case Feel::Gallop:
                    case Feel::Chug:
                    case Feel::TwoStep:
                    case Feel::HalfTime:
                    case Feel::Bounce:
                    case Feel::Doom:
                    case Feel::Tribal:
                    case Feel::Layered:
                    case Feel::BuildUp:
                    case Feel::Atmos:   // скэнк (остальные сюда не попадают)
                    {
                        for (int i = 0; i < g.groups(); ++i)
                        {
                            const int st = g.starts[(size_t) i];
                            if (s == st) kick = true;
                            if (s == st + g.lens[(size_t) i] / 2) snare = true;
                        }
                        cym = kick || snare;
                        break;
                    }
                }

                if (kick)  L[(size_t) Lane::Kick].push_back  ({ abs, Art::Kick,  jit (rng, 104, 6) });
                if (snare) L[(size_t) Lane::Snare].push_back ({ abs, Art::Snare, jit (rng, 112, 6) });

                if (cym)
                {
                    Art a = Art::RideBow;
                    if (voice == 1) a = Art::HatClosedTip;
                    else if (voice == 2) { if (g.isStart (s)) crashCur = cs.next (rng); a = crashCur; }
                    else if (voice == 3) a = Art::China;
                    else if (g.isStart (s) && rng.nextFloat() < 0.12f) a = Art::RideBell;   // акцент колоколом
                    L[(size_t) Lane::Cymbals].push_back ({ abs, a, jit (rng, g.isStart (s) ? 100 : 90, 5) });
                }
            }

            // крэш на первой доле такта
            const float p = bar == 0 ? f.crash1 : (bar == 2 ? juce::jmax (f.crash3, f.crashEvery) : f.crashEvery);
            if (rng.nextFloat() < p)
            {
                eraseAt (L[(size_t) Lane::Cymbals], bar * spb);
                L[(size_t) Lane::Cymbals].push_back ({ bar * spb, cs.next (rng), jit (rng, 120, 3) });
            }
        }

        return L;
    }

    // Нарастание: ролл снейра с усилением громкости и нарастающей плотностью.
    Lanes buildUpLanes (const Grid& g, int bars, juce::Random& rng)
    {
        Lanes L;
        const int spb = g.spb;
        static const int dens[3][4] = { { 4, 2, 2, 1 }, { 2, 2, 1, 1 }, { 4, 4, 2, 1 } };
        const int* d = dens[rng.nextInt (3)];

        int total = 0;
        for (int bar = 0; bar < bars; ++bar)
            total += (spb + d[juce::jmin (bar, 3)] - 1) / d[juce::jmin (bar, 3)];

        int n = 0;
        for (int bar = 0; bar < bars; ++bar)
        {
            const int step = d[juce::jmin (bar, 3)];
            for (int s = 0; s < spb; s += step)
            {
                const float t = (float) n++ / (float) juce::jmax (1, total - 1);
                L[(size_t) Lane::Snare].push_back ({ bar * spb + s, Art::Snare, jit (rng, 48 + (int) (74.0f * t), 3) });
            }
            // кик усиливается четвертями во второй половине
            if (bar >= bars / 2)
                for (int s : g.starts)
                    L[(size_t) Lane::Kick].push_back ({ bar * spb + s, Art::Kick, jit (rng, 80 + bar * 8, 4) });
            else if (rng.nextFloat() < 0.6f)
                L[(size_t) Lane::Kick].push_back ({ bar * spb, Art::Kick, jit (rng, 96, 4) });
        }

        // тарелки: свелл крэшей или хэт четвертями
        CrashSeq cs;
        if (rng.nextFloat() < 0.6f)
        {
            const LeadCfg c { Lead::CrashSwell, 0 };
            for (int bar = 0; bar < bars; ++bar)
                layLead (L[(size_t) Lane::Cymbals], c, g, bar * spb, bar, bars, rng, cs);
        }
        else
        {
            const LeadCfg c { Lead::HatQuarter, 0 };
            for (int bar = 0; bar < bars; ++bar)
                layLead (L[(size_t) Lane::Cymbals], c, g, bar * spb, bar, bars, rng, cs);
        }
        return L;
    }

    // Атмосфера: свеллы тарелок, редкий кик, мягкие томы.
    Lanes atmosLanes (const Grid& g, int bars, const Prof& f, juce::Random& rng)
    {
        Lanes L;
        const int spb = g.spb;
        CrashSeq cs;

        const LeadCfg c { rng.nextFloat() < 0.6f ? Lead::CrashSwell : Lead::RideBell, 0 };
        for (int bar = 0; bar < bars; ++bar)
            layLead (L[(size_t) Lane::Cymbals], c, g, bar * spb, bar, bars, rng, cs);

        for (int bar = 0; bar < bars; bar += rng.nextFloat() < 0.5f ? 1 : 2)
            L[(size_t) Lane::Kick].push_back ({ bar * spb, Art::Kick, jit (rng, 92, 4) });

        if (rng.nextFloat() < 0.7f)
            addTribalToms (L[(size_t) Lane::Toms], g, bars, 1, 0.65f, rng);

        if (rng.nextFloat() < f.crash1)
        {
            eraseAt (L[(size_t) Lane::Cymbals], 0);
            L[(size_t) Lane::Cymbals].push_back ({ 0, cs.next (rng), 118 });
        }
        return L;
    }

    // Выбор режима кика по рецепту.
    KickMode kickModeFor (Feel feel, Style style, juce::Random& rng)
    {
        switch (feel)
        {
            case Feel::Backbeat:   return style == Style::IndieRock ? KickMode::Rock
                                                                     : (rng.nextFloat() < 0.5f ? KickMode::Rock : KickMode::Gallop);
            case Feel::DoubleBass: return KickMode::Pulse16;
            case Feel::Gallop:     return KickMode::Gallop;
            case Feel::Chug:       return KickMode::Chug;
            case Feel::TwoStep:    return KickMode::HcTwoStep;
            case Feel::HalfTime:   return style == Style::PostMetal || style == Style::IndieRock ? KickMode::Slow
                                                                                                   : (rng.nextFloat() < 0.5f ? KickMode::Slow : KickMode::Chug);
            case Feel::Bounce:     return KickMode::Bounce;
            case Feel::Doom:       return KickMode::Slow;
            case Feel::Tribal:     return KickMode::Slow;
            case Feel::Layered:    return KickMode::Slow;
            case Feel::Atmos:      return KickMode::Sparse;
            case Feel::Skank:
            case Feel::BlastTrad:
            case Feel::BlastHammer:
            case Feel::BlastBomb:
            case Feel::BuildUp:    break;
        }
        return KickMode::Rock;
    }

    // Обычные грувы: кик + снейр + ведущие тарелки; фраза строится по форме (AAAA', AAAB, ABAB, AABB, ABAC).
    Lanes grooveLanes (Feel feel, Style style, Part part, const Grid& g, int bars, const Prof& f, juce::Random& rng)
    {
        Lanes L;
        const int spb = g.spb;
        const KickMode km = kickModeFor (feel, style, rng);

        const auto kA = kickSteps (km, f, g, rng);
        const auto kB = kickSteps (km, f, g, rng);

        const auto fam = snareFamily (feel, part, style);
        const SnVar svA = pickWeighted (fam, rng);
        const SnVar svB = rng.nextFloat() < 0.35f ? pickWeighted (fam, rng) : svA;

        // 0 = A, 1 = B, 2 = A с мутацией
        static const int forms[5][4] = { { 0, 0, 2, 2 }, { 0, 0, 0, 1 }, { 0, 1, 0, 1 }, { 0, 0, 1, 1 }, { 0, 1, 0, 2 } };
        const int form = pickWeighted<int> ({ { 0, .28f }, { 1, .26f }, { 2, .20f }, { 3, .10f }, { 4, .16f } }, rng);

        std::vector<std::vector<int>> kicks ((size_t) bars);
        for (int bar = 0; bar < bars; ++bar)
        {
            const int w = forms[form][juce::jmin (bar, 3)];
            auto k = w == 1 ? kB : kA;
            if (w == 2)
                mutateKicks (k, g, rng, bar == bars - 1 ? 2 : 1);
            kicks[(size_t) bar] = k;

            HitVec kickAbs;
            for (int s : k)
                kickAbs.push_back ({ bar * spb + s, Art::Kick, jit (rng, s == 0 ? 114 : 102, 6) });

            snareBar (L[(size_t) Lane::Snare], kickAbs, w == 1 ? svB : svA, g, bar * spb, rng);
            for (const auto& h : kickAbs)
                L[(size_t) Lane::Kick].push_back (h);
        }

        L[(size_t) Lane::Cymbals] = buildCymbals (leadTableFor (feel, style, part), part, g, bars, f, isHalfVar (svA), kicks, rng);
        return L;
    }

    Lanes feelLanes (Feel feel, Style style, Part part, const Grid& g, int bars, const Prof& f, juce::Random& rng)
    {
        const int spb = g.spb;
        Lanes L;

        switch (feel)
        {
            case Feel::BlastTrad:
            case Feel::BlastHammer:
            case Feel::BlastBomb:
            case Feel::Skank:
                L = blastLanes (feel, g, bars, f, rng);
                break;

            case Feel::BuildUp:
                return buildUpLanes (g, bars, rng);       // без дополнительных филлов: нарастание само ведёт в следующую часть

            case Feel::Atmos:
                L = atmosLanes (g, bars, f, rng);
                break;

            case Feel::Tribal:
            {
                L = grooveLanes (feel, style, part, g, bars, f, rng);
                addTribalToms (L[(size_t) Lane::Toms], g, bars, 1 + rng.nextInt (3), 1.0f, rng);
                break;
            }

            case Feel::Layered:
            {
                // слои: бар 1 только кик, бар 2 + томы, бар 3 + тарелки, бар 4 всё вместе
                L = grooveLanes (feel, style, part, g, bars, f, rng);
                addTribalToms (L[(size_t) Lane::Toms], g, bars, 1 + rng.nextInt (2), 0.9f, rng);
                L[(size_t) Lane::Cymbals].clear();
                {
                    const LeadCfg c { rng.nextFloat() < 0.5f ? Lead::CrashSwell : Lead::RideBell, 0 };
                    CrashSeq cs;
                    for (int bar = 0; bar < bars; ++bar)
                        layLead (L[(size_t) Lane::Cymbals], c, g, bar * spb, bar, bars, rng, cs);
                }
                const int snareFrom = juce::jmin (2, bars - 1) * spb;
                const int tomsFrom = spb;
                const int cymFrom = juce::jmin (2, bars - 1) * spb;
                eraseRange (L[(size_t) Lane::Toms], 0, tomsFrom);
                eraseRange (L[(size_t) Lane::Cymbals], 0, cymFrom);
                eraseRange (L[(size_t) Lane::Snare], 0, snareFrom);
                break;
            }

            case Feel::Backbeat:
            case Feel::DoubleBass:
            case Feel::Gallop:
            case Feel::Chug:
            case Feel::TwoStep:
            case Feel::HalfTime:
            case Feel::Bounce:
            case Feel::Doom:
                L = grooveLanes (feel, style, part, g, bars, f, rng);
                break;
        }

        finishPhrase (L, g, bars, f, rng);
        return L;
    }

    // ================================================================ фразы из присланных грувов
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

    int styleBit (Style s)
    {
        switch (s)
        {
            case Style::Djent:      return 1;
            case Style::Metalcore:  return 2;
            case Style::IndieRock:  return 4;
            case Style::PostMetal:  return 16;
            case Style::Deathcore:  return 32;
            case Style::BlackMetal: return 64;
        }
        return 1;
    }

    const CorpusPhrase* pickExact (Part part, Style style, int bars, juce::Random& rng)
    {
        const int bit = styleBit (style);
        std::vector<const CorpusPhrase*> pool;
        for (int i = 0; i < kCorpusSize; ++i)
        {
            const auto& p = kCorpus[i];
            if (p.part == (int) part && (p.styleMask & bit) != 0 && (bars < 0 || p.bars == bars))
                pool.push_back (&p);
        }
        // слишком маленький набор быстро надоедает: тогда лучше рецепты, чем одни и те же 2-3 фразы
        if (pool.size() < (bars < 0 ? 4u : 8u))
            return nullptr;
        return pool[(size_t) rng.nextInt ((int) pool.size())];
    }

    // Если для стиля нет фраз нужной части, берём ближайшую по характеру.
    const CorpusPhrase* pickPhrase (Part part, Style style, int bars, juce::Random& rng)
    {
        static const Part fallback[kNumParts][3] = {
            { Part::Intro,     Part::Verse,  Part::Verse },
            { Part::Verse,     Part::Chorus, Part::Verse },
            { Part::Bridge,    Part::Intro,  Part::Verse },
            { Part::Chorus,    Part::Verse,  Part::Chorus },
            { Part::Fill,      Part::Fill,   Part::Fill },
            { Part::Breakdown, Part::Chorus, Part::Verse } };

        // у black-metal в базе только бласты: для мостов и «брейкдаунов» (медленные части) берём рецепты, а не бласты
        const int tries = style == Style::BlackMetal ? 1 : 3;
        for (int i = 0; i < tries; ++i)
            if (const auto* ph = pickExact (fallback[(int) part][i], style, bars, rng))
                return ph;
        return nullptr;
    }

    bool corpusUsable (int num, int den)
    {
        return num == 4 && den == 4 && kCorpusSize > 0;
    }

    bool isGhost (const Hit& h) { return h.art == Art::Snare && h.velocity < 75; }

    // Полутайм ли снейр: основные удары на третьей доле, а не на второй и четвёртой.
    bool isHalfFeel (const HitVec& snare)
    {
        int half = 0, back = 0;
        for (const auto& h : snare)
        {
            if (h.velocity < 75)
                continue;
            const int s = h.step % 16;
            if (s == 8) ++half;
            else if (s == 4 || s == 12) ++back;
        }
        return half > back;
    }

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

        // кик: иногда из другой фразы с тем же ощущением (полутайм/бэкбит)
        if (rng.nextFloat() < 0.22f)
            if (const auto* other = pickPhrase (src, style, 4, rng))
            {
                Lanes tmp;
                decodePhrase (*other, 0, tmp, rng);
                if (isHalfFeel (tmp[(size_t) Lane::Snare]) == isHalfFeel (L[(size_t) Lane::Snare]) && ! tmp[(size_t) Lane::Kick].empty())
                    L[(size_t) Lane::Kick] = tmp[(size_t) Lane::Kick];
            }

        const Grid g = makeGrid (4, 4);
        const Prof f = makeProfile (style, part);

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
            std::vector<std::vector<int>> kicks (4);
            for (const auto& h : L[(size_t) Lane::Kick])
                kicks[(size_t) (h.step / 16)].push_back (h.step % 16);
            L[(size_t) Lane::Cymbals] = buildCymbals (leadTable (style, part), part, g, 4, f,
                                                      isHalfFeel (L[(size_t) Lane::Snare]), kicks, rng);
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

    // ================================================================ сборка
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

    // Начало филла в последнем такте (если он там есть): томов заметно больше, чем в первом такте.
    int detectFillStart (const Pattern& p)
    {
        const int spb = p.stepsPerBar();
        const int last = (p.bars - 1) * spb;
        int countLast = 0, countFirst = 0, first = -1;
        for (const auto& h : p.hits)
        {
            if (laneOf (h.art) != Lane::Toms)
                continue;
            if (h.step >= last)
            {
                ++countLast;
                if (first < 0 || h.step < first)
                    first = h.step;
            }
            else if (h.step < spb)
            {
                ++countFirst;
            }
        }
        return (countLast >= 3 && countLast > countFirst + 2) ? first : -1;
    }

    // Бласт ли это: в первом такте плотный снейр.
    bool isBlastish (const Pattern& p)
    {
        int n = 0;
        for (const auto& h : p.hits)
            if (h.step < p.stepsPerBar() && h.art == Art::Snare && h.velocity >= 75)
                ++n;
        return n >= 6 || (n >= 4 && p.stepsPerBar() <= 8);
    }

    KickMode randomKickMode (Style style, bool half, juce::Random& rng)
    {
        using K = KickMode;
        std::vector<std::pair<K, float>> t;
        switch (style)
        {
            case Style::Djent:      t = { { K::Chug, .55f }, { K::Gallop, .10f }, { K::Pulse16, .15f }, { K::Rock, .10f }, { K::Bounce, .10f } }; break;
            case Style::Metalcore:  t = { { K::Gallop, .35f }, { K::Pulse16, .20f }, { K::Chug, .15f }, { K::HcTwoStep, .15f }, { K::Rock, .15f } }; break;
            case Style::Deathcore:  t = { { K::Pulse16, .30f }, { K::Bounce, .25f }, { K::Chug, .20f }, { K::Gallop, .15f }, { K::Slow, .10f } }; break;
            case Style::BlackMetal: t = { { K::Pulse16, .35f }, { K::Gallop, .30f }, { K::Slow, .20f }, { K::Rock, .15f } }; break;
            case Style::PostMetal:  t = { { K::Slow, .50f }, { K::Chug, .20f }, { K::Rock, .15f }, { K::Sparse, .15f } }; break;
            case Style::IndieRock:  t = { { K::Rock, .70f }, { K::HcTwoStep, .15f }, { K::Sparse, .15f } }; break;
        }
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const K k = pickWeighted (t, rng);
            const bool halfOk = k == K::Chug || k == K::Slow || k == K::Bounce || k == K::Sparse;
            if (half ? halfOk : (k != K::Slow && k != K::Sparse && k != K::Bounce))
                return k;
        }
        return half ? K::Chug : K::Rock;
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

    // примерно в половине случаев берём реальную фразу из базы (если для стиля и части она есть)
    if (corpusUsable (num, den) && rng.nextFloat() < 0.5f)
        L = corpusLanes (style, part, rng, done);

    if (! done)
    {
        const Grid g = makeGrid (num, den);
        const Prof f = makeProfile (style, part);
        const Feel feel = pickWeighted (feelTable (style, part), rng);
        L = feelLanes (feel, style, part, g, 4, f, rng);
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
    const bool blast = isBlastish (cur);

    Lanes L = splitLanes (cur);
    const bool half = isHalfFeel (L[(size_t) Lane::Snare]);
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
        if (blast)
        {
            // варианты бласта: по нечётным шестнадцатым / по чётным (вместе с киком) / на «и» (скэнк)
            const int kind = rng.nextInt (3);
            for (int bar = 0; bar < bars; ++bar)
                for (int s = 0; s < spb; ++s)
                {
                    bool on = false;
                    if (kind == 0) on = s % 2 == 1;
                    else if (kind == 1) on = s % 2 == 0;
                    else
                        for (int i = 0; i < g.groups(); ++i)
                            if (s == g.starts[(size_t) i] + g.lens[(size_t) i] / 2)
                                on = true;
                    if (on && bar * spb + s < limit)
                        fresh.push_back ({ bar * spb + s, Art::Snare, jit (rng, 112, 6) });
                }
        }
        else
        {
            const SnVar sv = pickWeighted (snareFamilyFor (half, cur.part, cur.style), rng);
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
    }
    else if (lane == Lane::Cymbals)
    {
        bool done = false;

        if (blast)
        {
            // тарелка вместе с киком или с каждым ударом, голос — райд, хэт, крэш или чайна
            const bool onAll = rng.nextBool();
            const int voice = pickWeighted<int> ({ { 0, .50f }, { 1, .20f }, { 2, .20f }, { 3, .10f } }, rng);
            CrashSeq cs;
            Art crashCur = Art::CrashLeft;
            std::vector<int> steps;
            for (const auto& h : L[(size_t) Lane::Kick])
                steps.push_back (h.step);
            if (onAll)
                for (const auto& h : L[(size_t) Lane::Snare])
                    steps.push_back (h.step);
            std::sort (steps.begin(), steps.end());
            steps.erase (std::unique (steps.begin(), steps.end()), steps.end());

            int last = -100;
            for (int s : steps)
            {
                if (s - last < 2)       // не чаще восьмых
                    continue;
                last = s;
                Art a = Art::RideBow;
                if (voice == 1) a = Art::HatClosedTip;
                else if (voice == 2) { if (g.isStart (s % spb)) crashCur = cs.next (rng); a = crashCur; }
                else if (voice == 3) a = Art::China;
                fresh.push_back ({ s, a, jit (rng, g.isStart (s % spb) ? 100 : 90, 5) });
            }
            done = true;
        }

        if (! done && corpusUsable (cur.numerator, cur.denominator) && rng.nextFloat() < 0.45f)
        {
            const Part src = cur.part == Part::Fill ? Part::Verse : cur.part;
            if (const auto* ph = pickPhrase (src, cur.style, 4, rng))
            {
                Lanes tmp;
                decodePhrase (*ph, 0, tmp, rng);
                fresh = tmp[(size_t) Lane::Cymbals];
                done = true;
            }
        }

        if (! done)
        {
            std::vector<std::vector<int>> kicks ((size_t) bars);
            for (const auto& h : L[(size_t) Lane::Kick])
                kicks[(size_t) juce::jlimit (0, bars - 1, h.step / spb)].push_back (h.step % spb);

            // ведущий голос подбираем под текущее ощущение
            const Feel lf = half ? Feel::HalfTime : Feel::Backbeat;
            fresh = buildCymbals (leadTableFor (lf, cur.style, cur.part), cur.part, g, bars, f, half, kicks, rng);
        }
    }
    else if (lane == Lane::Kick)
    {
        if (blast)
        {
            // относительно снейра: по очереди / вместе с ним / каждая шестнадцатая
            const int kind = rng.nextInt (3);
            for (int bar = 0; bar < bars; ++bar)
                for (int s = 0; s < spb; ++s)
                {
                    const int abs = bar * spb + s;
                    const bool sn = hasAt (L[(size_t) Lane::Snare], abs);
                    bool on = kind == 0 ? ! sn : (kind == 1 ? sn : true);
                    if (kind == 0 && s % 2 == 1)
                        on = false;
                    if (on && abs < limit)
                        fresh.push_back ({ abs, Art::Kick, jit (rng, 104, 6) });
                }
        }
        else
        {
            bool fromCorpus = false;
            if (corpusUsable (cur.numerator, cur.denominator) && rng.nextFloat() < 0.5f)
            {
                const Part src = cur.part == Part::Fill ? Part::Verse : cur.part;
                if (const auto* ph = pickPhrase (src, cur.style, 4, rng))
                {
                    Lanes tmp;
                    decodePhrase (*ph, 0, tmp, rng);
                    if (isHalfFeel (tmp[(size_t) Lane::Snare]) == half && ! tmp[(size_t) Lane::Kick].empty())
                    {
                        fresh = tmp[(size_t) Lane::Kick];
                        fromCorpus = true;
                    }
                }
            }

            if (! fromCorpus)
            {
                const KickMode km = randomKickMode (cur.style, half, rng);
                const auto base = kickSteps (km, f, g, rng);
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
    }
    else // Toms
    {
        const float r = rng.nextFloat();
        if (r < 0.55f)
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
        else if (r < 0.80f)
        {
            // племенной томовый грув на всю фразу
            addTribalToms (fresh, g, bars, 1 + rng.nextInt (3), 0.95f, rng);
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
