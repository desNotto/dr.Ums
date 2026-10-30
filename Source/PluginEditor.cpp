#include "PluginEditor.h"

using namespace drums;
using ui::ru;

namespace
{
    constexpr int kW = 766, kH = 623;      // средний размер (100%) и пропорции окна
    constexpr int kLargeW = 1072;          // большой размер (140%)
    constexpr int kMinW = 383, kMaxW = kW * 2;

    // Порядок рядов слева сверху вниз: тарелки, тома, снейр, кик.
    const Lane rowLane[4] = { Lane::Cymbals, Lane::Toms, Lane::Snare, Lane::Kick };
}

DrUmsEditor::DrUmsEditor (DrUmsProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p), preview (p), grooves (p)
{
    setLookAndFeel (&laf);
    addAndMakeVisible (content);

    for (auto* c : std::initializer_list<juce::Component*> { &backdrop, &kit, &grooves, &libraryBtn, &styleBtn,
                                                              &modeBox, &partBox, &scaleBox, &sigBox, &diceAll, &gearBtn,
                                                              &playBtn, &preview, &heartBtn, &status })
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

        laneLock[i]->setTint (ui::col::lane (lane));
        laneDice[i]->setBounds (16, 164 + i * 92, 60, 60);
        laneLock[i]->setBounds (88, 164 + i * 92, 60, 60);
    }

    backdrop.setBounds (0, 0, kW, kH);
    libraryBtn.setBounds (14, 10, 182, 46);
    styleBtn.setBounds (204, 10, 140, 46);
    sigBox.setBounds (352, 10, 68, 46);
    modeBox.setBounds (430, 14, 142, 38);
    scaleBox.setBounds (582, 16, 58, 34);
    playBtn.setBounds (648, 8, 50, 50);
    gearBtn.setBounds (706, 10, 44, 44);
    diceAll.setBounds (22, 70, 66, 66);
    partBox.setBounds (176, 82, 574, 44);
    kit.setBounds (170, 158, 582, 360);
    grooves.setBounds (14, 158, 738, 360);
    preview.setBounds (14, 540, 676, 60);
    heartBtn.setBounds (698, 543, 54, 54);
    status.setBounds (188, 488, 380, 22);

    status.setFont (ui::boldFont (12.0f));
    status.setColour (juce::Label::textColourId, ui::col::text.withAlpha (0.85f));
    status.setJustificationType (juce::Justification::centredLeft);
    status.setInterceptsMouseClicks (false, false);

    modeBox.setItems ({ "EDIT", "GROOVES" });
    modeBox.fontSize = 13.0f;
    scaleBox.setItems ({ "M", "L" });
    scaleBox.fontSize = 13.0f;
    partBox.fontSize = 14.0f;

    libraryBtn.onClick = [this] { showLibraryMenu(); };
    styleBtn.onClick   = [this] { showStyleMenu(); };
    gearBtn.onClick    = [this] { showSettingsMenu(); };
    diceAll.onClick    = [this] { proc.randomizeAll(); };
    playBtn.onClick    = [this] { proc.setPlaying (! proc.isPlaying()); };
    heartBtn.onClick   = [this] { saveToGrooves(); };

    modeBox.onChange = [this] (int i) { setGrooveMode (i == 1); };
    scaleBox.onChange = [this] (int i) { setScale (i); };
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
    setResizeLimits (kMinW, (int) std::lround (kMinW * (double) kH / kW), kMaxW, (int) std::lround (kMaxW * (double) kH / kW));
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);

    // размер окна запоминается между открытиями плагина
    const int savedW = proc.getSavedEditorWidth();
    const int startW = savedW >= kMinW ? juce::jmin (savedW, kMaxW) : kW;
    setSize (startW, (int) std::lround (startW * (double) kH / kW));
    sizeReady = true;

    shownLang = loc::get();
    applyLanguage();
    updateScaleBox();

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
    g.fillAll (ui::col::bg0);
}

void DrUmsEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content.setTransform (juce::AffineTransform::scale (s));
    content.setBounds (0, 0, kW, kH);

    if (sizeReady)
    {
        sizeSaveTicks = 15;   // сохраняем через полсекунды после последнего изменения размера
        updateScaleBox();
    }
}

void DrUmsEditor::updateScaleBox()
{
    const int w = getWidth();
    scaleBox.setSelected (std::abs (w - kW) <= 8 ? 0 : (std::abs (w - kLargeW) <= 8 ? 1 : -1));
}

void DrUmsEditor::setScale (int index)
{
    const int w = index == 1 ? kLargeW : kW;
    setSize (w, (int) std::lround (w * (double) kH / kW));
}

void DrUmsEditor::applyLanguage()
{
    libraryBtn.setCaption (ui::tr ("БИБЛИОТЕКА", "LIBRARY"));
    styleBtn.setCaption (ui::tr ("СТИЛЬ", "STYLE"));
    sigBox.setCaption (ui::tr ("РАЗМЕР", "TIME SIG."));

    juce::StringArray parts;
    for (int i = 0; i < kNumParts; ++i)
        parts.add (ui::partLabel ((Part) i));
    partBox.setItems (parts);

    grooves.refreshLanguage();
    backdrop.repaint();
    status.setText ({}, juce::dontSendNotification);
}

void DrUmsEditor::refresh()
{
    if (shownLang != loc::get())
    {
        shownLang = loc::get();
        applyLanguage();
    }

    libraryBtn.setValue (proc.currentMap().name);
    styleBtn.setValue (juce::String (styleName (proc.getStyle())));
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

    if (on)
        grooves.setBounds (14, 76, 738, 442);   // вкладка занимает и место скрытых органов управления
    grooves.setVisible (on);
    backdrop.setEditLayout (! on);
    refresh();
}

void DrUmsEditor::timerCallback()
{
    const bool on = proc.isPlaying();
    if (playBtn.getToggleState() != on)
        playBtn.setToggleState (on, juce::dontSendNotification);

    for (size_t a = 0; a < lastHits.size(); ++a)
    {
        const auto c = proc.artHits[a].load();
        if (c != lastHits[a])
        {
            lastHits[a] = c;
            kit.flash ((int) a);
        }
    }
    kit.decay();

    if (on || wasPlayingUi)
        preview.repaint();
    wasPlayingUi = on;

    if (statusTicks > 0 && --statusTicks == 0)
        status.setText ({}, juce::dontSendNotification);

    if (sizeSaveTicks > 0 && --sizeSaveTicks == 0)
        proc.saveEditorWidth (getWidth());
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
        say (ui::tr ("Сохранено в GROOVES: ", "Saved to GROOVES: ") + name);
    else
        say (ui::tr ("Не удалось сохранить в ", "Could not save to ") + proc.getMidiFolder().getFullPathName());
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
    m.addItem (100, ui::tr ("Папка для MIDI-файлов:", "MIDI files folder:"), false);
    m.addItem (101, proc.getMidiFolder().getFullPathName(), false);
    m.addSeparator();
    m.addItem (1, ui::tr ("Выбрать папку…", "Choose folder…"));
    m.addItem (2, ui::tr ("Открыть папку", "Open folder"));
    m.addSeparator();
    m.addItem (200, ui::tr ("Язык интерфейса:", "Interface language:"), false);
    m.addItem (10, juce::String::fromUTF8 ("Русский"), true, loc::isRu());
    m.addItem (11, "English", true, ! loc::isRu());

    juce::Component::SafePointer<DrUmsEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gearBtn),
                     [safe] (int r)
                     {
                         if (safe == nullptr)
                             return;

                         if (r == 10)
                         {
                             safe->proc.setLanguage (loc::Lang::Ru);
                         }
                         else if (r == 11)
                         {
                             safe->proc.setLanguage (loc::Lang::En);
                         }
                         else if (r == 2)
                         {
                             safe->proc.getMidiFolder().revealToUser();
                         }
                         else if (r == 1)
                         {
                             safe->chooser = std::make_unique<juce::FileChooser> (ui::tr ("Папка для MIDI-файлов", "MIDI files folder"),
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
