#pragma once

#include "Components.h"

class DrUmsEditor : public juce::AudioProcessorEditor,
                    public juce::DragAndDropContainer,
                    private juce::ChangeListener,
                    private juce::Timer
{
public:
    explicit DrUmsEditor (DrUmsProcessor&);
    ~DrUmsEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // Строки GROOVES при выносе за пределы окна превращаются во внешний drag .mid-файла.
    bool shouldDropFilesWhenDraggedExternally (const juce::DragAndDropTarget::SourceDetails&,
                                               juce::StringArray& files, bool& canMoveFiles) override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refresh(); }
    void timerCallback() override;

    void refresh();
    void setGrooveMode (bool on);
    void showLibraryMenu();
    void showStyleMenu();
    void showSettingsMenu();
    void saveToGrooves();
    void say (const juce::String& text);

    DrUmsProcessor& proc;
    ui::ReferenceLookAndFeel laf;

    juce::Component content;   // рисуется в координатах референса 766x623 и масштабируется
    ui::KitView kit;
    ui::PreviewView preview;
    ui::GroovesView grooves;
    ui::BoxButton libraryBtn, styleBtn;
    ui::SegmentedBox modeBox, partBox;
    ui::SigBox sigBox;
    ui::IconButton diceAll { ui::Icon::Dice }, gearBtn { ui::Icon::Gear },
                   playBtn { ui::Icon::Play }, heartBtn { ui::Icon::Heart };
    std::unique_ptr<ui::IconButton> laneDice[4], laneLock[4];
    juce::Label status;

    std::array<juce::uint32, 4> lastHits {};
    bool grooveMode = false;
    bool wasPlayingUi = false;
    int shownGrooveId = -1;
    int statusTicks = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrUmsEditor)
};
