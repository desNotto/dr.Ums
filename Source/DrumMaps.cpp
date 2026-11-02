#include "DrumMaps.h"

namespace drums
{
namespace
{
    DrumMap makeMap (const char* name, std::initializer_list<std::pair<Art, int>> notes)
    {
        DrumMap m;
        m.name = juce::String::fromUTF8 (name);
        m.note.fill (-1);
        for (const auto& n : notes)
            m.note[(size_t) n.first] = n.second;
        return m;
    }

    std::vector<DrumMap> buildMaps()
    {
        // Номера нот взяты из присланных файлов (формат Reaper) и со скриншотов вкладки Mapping.
        // GGD Fusion и P4 это старые библиотеки для Kontakt, у них своя нумерация (кик на 24).
        // Если голоса в библиотеке нет, DrumMap::noteFor подставляет ближайший по звучанию
        // (например, нет рим-клика: играет снейр; нет чайны: играет крэш).
        return
        {
        // Modern & Massive 2: раскладка по скриншотам вкладки Mapping (кик на C1 = нота 36); проверена на слух.
        makeMap ("Modern & Massive 2",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::SnareCross, 37 }, { Art::TomRack1, 42 }, { Art::TomRack2, 43 },
            { Art::TomFloor1, 45 }, { Art::TomFloor2, 47 }, { Art::HatClosedTip, 51 }, { Art::HatClosedEdge, 52 },
            { Art::HatOpen1, 53 }, { Art::HatOpen2, 54 }, { Art::HatOpen3, 55 }, { Art::HatPedal, 56 },
            { Art::RideBow, 68 }, { Art::RideBell, 69 }, { Art::CrashFarLeft, 58 }, { Art::CrashLeft, 60 },
            { Art::CrashRight, 64 }, { Art::China, 74 }, { Art::Splash, 76 }
        }),
        makeMap ("GM",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::SnareCross, 37 }, { Art::TomRack1, 50 },
            { Art::TomRack2, 48 }, { Art::TomFloor1, 45 }, { Art::TomFloor2, 43 }, { Art::HatClosedTip, 42 },
            { Art::HatClosedEdge, 42 }, { Art::HatOpen1, 46 }, { Art::HatOpen2, 46 }, { Art::HatOpen3, 46 },
            { Art::HatPedal, 44 }, { Art::RideBow, 51 }, { Art::RideBell, 53 }, { Art::CrashFarLeft, 49 },
            { Art::CrashLeft, 49 }, { Art::CrashRight, 57 }, { Art::China, 52 }, { Art::Splash, 55 }
        }),
        makeMap ("GGD Fusion",
        {
            { Art::Kick, 24 }, { Art::Snare, 26 }, { Art::SnareCross, 30 }, { Art::TomRack1, 33 },
            { Art::TomRack2, 34 }, { Art::TomFloor1, 35 }, { Art::HatClosedTip, 43 }, { Art::HatClosedEdge, 44 },
            { Art::HatOpen1, 45 }, { Art::HatOpen2, 46 }, { Art::HatOpen3, 47 }, { Art::HatPedal, 48 },
            { Art::RideBow, 62 }, { Art::RideBell, 61 }, { Art::CrashLeft, 52 }, { Art::CrashRight, 54 },
            { Art::China, 65 }, { Art::Splash, 73 }
        }),
        makeMap ("GGD P4",
        {
            { Art::Kick, 24 }, { Art::Snare, 26 }, { Art::SnareCross, 30 }, { Art::TomRack1, 33 },
            { Art::TomRack2, 34 }, { Art::TomFloor1, 35 }, { Art::TomFloor2, 36 }, { Art::HatClosedTip, 41 },
            { Art::HatClosedEdge, 42 }, { Art::HatOpen1, 43 }, { Art::HatOpen2, 46 }, { Art::HatOpen3, 47 },
            { Art::HatPedal, 44 }, { Art::RideBow, 62 }, { Art::RideBell, 61 }, { Art::CrashFarLeft, 56 },
            { Art::CrashLeft, 54 }, { Art::CrashRight, 52 }, { Art::China, 65 }, { Art::Splash, 73 }
        }),
        makeMap ("RS Monarch",
        {
            { Art::Kick, 24 }, { Art::Snare, 26 }, { Art::SnareCross, 28 }, { Art::TomRack1, 38 },
            { Art::TomRack2, 37 }, { Art::TomFloor1, 36 }, { Art::TomFloor2, 35 }, { Art::HatClosedTip, 41 },
            { Art::HatClosedEdge, 42 }, { Art::HatOpen1, 43 }, { Art::HatOpen2, 45 }, { Art::HatOpen3, 47 },
            { Art::HatPedal, 40 }, { Art::RideBow, 62 }, { Art::RideBell, 53 }, { Art::CrashFarLeft, 58 },
            { Art::CrashLeft, 49 }, { Art::CrashRight, 54 }, { Art::China, 56 }, { Art::Splash, 51 }
        }),
        makeMap ("Buster",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::TomRack1, 47 }, { Art::TomRack2, 45 },
            { Art::TomFloor1, 43 }, { Art::TomFloor2, 41 }, { Art::HatClosedTip, 51 },
            { Art::HatClosedEdge, 52 }, { Art::HatOpen1, 50 }, { Art::HatOpen2, 49 }, { Art::HatOpen3, 48 },
            { Art::HatPedal, 53 }, { Art::RideBow, 58 }, { Art::RideBell, 59 }, { Art::CrashFarLeft, 61 },
            { Art::CrashLeft, 54 }, { Art::CrashRight, 56 }, { Art::China, 63 }, { Art::Splash, 68 }
        }),
        makeMap ("Josh Middleton",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::TomRack1, 48 }, { Art::TomRack2, 47 },
            { Art::TomFloor1, 41 }, { Art::TomFloor2, 40 }, { Art::HatClosedTip, 62 },
            { Art::HatClosedEdge, 61 }, { Art::HatOpen1, 59 }, { Art::HatOpen2, 60 }, { Art::HatPedal, 44 },
            { Art::RideBow, 51 }, { Art::RideBell, 53 }, { Art::CrashFarLeft, 66 }, { Art::CrashLeft, 49 },
            { Art::CrashRight, 57 }, { Art::China, 52 }, { Art::Splash, 55 }
        }),
        makeMap ("Krimh",
        {
            { Art::Kick, 36 }, { Art::Snare, 40 }, { Art::TomRack1, 48 }, { Art::TomRack2, 47 },
            { Art::TomFloor1, 45 }, { Art::TomFloor2, 43 }, { Art::HatClosedTip, 42 }, { Art::HatOpen1, 77 },
            { Art::HatOpen2, 78 }, { Art::HatOpen3, 79 }, { Art::HatPedal, 81 }, { Art::RideBow, 51 },
            { Art::RideBell, 60 }, { Art::CrashFarLeft, 50 }, { Art::CrashLeft, 49 }, { Art::CrashRight, 57 },
            { Art::China, 52 }, { Art::Splash, 55 }
        }),
        makeMap ("Mixwave - Gojira",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::SnareCross, 37 }, { Art::TomRack1, 48 },
            { Art::TomRack2, 47 }, { Art::TomFloor1, 43 }, { Art::TomFloor2, 41 }, { Art::HatClosedTip, 75 },
            { Art::HatClosedEdge, 76 }, { Art::HatOpen1, 77 }, { Art::HatOpen2, 78 }, { Art::HatOpen3, 79 },
            { Art::HatPedal, 44 }, { Art::RideBow, 51 }, { Art::RideBell, 53 }, { Art::CrashLeft, 49 },
            { Art::CrashRight, 57 }, { Art::China, 52 }, { Art::Splash, 55 }
        }),
        makeMap ("Mixwave - Lorna Shore",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::SnareCross, 37 }, { Art::TomRack1, 48 },
            { Art::TomRack2, 47 }, { Art::TomFloor1, 43 }, { Art::TomFloor2, 41 }, { Art::HatClosedTip, 75 },
            { Art::HatClosedEdge, 76 }, { Art::HatOpen1, 77 }, { Art::HatOpen2, 78 }, { Art::HatOpen3, 79 },
            { Art::HatPedal, 44 }, { Art::RideBow, 51 }, { Art::RideBell, 53 }, { Art::CrashFarLeft, 60 },
            { Art::CrashLeft, 49 }, { Art::CrashRight, 57 }, { Art::China, 52 }, { Art::Splash, 55 }
        }),
        makeMap ("Mixwave - Sleep Token",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::SnareCross, 37 }, { Art::TomRack1, 48 },
            { Art::TomRack2, 47 }, { Art::TomFloor1, 43 }, { Art::TomFloor2, 41 }, { Art::HatClosedTip, 75 },
            { Art::HatClosedEdge, 76 }, { Art::HatOpen1, 77 }, { Art::HatOpen2, 78 }, { Art::HatOpen3, 79 },
            { Art::HatPedal, 44 }, { Art::RideBow, 51 }, { Art::RideBell, 53 }, { Art::CrashFarLeft, 54 },
            { Art::CrashLeft, 49 }, { Art::CrashRight, 57 }, { Art::China, 52 }, { Art::Splash, 55 }
        }),
        makeMap ("Mixwave - Slipknot",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::SnareCross, 37 }, { Art::TomRack1, 48 },
            { Art::TomRack2, 47 }, { Art::TomFloor1, 43 }, { Art::TomFloor2, 41 }, { Art::HatClosedTip, 75 },
            { Art::HatClosedEdge, 76 }, { Art::HatOpen1, 77 }, { Art::HatOpen2, 78 }, { Art::HatOpen3, 79 },
            { Art::HatPedal, 44 }, { Art::RideBow, 51 }, { Art::RideBell, 53 }, { Art::CrashLeft, 49 },
            { Art::CrashRight, 57 }, { Art::China, 52 }, { Art::Splash, 55 }
        }),
        makeMap ("Mixwave - Underøath",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::SnareCross, 37 }, { Art::TomRack1, 47 },
            { Art::TomRack2, 45 }, { Art::TomFloor1, 43 }, { Art::TomFloor2, 41 }, { Art::HatClosedTip, 75 },
            { Art::HatClosedEdge, 76 }, { Art::HatOpen1, 77 }, { Art::HatOpen2, 78 }, { Art::HatOpen3, 79 },
            { Art::HatPedal, 44 }, { Art::RideBow, 51 }, { Art::RideBell, 53 }, { Art::CrashLeft, 49 },
            { Art::CrashRight, 57 }, { Art::China, 52 }, { Art::Splash, 55 }
        }),
        makeMap ("Mixwave - Luke",
        {
            { Art::Kick, 36 }, { Art::Snare, 38 }, { Art::SnareCross, 37 }, { Art::TomRack1, 47 },
            { Art::TomRack2, 45 }, { Art::TomFloor1, 43 }, { Art::TomFloor2, 41 }, { Art::HatClosedTip, 75 },
            { Art::HatClosedEdge, 76 }, { Art::HatOpen1, 77 }, { Art::HatOpen2, 78 }, { Art::HatOpen3, 79 },
            { Art::HatPedal, 44 }, { Art::RideBow, 51 }, { Art::RideBell, 53 }, { Art::CrashLeft, 49 },
            { Art::CrashRight, 57 }, { Art::China, 52 }, { Art::Splash, 55 }
        }),
        };
    }
} // namespace

const std::vector<DrumMap>& builtinMaps()
{
    static const std::vector<DrumMap> maps = buildMaps();
    return maps;
}
} // namespace drums
