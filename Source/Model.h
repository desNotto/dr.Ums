#pragma once

#include <juce_core/juce_core.h>
#include <algorithm>
#include <array>
#include <vector>

namespace drums
{
enum class Lane  { Kick = 0, Snare, Toms, Cymbals };
constexpr int kNumLanes = 4;

enum class Style { Djent = 0, Metalcore, IndieRock, DrumAndBass };
enum class Part  { Intro = 0, Verse, Bridge, Chorus, Fill, Breakdown };
constexpr int kNumStyles = 4;
constexpr int kNumParts  = 6;

// Абстрактный "голос" барабана. В MIDI-ноту он превращается через DrumMap,
// поэтому паттерн не зависит от выбранной библиотеки.
enum class Art
{
    Kick = 0, Snare, SnareCross,
    TomRack1, TomRack2, TomFloor1, TomFloor2,
    HatClosedTip, HatClosedEdge, HatOpen1, HatOpen2, HatOpen3, HatPedal,
    RideBow, RideBell,
    CrashFarLeft, CrashLeft, CrashRight, China, Splash,
    Count
};
constexpr int kNumArts = (int) Art::Count;

inline Lane laneOf (Art a)
{
    switch (a)
    {
        case Art::Kick:       return Lane::Kick;
        case Art::Snare:
        case Art::SnareCross: return Lane::Snare;
        case Art::TomRack1:
        case Art::TomRack2:
        case Art::TomFloor1:
        case Art::TomFloor2:  return Lane::Toms;
        case Art::HatClosedTip:
        case Art::HatClosedEdge:
        case Art::HatOpen1:
        case Art::HatOpen2:
        case Art::HatOpen3:
        case Art::HatPedal:
        case Art::RideBow:
        case Art::RideBell:
        case Art::CrashFarLeft:
        case Art::CrashLeft:
        case Art::CrashRight:
        case Art::China:
        case Art::Splash:
        case Art::Count:      break;
    }
    return Lane::Cymbals;
}

inline const char* styleName (Style s)
{
    static const char* n[] = { "Djent", "Metalcore", "Indie Rock", "Drum & Bass" };
    return n[(int) s];
}
inline const char* styleSlug (Style s)
{
    static const char* n[] = { "djent", "metalcore", "indierock", "dnb" };
    return n[(int) s];
}
inline const char* partName (Part p)
{
    static const char* n[] = { "Intro", "Verse", "Bridge", "Chorus", "Fill", "Breakdown" };
    return n[(int) p];
}
inline const char* partSlug (Part p)
{
    static const char* n[] = { "intro", "verse", "bridge", "chorus", "fill", "breakdown" };
    return n[(int) p];
}

// Один удар. step — позиция на сетке 1/16 от начала паттерна.
struct Hit
{
    int step = 0;
    Art art = Art::Kick;
    int velocity = 100;
};

struct Pattern
{
    int numerator = 4;
    int denominator = 4;
    int bars = 4;
    Style style = Style::Djent;
    Part part = Part::Verse;
    std::vector<Hit> hits;

    int stepsPerBar() const   { return numerator * 16 / denominator; }
    int totalSteps() const    { return stepsPerBar() * bars; }
    double beatsPerBar() const { return numerator * 4.0 / (double) denominator; }
    double totalBeats() const  { return beatsPerBar() * bars; }
    juce::String timeSigText() const { return juce::String (numerator) + "/" + juce::String (denominator); }
};

inline bool validTimeSig (int num, int den)
{
    return num >= 1 && num <= 32 && (den == 2 || den == 4 || den == 8 || den == 16);
}

juce::var patternToVar (const Pattern& p);
Pattern patternFromVar (const juce::var& v);
} // namespace drums
