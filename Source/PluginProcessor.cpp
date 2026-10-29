#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace drums;

DrUmsProcessor::DrUmsProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      rng ((juce::int64) juce::Time::currentTimeMillis())
{
    for (auto& a : artHits)
        a.store (0);

    pendingOffs.reserve (512);

    loadSettings();
    pattern = generate (style, part, num, den, rng);
    rebuildPlayEvents();
}

// ------------------------------------------------------------------ настройки
juce::PropertiesFile::Options DrUmsProcessor::settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "dr. Ums";
    o.folderName = "desNotto";
    o.filenameSuffix = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.commonToAllUsers = false;
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    return o;
}

juce::File DrUmsProcessor::defaultMidiFolder()
{
    // По ТЗ папка по умолчанию — рядом с плагином. Если туда нельзя писать
    // (например, C:\Program Files\Common Files\VST3), берём Документы.
    const auto module = juce::File::getSpecialLocation (juce::File::currentExecutableFile);

    juce::File bundle = module;
    for (auto d = module; d.getFullPathName().isNotEmpty(); d = d.getParentDirectory())
    {
        if (d.hasFileExtension ("vst3"))
            bundle = d;
        if (d == d.getParentDirectory())
            break;
    }

    const auto beside = bundle.getParentDirectory().getChildFile ("dr. Ums MIDI");
    if (beside.createDirectory().wasOk() && beside.hasWriteAccess())
        return beside;

    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("dr. Ums");
    docs.createDirectory();
    return docs;
}

void DrUmsProcessor::loadSettings()
{
    juce::PropertiesFile props (settingsOptions());
    juce::File folder (props.getValue ("midiFolder"));

    if (folder.getFullPathName().isEmpty() || ! folder.isDirectory())
        folder = defaultMidiFolder();

    library.setFolder (folder);
}

void DrUmsProcessor::saveSettings() const
{
    juce::PropertiesFile props (settingsOptions());
    props.setValue ("midiFolder", library.getFolder().getFullPathName());
    props.saveIfNeeded();
}

void DrUmsProcessor::setMidiFolder (const juce::File& f)
{
    if (! f.createDirectory().wasOk())
        return;

    library.setFolder (f);
    saveSettings();
    sendChangeMessage();
}

// ------------------------------------------------------------------ генерация
void DrUmsProcessor::changed()
{
    rebuildPlayEvents();
    sendChangeMessage();
}

void DrUmsProcessor::rebuildPlayEvents()
{
    const auto& p = getShownPattern();
    auto ev = buildPlayEvents (p, currentMap());
    const double loop = p.totalBeats();

    const juce::SpinLock::ScopedLockType sl (eventLock);
    events = std::move (ev);
    loopBeats = loop;
}

void DrUmsProcessor::randomizeAll()
{
    audition.reset();
    const auto fresh = generate (style, part, num, den, rng);
    pattern = mergeLocked (fresh, pattern, locked);
    changed();
}

void DrUmsProcessor::randomizeLane (Lane lane)
{
    if (locked[(size_t) lane])
        return;

    audition.reset();
    pattern = regenerateLane (pattern, lane, rng);
    changed();
}

void DrUmsProcessor::setStyle (Style s)
{
    style = s;
    randomizeAll();
}

void DrUmsProcessor::setPart (Part p)
{
    part = p;
    randomizeAll();
}

bool DrUmsProcessor::setTimeSig (int n, int d)
{
    if (! validTimeSig (n, d))
        return false;

    num = n;
    den = d;
    randomizeAll();
    return true;
}

void DrUmsProcessor::setMapIndex (int i)
{
    mapIndex = juce::jlimit (0, (int) builtinMaps().size() - 1, i);
    changed();
}

void DrUmsProcessor::setAudition (const Pattern* p)
{
    if (p != nullptr)
        audition = *p;
    else
        audition.reset();
    changed();
}

void DrUmsProcessor::loadGroove (const GrooveEntry& e)
{
    audition.reset();
    pattern = e.pattern;
    style = pattern.style;
    part = pattern.part;
    num = pattern.numerator;
    den = pattern.denominator;
    changed();
}

bool DrUmsProcessor::saveGroove (juce::String& nameOut)
{
    const bool ok = library.add (pattern, currentMap(), nameOut);
    sendChangeMessage();
    return ok;
}

juce::File DrUmsProcessor::exportTempMidi (const Pattern& p)
{
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("dr. Ums");
    const auto file = dir.getChildFile ("drum_" + juce::String (styleSlug (p.style)) + "_"
                                        + juce::String (partSlug (p.part)) + "_draft.mid");
    writeMidiFile (file, p, currentMap());
    return file;
}

