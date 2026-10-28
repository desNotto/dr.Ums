#include "MidiTools.h"

namespace drums
{
bool writeMidiFile (const juce::File& file, const Pattern& pattern, const DrumMap& map, double bpm)
{
    constexpr int stepTicks = kPPQ / 4;

    juce::MidiMessageSequence track;
    track.addEvent (juce::MidiMessage::textMetaEvent (3, "dr. Ums " + juce::String (styleName (pattern.style))
                                                         + " " + juce::String (partName (pattern.part))), 0.0);
    track.addEvent (juce::MidiMessage::tempoMetaEvent ((int) (60000000.0 / bpm)), 0.0);
    track.addEvent (juce::MidiMessage::timeSignatureMetaEvent (pattern.numerator, pattern.denominator), 0.0);

    for (const auto& h : pattern.hits)
    {
        const int note = map.noteFor (h.art);
        if (note < 0)
            continue;

        const double on = (double) (h.step * stepTicks);
        track.addEvent (juce::MidiMessage::noteOn (kMidiChannel, note, (juce::uint8) h.velocity), on);
        track.addEvent (juce::MidiMessage::noteOff (kMidiChannel, note), on + kHitTicks);
    }

    const double endTick = (double) (pattern.totalSteps() * stepTicks);
    track.addEvent (juce::MidiMessage::endOfTrack(), endTick);
    track.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (kPPQ);
    mf.addTrack (track);

    file.getParentDirectory().createDirectory();
    file.deleteFile();

    std::unique_ptr<juce::FileOutputStream> out (file.createOutputStream());
    if (out == nullptr)
        return false;

    const bool ok = mf.writeTo (*out);
    out->flush();
    return ok;
}

std::vector<PlayEvent> buildPlayEvents (const Pattern& pattern, const DrumMap& map)
{
    std::vector<PlayEvent> ev;
    ev.reserve (pattern.hits.size());

    for (const auto& h : pattern.hits)
    {
        const int note = map.noteFor (h.art);
        if (note < 0)
            continue;
        ev.push_back ({ h.step * 0.25, note, h.velocity, (int) laneOf (h.art) });
    }

    std::sort (ev.begin(), ev.end(), [] (const PlayEvent& a, const PlayEvent& b) { return a.beat < b.beat; });
    return ev;
}
} // namespace drums
