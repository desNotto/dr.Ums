#include "PluginEditor.h"

using namespace drums;
using ui::ru;

namespace
{
    constexpr int kW = 766, kH = 623;      // размер референса

    // Порядок рядов слева сверху вниз: тарелки, тома, снейр, кик.
    const Lane rowLane[4] = { Lane::Cymbals, Lane::Toms, Lane::Snare, Lane::Kick };
}

DrUmsEditor::DrUmsEditor (DrUmsProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), preview (p), grooves (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (content);

    for (auto* c : std::initializer_list<juce::Component*> { &kit, &grooves, &libraryBtn, &styleBtn, &modeBox,
                                                              &partBox, &sigBox, &diceAll, &gearBtn, &playBtn,
                                                              &preview, &heartBtn, &status })
        content.addAndMakeVisible (c);

    for (int i = 0; i < 4; ++i)
    {
        laneDice[i] = std::make_unique<ui::IconButton> (ui::Icon::Dice);
        laneLock[i] = std::make_unique<ui::IconButton> (ui::Icon::Lock);
        laneLock[i]->setClickingTogglesState (true);
        content.addAndMakeVisible (*laneDice[i]);
        content.addAndMakeVisible (*laneLock[i]);

        const Lane lane = rowLane[i];
        laneDice[i]->onClick = [this, lane] { proc.randomizeLane (lane); };
        laneLock[i]->onClick = [this, i, lane] { proc.setLocked (lane, laneLock[i]->getToggleState()); };

        laneDice[i]->setBounds (3, 143 + i * 98, 68, 70);
        laneLock[i]->setBounds (75, 143 + i * 98, 76, 70);
    }

    libraryBtn.setBounds (10, 15, 150, 34);
    diceAll.setBounds (42, 58, 76, 76);
    styleBtn.setBounds (160, 80, 110, 36);
    partBox.setBounds (285, 77, 360, 40);
    modeBox.setBounds (326, 16, 150, 38);
    playBtn.setBounds (492, 13, 42, 42);
    gearBtn.setBounds (696, 12, 66, 66);
    sigBox.setBounds (706, 84, 52, 34);
    kit.setBounds (157, 131, 609, 394);
    grooves.setBounds (0, 131, kW, 394);
    preview.setBounds (0, 543, kW, 60);
    heartBtn.setBounds (716, 538, 50, 50);
    status.setBounds (164, 498, 380, 22);

    status.setFont (ui::plainFont (13.0f));
    status.setColour (juce::Label::textColourId, juce::Colours::black);
    status.setJustificationType (juce::Justification::centredLeft);
    status.setInterceptsMouseClicks (false, false);

    modeBox.setItems ({ "EDIT", "GROOVES" });
    juce::StringArray parts;
    for (int i = 0; i < kNumParts; ++i)
        parts.add (juce::String (partName ((Part) i)).toLowerCase());
    partBox.setItems (parts);

    libraryBtn.onClick = [this] { showLibraryMenu(); };
    styleBtn.onClick   = [this] { showStyleMenu(); };
    gearBtn.onClick    = [this] { showSettingsMenu(); };
    diceAll.onClick    = [this] { proc.randomizeAll(); };
    playBtn.onClick    = [this] { proc.setPlaying (! proc.isPlaying()); };
    heartBtn.onClick   = [this] { saveToGrooves(); };

    modeBox.onChange = [this] (int i) { setGrooveMode (i == 1); };
    partBox.onChange = [this] (int i) { proc.setPart ((Part) i); };
    sigBox.onApply   = [this] (int n, int d) { proc.setTimeSig (n, d); };

    grooves.onSelect = [this] (const GrooveEntry& e)
    {
        shownGrooveId = e.id;
        proc.setAudition (&e.pattern);
    };
    grooves.onLoad = [this] (const GrooveEntry& e)
    {
        shownGrooveId = -1;
        proc.loadGroove (e);
        modeBox.setSelected (0);
        setGrooveMode (false);
    };

    preview.getDragFile = [this]() -> juce::File
    {
        if (grooveMode && shownGrooveId >= 0)
            if (const auto* e = proc.getLibrary().findById (shownGrooveId))
                return e->file;
        return proc.exportTempMidi (proc.getShownPattern());
    };

    grooves.setVisible (false);

    setResizable (true, true);
    setResizeLimits (383, 312, kW * 2, kH * 2);
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    setSize (kW, kH);

    proc.addChangeListener (this);
    startTimerHz (30);
    refresh();
}

DrUmsEditor::~DrUmsEditor()
{
    stopTimer();
    proc.setAudition (nullptr);
    proc.removeChangeListener (this);
    setLookAndFeel (nullptr);
}

void DrUmsEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::white);
}

void DrUmsEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kW, kH);
}

