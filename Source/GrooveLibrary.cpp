#include "GrooveLibrary.h"

namespace
{
    const char* kIndexName = "grooves.json";
}

void GrooveLibrary::setFolder (const juce::File& newFolder)
{
    folder = newFolder;
    load();
}

const GrooveEntry* GrooveLibrary::findById (int id) const
{
    for (const auto& e : entries)
        if (e.id == id)
            return &e;
    return nullptr;
}

void GrooveLibrary::load()
{
    entries.clear();
    nextId = 1;

    const auto index = folder.getChildFile (kIndexName);
    if (! index.existsAsFile())
        return;

    const auto root = juce::JSON::parse (index.loadFileAsString());
    nextId = juce::jmax (1, (int) root.getProperty ("nextId", 1));

    const auto itemsVar = root.getProperty ("items", juce::var());
    const auto* items = itemsVar.getArray();
    if (items == nullptr)
        return;

    for (const auto& it : *items)
    {
        GrooveEntry e;
        e.id = (int) it.getProperty ("id", 0);
        e.name = it.getProperty ("name", juce::var()).toString();
        e.savedMs = (juce::int64) it.getProperty ("date", 0);
        e.pattern = drums::patternFromVar (it.getProperty ("pattern", juce::var()));
        e.file = folder.getChildFile (e.name + ".mid");

        // Если .mid удалили руками, запись из библиотеки пропадает.
        if (e.id > 0 && e.name.isNotEmpty() && e.file.existsAsFile())
        {
            nextId = juce::jmax (nextId, e.id + 1);
            entries.push_back (std::move (e));
        }
    }
}

bool GrooveLibrary::writeIndex() const
{
    juce::Array<juce::var> items;
    for (const auto& e : entries)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("id", e.id);
        o->setProperty ("name", e.name);
        o->setProperty ("date", e.savedMs);
        o->setProperty ("pattern", drums::patternToVar (e.pattern));
        items.add (juce::var (o));
    }

    auto* root = new juce::DynamicObject();
    root->setProperty ("nextId", nextId);
    root->setProperty ("items", items);

    return folder.getChildFile (kIndexName).replaceWithText (juce::JSON::toString (juce::var (root)));
}

bool GrooveLibrary::add (const drums::Pattern& pattern, const drums::DrumMap& map, juce::String& nameOut)
{
    if (! folder.createDirectory().wasOk())
        return false;

    GrooveEntry e;
    e.id = nextId;
    e.name = "drum_" + juce::String (drums::styleSlug (pattern.style)) + "_"
             + juce::String (drums::partSlug (pattern.part)) + "_" + juce::String (e.id).paddedLeft ('0', 4);
    e.file = folder.getChildFile (e.name + ".mid");
    e.pattern = pattern;
    e.savedMs = juce::Time::currentTimeMillis();

    if (! drums::writeMidiFile (e.file, pattern, map))
        return false;

    ++nextId;
    nameOut = e.name;
    entries.push_back (std::move (e));
    return writeIndex();
}
