#include "DrumMaps.h"

namespace drums
{
namespace
{
    DrumMap makeMap (const char* name, std::initializer_list<std::pair<Art, int>> notes)
    {
        DrumMap m;
        m.name = name;
        m.note.fill (-1);
        for (const auto& n : notes)
            m.note[(size_t) n.first] = n.second;
        return m;
    }

    // Modern & Massive 2 (GGD), раскладка со скриншотов вкладки Mapping.
    // Нумерация как в GGD: C1 = 36 (C3 = 60), т.е. кик на C1 = 36.
    DrumMap modernAndMassive2()
    {
        return makeMap ("Modern & Massive 2",
        {
            { Art::Kick,          36 },   // C1
            { Art::Snare,         38 },   // D1
            { Art::SnareCross,    37 },   // C#1
            { Art::TomRack1,      42 },   // F#1
            { Art::TomRack2,      43 },   // G1
            { Art::TomFloor1,     45 },   // A1
            { Art::TomFloor2,     47 },   // B1
            { Art::HatClosedTip,  51 },   // D#2
            { Art::HatClosedEdge, 52 },   // E2
            { Art::HatOpen1,      53 },   // F2
            { Art::HatOpen2,      54 },   // F#2
            { Art::HatOpen3,      55 },   // G2
            { Art::HatPedal,      56 },   // G#2 (Pedal Chick)
            { Art::RideBow,       68 },   // G#3
            { Art::RideBell,      69 },   // A3
            { Art::CrashFarLeft,  58 },   // A#2
            { Art::CrashLeft,     60 },   // C3
            { Art::CrashRight,    64 },   // E3
            { Art::China,         74 },   // D4
            { Art::Splash,        76 },   // E4
        });
    }

    // General MIDI, стандартная ударная раскладка.
    DrumMap generalMidi()
    {
        return makeMap ("GM",
        {
            { Art::Kick,          36 },
            { Art::Snare,         38 },
            { Art::SnareCross,    37 },
            { Art::TomRack1,      50 },
            { Art::TomRack2,      48 },
            { Art::TomFloor1,     45 },
            { Art::TomFloor2,     43 },
            { Art::HatClosedTip,  42 },
            { Art::HatClosedEdge, 42 },
            { Art::HatOpen1,      46 },
            { Art::HatOpen2,      46 },
            { Art::HatOpen3,      46 },
            { Art::HatPedal,      44 },
            { Art::RideBow,       51 },
            { Art::RideBell,      53 },
            { Art::CrashFarLeft,  49 },
            { Art::CrashLeft,     49 },
            { Art::CrashRight,    57 },
            { Art::China,         52 },
            { Art::Splash,        55 },
        });
    }
} // namespace

const std::vector<DrumMap>& builtinMaps()
{
    static const std::vector<DrumMap> maps { modernAndMassive2(), generalMidi() };
    return maps;
}
} // namespace drums