// ------------------------------------------------------------------ состояние проекта
void DrUmsProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("map", mapIndex);
    o->setProperty ("style", (int) style);
    o->setProperty ("part", (int) part);
    o->setProperty ("num", num);
    o->setProperty ("den", den);

    juce::Array<juce::var> locks;
    for (bool b : locked)
        locks.add (b);
    o->setProperty ("locks", locks);
    o->setProperty ("pattern", patternToVar (pattern));

    const auto text = juce::JSON::toString (juce::var (o));
    destData.append (text.toRawUTF8(), text.getNumBytesAsUTF8());
}

void DrUmsProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto v = juce::JSON::parse (juce::String::fromUTF8 ((const char*) data, sizeInBytes));
    if (! v.isObject())
        return;

    mapIndex = juce::jlimit (0, (int) builtinMaps().size() - 1, (int) v.getProperty ("map", 0));
    style = (Style) juce::jlimit (0, kNumStyles - 1, (int) v.getProperty ("style", 0));
    part  = (Part)  juce::jlimit (0, kNumParts - 1,  (int) v.getProperty ("part", 1));

    const int n = (int) v.getProperty ("num", 4);
    const int d = (int) v.getProperty ("den", 4);
    if (validTimeSig (n, d))
    {
        num = n;
        den = d;
    }

    const auto locksVar = v.getProperty ("locks", juce::var());
    if (const auto* arr = locksVar.getArray())
        for (int i = 0; i < juce::jmin ((int) locked.size(), arr->size()); ++i)
            locked[(size_t) i] = (bool) (*arr)[i];

    const auto saved = patternFromVar (v.getProperty ("pattern", juce::var()));
    if (! saved.hits.empty())
        pattern = saved;

    audition.reset();
    changed();
}

// ------------------------------------------------------------------ аудио-поток
void DrUmsProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
    pendingOffs.clear();
    playBeatPos = 0.0;
}

bool DrUmsProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void DrUmsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    midi.clear();

    const int n = buffer.getNumSamples();

    if (! playing.load())
    {
        if (wasPlaying || ! pendingOffs.empty())
        {
            for (const auto& p : pendingOffs)
                midi.addEvent (juce::MidiMessage::noteOff (kMidiChannel, p.note), 0);
            pendingOffs.clear();
        }
        wasPlaying = false;
        playBeatPos = 0.0;
        playBeatAtomic.store (0.0);
        return;
    }

    wasPlaying = true;

    // Темп берём у хоста; если хост его не отдаёт, играем на 120.
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = *b;
    bpm = juce::jlimit (20.0, 400.0, bpm);

    const double sr = currentSampleRate > 0.0 ? currentSampleRate : 44100.0;
    const double beatsPerSample = bpm / 60.0 / sr;
    const int noteLen = juce::jmax (1, (int) (kHitBeats / beatsPerSample));

    {
        juce::SpinLock::ScopedTryLockType lock (eventLock);
        if (lock.isLocked() && loopBeats > 0.0)
        {
            playBeatPos = std::fmod (playBeatPos, loopBeats);
            const double start = playBeatPos;
            double end = start + n * beatsPerSample;

            auto emit = [&] (double from, double to, int baseOffset)
            {
                for (const auto& e : events)
                {
                    if (e.beat < from) continue;
                    if (e.beat >= to) break;

                    const int off = juce::jlimit (0, n - 1, baseOffset + (int) ((e.beat - from) / beatsPerSample));
                    midi.addEvent (juce::MidiMessage::noteOn (kMidiChannel, e.note, (juce::uint8) e.velocity), off);
                    pendingOffs.push_back ({ e.note, off + noteLen });
                    artHits[(size_t) e.art].fetch_add (1);
                }
            };

            if (end < loopBeats)
            {
                emit (start, end, 0);
                playBeatPos = end;
            }
            else
            {
                emit (start, loopBeats, 0);
                const int split = (int) ((loopBeats - start) / beatsPerSample);
                end -= loopBeats;
                emit (0.0, end, split);
                playBeatPos = end;
            }
        }
    }

    for (auto it = pendingOffs.begin(); it != pendingOffs.end();)
    {
        if (it->remaining < n)
        {
            midi.addEvent (juce::MidiMessage::noteOff (kMidiChannel, it->note), it->remaining);
            it = pendingOffs.erase (it);
        }
        else
        {
            it->remaining -= n;
            ++it;
        }
    }

    playBeatAtomic.store (playBeatPos);
}

juce::AudioProcessorEditor* DrUmsProcessor::createEditor()
{
    return new DrUmsEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrUmsProcessor();
}
