#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include "PluginProcessor.h"

// Look: dark panel, amber value arcs. Draws the knob/toggle names and group headings itself.
class AudreyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AudreyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    // LookAndFeel_V4 only applies the slider text-box outline colour for non-midnight schemes, so a light
    // outline showed under the default scheme. Set the label colours here instead.
    juce::Label* createSliderTextBox (juce::Slider&) override;

    static juce::Colour background()  { return juce::Colour (0xff15171b); }
    static juce::Colour panel()       { return juce::Colour (0xff1e2127); }
    static juce::Colour accent()      { return juce::Colour (0xffe8b04a); }
    static juce::Colour text()        { return juce::Colour (0xffdfe3ea); }
    static juce::Colour dim()         { return juce::Colour (0xff7d8594); }
};

class AudreyEditor : public juce::AudioProcessorEditor
{
public:
    explicit AudreyEditor (AudreyProcessor&);
    ~AudreyEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Cell
    {
        int paramId = -1;
        juce::Rectangle<int> bounds;                  // where the cell sits (set in resized)
        std::unique_ptr<juce::Slider> slider;         // knobs
        std::unique_ptr<juce::ToggleButton> toggle;   // on/off parameters
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };

    struct Group
    {
        const char* title;
        std::array<int, 4> ids;   // parameter ids, -1 = unused
        int row;
        juce::Rectangle<int> bounds;
    };

    AudreyLookAndFeel laf;
    std::array<Cell, audrey::kNumParams> cells;
    std::array<Group, 7> groups;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudreyEditor)
};
