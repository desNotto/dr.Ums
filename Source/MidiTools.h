#pragma once

#include "DrumMaps.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace drums
{
constexpr int    kPPQ         = 480;     // тиков на четвертную в .mid
constexpr int    kHitTicks    = 60;      // длина удара: 1/32
constexpr double kHitBeats    = 60.0 / 480.0;
constexpr int    kMidiChannel = 10;      // GM-канал ударных

// Пишет паттерн в .mid (с размером и темпом в метасобытиях).
bool writeMidiFile (const juce::File& file, const Pattern& pattern, const DrumMap& map, double bpm = 120.0);

struct PlayEvent
{
    double beat = 0.0;
    int note = 0;
    int velocity = 100;
    int lane = 0;
};

// События для кнопки Play, отсортированные по времени (в четвертных от начала паттерна).
std::vector<PlayEvent> buildPlayEvents (const Pattern& pattern, const DrumMap& map);
} // namespace drums
