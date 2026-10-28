#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <optional>
#include "DrumMaps.h"
#include "Generator.h"
#include "GrooveLibrary.h"
#include "MidiTools.h"

class DrUmsProcessor : public juce::AudioProcessor,
                       public juce::ChangeBroadcaster
{
public:
    DrUmsProcessor();
    ~DrUmsProcessor() override = default;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "dr. Ums"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- состояние (вызывать из потока интерфейса)
    const drums::Pattern& getPattern() const { return pattern; }
    const drums::Pattern& getShownPattern() const { return audition ? *audition : pattern; }

    drums::Style getStyle() const { return style; }
    drums::Part getPart() const { return part; }
    int getNumerator() const { return num; }
    int getDenominator() const { return den; }

    int getMapIndex() const { return mapIndex; }
    const drums::DrumMap& currentMap() const { return drums::builtinMaps()[(size_t) mapIndex]; }

    bool isLocked (drums::Lane l) const { return locked[(size_t) l]; }
    void setLocked (drums::Lane l, bool b) { locked[(size_t) l] = b; sendChangeMessage(); }

    void randomizeAll();
    void randomizeLane (drums::Lane lane);
    void setStyle (drums::Style s);
    void setPart (drums::Part p);
    bool setTimeSig (int n, int d);
    void setMapIndex (int i);

    // Паттерн из GROOVES, который сейчас слушаем/смотрим, не трогая основной (nullptr — выключить).
    void setAudition (const drums::Pattern* p);
    void loadGroove (const GrooveEntry& e);

    bool saveGroove (juce::String& nameOut);
    GrooveLibrary& getLibrary() { return library; }
    juce::File getMidiFolder() const { return library.getFolder(); }
    void setMidiFolder (const juce::File& f);

    // Временный .mid для drag-and-drop из окна превью.
    juce::File exportTempMidi (const drums::Pattern& p);

    // ---- воспроизведение (Play отправляет MIDI во внешнюю библиотеку)
    void setPlaying (bool b) { playing.store (b); }
    bool isPlaying() const { return playing.load(); }
    double getPlayBeat() const { return playing.load() ? playBeatAtomic.load() : -1.0; }
    std::array<std::atomic<juce::uint32>, (size_t) drums::kNumLanes> laneHits;

private:
    void changed();
    void rebuildPlayEvents();
    void loadSettings();
    void saveSettings() const;
    static juce::File defaultMidiFolder();
    static juce::PropertiesFile::Options settingsOptions();

    juce::Random rng;
    drums::Style style = drums::Style::Djent;
    drums::Part part = drums::Part::Verse;
    int num = 4, den = 4;
    int mapIndex = 0;
    std::array<bool, (size_t) drums::kNumLanes> locked {};
    drums::Pattern pattern;
    std::optional<drums::Pattern> audition;
    GrooveLibrary library;

    // аудио-поток
    struct PendingOff { int note; int remaining; };
    juce::SpinLock eventLock;
    std::vector<drums::PlayEvent> events;
    double loopBeats = 0.0;
    std::vector<PendingOff> pendingOffs;
    std::atomic<bool> playing { false };
    std::atomic<double> playBeatAtomic { 0.0 };
    double playBeatPos = 0.0;
    bool wasPlaying = false;
    double currentSampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrUmsProcessor)
};
