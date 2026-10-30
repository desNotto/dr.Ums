#pragma once

#include "Components.h"

// Фон окна: градиент, панели рядов Random/Lock с подписями дорожек.
class Backdrop : public juce::Component
{
public:
    Backdrop() { setInterceptsMouseClicks (false, false); }

    void setEditLayout (bool on)
    {
        if (editLayout != on)
        {
            editLayout = on;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setGradientFill (juce::ColourGradient::vertical (ui::col::bg1, b.getY(), ui::col::bg0, b.getBottom()));
        g.fillRect (b);

        if (! editLayout)
            return;

        static const drums::Lane lanes[4] = { drums::Lane::Cymbals, drums::Lane::Toms, drums::Lane::Snare, drums::Lane::Kick };
        const juce::String names[4] = { ui::tr ("ТАРЕЛКИ", "CYMBALS"), ui::tr ("ТОМЫ", "TOMS"), ui::tr ("СНЕЙР", "SNARE"), ui::tr ("КИК", "KICK") };

        for (int i = 0; i < 4; ++i)
        {
            const juce::Rectangle<float> row (8.0f, 158.0f + (float) i * 92.0f, 150.0f, 84.0f);
            g.setColour (ui::col::panel);
            g.fillRoundedRectangle (row, 14.0f);
            g.setColour (ui::col::line);
            g.drawRoundedRectangle (row.reduced (0.5f), 14.0f, 1.0f);

            const auto c = ui::col::lane (lanes[i]);
            g.setColour (c);
            g.fillEllipse (row.getX() + 14.0f, row.getBottom() - 15.5f, 7.0f, 7.0f);
            g.setColour (ui::col::dim);
            g.setFont (ui::boldFont (9.5f, 0.1f));
            g.drawText (names[i], juce::Rectangle<float> (row.getX() + 28.0f, row.getBottom() - 19.0f, 100.0f, 14.0f),
                        juce::Justification::centredLeft, false);
        }

        g.setColour (ui::col::dim);
        g.setFont (ui::boldFont (9.5f, 0.1f));
        g.drawText (ui::tr ("СЛУЧАЙНЫЙ", "RANDOM"), juce::Rectangle<float> (96.0f, 92.0f, 90.0f, 12.0f), juce::Justification::centredLeft, false);
        g.drawText (ui::tr ("ПАТТЕРН", "PATTERN"), juce::Rectangle<float> (96.0f, 105.0f, 90.0f, 12.0f), juce::Justification::centredLeft, false);
    }

private:
    bool editLayout = true;
};

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
    void applyLanguage();
    void setScale (int index);      // 0 средний, 1 большой
    void updateScaleBox();

    DrUmsProcessor& proc;
    ui::ReferenceLookAndFeel laf;

    juce::Component content;   // рисуется в логических координатах 766x623 и масштабируется
    Backdrop backdrop;
    ui::KitView kit;
    ui::PreviewView preview;
    ui::GroovesView grooves;
    ui::DropButton libraryBtn { juce::String() }, styleBtn { juce::String() };
    ui::SegmentedBox modeBox, partBox, scaleBox;
    ui::SigBox sigBox { juce::String() };
    ui::IconButton diceAll { ui::Icon::Dice }, gearBtn { ui::Icon::Gear },
                   playBtn { ui::Icon::Play }, heartBtn { ui::Icon::Heart };
    std::unique_ptr<ui::IconButton> laneDice[4], laneLock[4];
    juce::Label status;

    std::array<juce::uint32, (size_t) drums::kNumArts> lastHits {};
    bool grooveMode = false;
    bool wasPlayingUi = false;
    int shownGrooveId = -1;
    int statusTicks = 0;
    int sizeSaveTicks = 0;
    bool sizeReady = false;
    loc::Lang shownLang = loc::Lang::Ru;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrUmsEditor)
};
