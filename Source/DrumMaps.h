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

    int noteFor (Art a) const { return note[(size_t) a]; }
};

// Индекс 0 — раскладка по умолчанию (Modern & Massive 2).
// Чтобы добавить библиотеку, допиши функцию в DrumMaps.cpp и добавь её в builtinMaps().
const std::vector<DrumMap>& builtinMaps();
} // namespace drums
