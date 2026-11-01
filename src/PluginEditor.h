#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>
#include <vector>
#include "PluginProcessor.h"

// Look: black panel, white knobs with thin value rings, hairline signal-flow lines and concentric ring graphics
// (drawn procedurally, our own artwork). Control placement follows the instrument's signal flow, in the manner of
// the Audrey II panel. No logos or artwork from the original panel are used.
class AudreyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AudreyLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    // LookAndFeel_V4 only applies the slider text-box outline colour for non-midnight schemes, so a light
    // outline showed under the default scheme. Set the label colours and font here instead.
    juce::Label* createSliderTextBox (juce::Slider&) override;
    // value boxes: text only, no outline or fill (the default draws a light box around the text)
    void drawLabel (juce::Graphics&, juce::Label&) override;

    static juce::Colour background() { return juce::Colour (0xff09090b); }
    static juce::Colour line()       { return juce::Colour (0xffd9d5cb); }
    static juce::Colour knob()       { return juce::Colour (0xfff3f0e8); }
    static juce::Colour text()       { return juce::Colour (0xffe6e2d8); }
    static juce::Colour dim()        { return juce::Colour (0xff8c8980); }
    static juce::Colour accent()     { return juce::Colour (0xffe8b04a); }
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
        float x = 0, y = 0, dia = 0;                  // centre and knob diameter on the 1200 x 840 reference canvas
        juce::Rectangle<float> labelBounds;           // where the name is drawn (set in resized)
        std::unique_ptr<juce::Slider> slider;         // knobs
        std::unique_ptr<juce::ToggleButton> toggle;   // on/off parameters
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
    };

    AudreyLookAndFeel laf;
    std::array<Cell, audrey::kNumParams> cells;
    juce::Path ringArt;     // concentric ring graphics
    juce::Path flowLines;   // signal-flow hairlines
    juce::Path arrowHeads;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudreyEditor)
};