void DrUmsEditor::refresh()
{
    libraryBtn.setLabel (proc.currentMap().name);
    styleBtn.setLabel (juce::String (styleName (proc.getStyle())));
    partBox.setSelected ((int) proc.getPart());
    sigBox.setValue (proc.getNumerator(), proc.getDenominator());

    for (int i = 0; i < 4; ++i)
        laneLock[i]->setToggleState (proc.isLocked (rowLane[i]), juce::dontSendNotification);

    playBtn.setToggleState (proc.isPlaying(), juce::dontSendNotification);
    preview.repaint();
    grooves.refresh();
}

void DrUmsEditor::setGrooveMode (bool on)
{
    grooveMode = on;
    if (! on)
    {
        shownGrooveId = -1;
        proc.setAudition (nullptr);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &kit, &diceAll, &styleBtn, &partBox,
                                                              &sigBox, &heartBtn, &status })
        c->setVisible (! on);

    for (int i = 0; i < 4; ++i)
    {
        laneDice[i]->setVisible (! on);
        laneLock[i]->setVisible (! on);
    }

    grooves.setVisible (on);
    refresh();
}

void DrUmsEditor::timerCallback()
{
    const bool on = proc.isPlaying();
    if (playBtn.getToggleState() != on)
        playBtn.setToggleState (on, juce::dontSendNotification);

    for (size_t l = 0; l < 4; ++l)
    {
        const auto c = proc.laneHits[l].load();
        if (c != lastHits[l])
        {
            lastHits[l] = c;
            kit.flash ((int) l);
        }
    }
    kit.decay();

    if (on || wasPlayingUi)
        preview.repaint();
    wasPlayingUi = on;

    if (statusTicks > 0 && --statusTicks == 0)
        status.setText ({}, juce::dontSendNotification);
}

void DrUmsEditor::say (const juce::String& text)
{
    status.setText (text, juce::dontSendNotification);
    statusTicks = 90;
}

void DrUmsEditor::saveToGrooves()
{
    juce::String name;
    if (proc.saveGroove (name))
        say (ru ("Сохранено в GROOVES: ") + name);
    else
        say (ru ("Не удалось сохранить в ") + proc.getMidiFolder().getFullPathName());
}

void DrUmsEditor::showLibraryMenu()
{
    juce::PopupMenu m;
    const auto& maps = builtinMaps();
    for (size_t i = 0; i < maps.size(); ++i)
        m.addItem ((int) i + 1, maps[i].name, true, (int) i == proc.getMapIndex());

    juce::Component::SafePointer<DrUmsEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&libraryBtn),
                     [safe] (int r)
                     {
                         if (safe != nullptr && r > 0)
                             safe->proc.setMapIndex (r - 1);
                     });
}

void DrUmsEditor::showStyleMenu()
{
    juce::PopupMenu m;
    for (int i = 0; i < kNumStyles; ++i)
        m.addItem (i + 1, juce::String (styleName ((Style) i)), true, (Style) i == proc.getStyle());

    juce::Component::SafePointer<DrUmsEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&styleBtn),
                     [safe] (int r)
                     {
                         if (safe != nullptr && r > 0)
                             safe->proc.setStyle ((Style) (r - 1));
                     });
}

void DrUmsEditor::showSettingsMenu()
{
    juce::PopupMenu m;
    m.addItem (100, ru ("Папка для MIDI-файлов:"), false);
    m.addItem (101, proc.getMidiFolder().getFullPathName(), false);
    m.addSeparator();
    m.addItem (1, ru ("Выбрать папку…"));
    m.addItem (2, ru ("Открыть папку"));

    juce::Component::SafePointer<DrUmsEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gearBtn),
                     [safe] (int r)
                     {
                         if (safe == nullptr)
                             return;

                         if (r == 2)
                         {
                             safe->proc.getMidiFolder().revealToUser();
                         }
                         else if (r == 1)
                         {
                             safe->chooser = std::make_unique<juce::FileChooser> (ru ("Папка для MIDI-файлов"),
                                                                                   safe->proc.getMidiFolder());
                             safe->chooser->launchAsync (juce::FileBrowserComponent::openMode
                                                             | juce::FileBrowserComponent::canSelectDirectories,
                                                         [safe] (const juce::FileChooser& fc)
                                                         {
                                                             const auto f = fc.getResult();
                                                             if (safe != nullptr && f.isDirectory())
                                                                 safe->proc.setMidiFolder (f);
                                                         });
                         }
                     });
}

bool DrUmsEditor::shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails& d,
                                                         juce::StringArray& files, bool& canMoveFiles)
{
    const auto desc = d.description.toString();
    if (desc.startsWith ("groove:"))
    {
        const int id = desc.fromFirstOccurrenceOf (":", false, false).getIntValue();
        if (const auto* e = proc.getLibrary().findById (id))
        {
            files.add (e->file.getFullPathName());
            canMoveFiles = false;
            return true;
        }
    }
    return false;
}
