#pragma once

#include "Model.h"

namespace drums
{
// MIDI-раскладка барабанной библиотеки: какой нотой играется каждый голос.
// -1 означает, что в этой раскладке голоса нет (такие удары пропускаются).
struct DrumMap
{
    juce::String name;
    std::array<int, (size_t) kNumArts> note {};

    // Нота для голоса; если голоса нет, берётся ближайший по звучанию (рим-клик -> снейр, чайна -> крэш и т. д.).
    int noteFor (Art a) const
    {
        for (int i = 0; i < 4; ++i)
        {
            const int n = note[(size_t) a];
            if (n >= 0)
                return n;
            const Art next = fallbackOf (a);
            if (next == a)
                break;
            a = next;
        }
        return -1;
    }

    static Art fallbackOf (Art a)
    {
        switch (a)
        {
            case Art::SnareCross:    return Art::Snare;
            case Art::TomRack1:      return Art::TomRack2;
            case Art::TomRack2:      return Art::TomRack1;
            case Art::TomFloor1:     return Art::TomRack2;
            case Art::TomFloor2:     return Art::TomFloor1;
            case Art::HatClosedEdge: return Art::HatClosedTip;
            case Art::HatOpen1:      return Art::HatClosedTip;
            case Art::HatOpen2:      return Art::HatOpen1;
            case Art::HatOpen3:      return Art::HatOpen2;
            case Art::HatPedal:      return Art::HatClosedTip;
            case Art::RideBell:      return Art::RideBow;
            case Art::CrashFarLeft:  return Art::CrashLeft;
            case Art::CrashLeft:     return Art::CrashRight;
            case Art::CrashRight:    return Art::CrashLeft;
            case Art::China:         return Art::CrashRight;
            case Art::Splash:        return Art::CrashLeft;
            case Art::Kick:
            case Art::Snare:
            case Art::HatClosedTip:
            case Art::RideBow:
            case Art::Count:         break;
        }
        return a;
    }
};

// Индекс 0 — раскладка по умолчанию (Modern & Massive 2), индекс 1 — GM, дальше библиотеки из присланных файлов.
// Чтобы добавить библиотеку, допиши функцию в DrumMaps.cpp и добавь её в builtinMaps().
const std::vector<DrumMap>& builtinMaps();
} // namespace drums
