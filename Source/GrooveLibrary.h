#pragma once

#include "MidiTools.h"

struct GrooveEntry
{
    int id = 0;
    juce::String name;
    drums::Pattern pattern;
    juce::int64 savedMs = 0;
    juce::File file;
};

// Библиотека GROOVES: каждый паттерн лежит в папке как .mid, а рядом файл grooves.json
// хранит стиль, часть, размер, дату и сам паттерн (чтобы двойной клик грузил его в EDIT).
class GrooveLibrary
{
public:
    void setFolder (const juce::File& folder);
    const juce::File& getFolder() const { return folder; }

    const std::vector<GrooveEntry>& getEntries() const { return entries; }
    const GrooveEntry* findById (int id) const;

    // Имя вида drum_%style%_%part%_XXXX
    bool add (const drums::Pattern& pattern, const drums::DrumMap& map, juce::String& nameOut);

private:
    void load();
    bool writeIndex() const;

    juce::File folder;
    std::vector<GrooveEntry> entries;
    int nextId = 1;
};
