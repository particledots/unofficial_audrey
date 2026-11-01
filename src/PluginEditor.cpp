#include "PluginEditor.h"

using namespace audrey;

// ---------------------------------------------------------------------------------------------
// Look and feel
// ---------------------------------------------------------------------------------------------
AudreyLookAndFeel::AudreyLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, text());
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, accent().withAlpha (0.35f));
    setColour (juce::TextEditor::backgroundColourId, panel());
    setColour (juce::TextEditor::textColourId, text());
    setColour (juce::TextEditor::outlineColourId, accent());
    setColour (juce::TextEditor::focusedOutlineColourId, accent());
    setColour (juce::CaretComponent::caretColourId, accent());
}

void AudreyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float trackWidth = 4.0f;
    const float arcRadius = radius - trackWidth * 0.5f;
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // body
    g.setColour (panel().brighter (0.08f));
    g.fillEllipse (centre.x - radius + 7.0f, centre.y - radius + 7.0f, (radius - 7.0f) * 2.0f, (radius - 7.0f) * 2.0f);

    // track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (dim().withAlpha (0.35f));
    g.strokePath (track, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (slider.isEnabled() ? accent() : dim());
    g.strokePath (value, juce::PathStrokeType (trackWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    juce::Path pointer;
    const float inner = radius * 0.30f, outer = radius * 0.68f;
    pointer.addRoundedRectangle (-1.5f, -outer, 3.0f, outer - inner, 1.5f);
    g.setColour (text());
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

juce::Label* AudreyLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, text());
    l->setColour (juce::Label::outlineWhenEditingColourId, accent());
    return l;
}

void AudreyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (4.0f, 14.0f);
    const bool on = b.getToggleState();

    g.setColour (on ? accent() : panel().brighter (0.12f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (highlighted ? text() : dim());
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    g.setColour (on ? juce::Colour (0xff1a1305) : text());
    g.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 1);
}

// ---------------------------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------------------------
namespace
{
    constexpr int kCellW = 96;
    constexpr int kCellH = 128;      // name + knob + value
    constexpr int kGroupHeader = 26;
    constexpr int kGroupPad = 10;
    constexpr int kGap = 12;
    constexpr int kMargin = 18;
    constexpr int kTitleH = 58;
    constexpr int kFooterH = 30;

    constexpr int kRowW = 9;         // widest row, in cells
    constexpr int kWidth = kMargin * 2 + kRowW * kCellW + 4 * (kGroupPad * 2) + 3 * kGap;   // widest row: 4 groups
    constexpr int kHeight = kTitleH + 2 * (kGroupHeader + kCellH + kGroupPad) + kGap + kFooterH + kMargin;

    const char* shortName (int id)
    {
        switch (id)
        {
            case kPitchOffset:     return "Freq Offset";
            case kFeedbackGain:    return "Fb Gain";
            case kBody:            return "Body";
            case kLpfCutoff:       return "LPF";
            case kHpfCutoff:       return "HPF";
            case kReverbMix:       return "Mix";
            case kReverbDecay:     return "Decay";
            case kOutputVolume:    return "Output";
            case kInputVolume:     return "Input";
            case kEnvelopeShape:   return "Shape";
            case kLfoFrequency:    return "Rate";
            case kLfoDistribution: return "Distrib";
            case kLfoOn:           return "LFO";
            case kDroneOn:         return "Drone";
            case kEchoSend:        return "Send";
            case kEchoTime:        return "Time";
            case kEchoFeedback:    return "Feedback";
            default:               return "";
        }
    }
}

AudreyEditor::AudreyEditor (AudreyProcessor& p) : juce::AudioProcessorEditor (p)
{
    setLookAndFeel (&laf);

    // Row 0: STRING, FILTER, REVERB, LEVELS   Row 1: ENVELOPE, LFO, ECHO
    groups = { Group { "STRING",   { kPitchOffset, kFeedbackGain, kBody, -1 },          0, {} },
               Group { "FILTER",   { kLpfCutoff, kHpfCutoff, -1, -1 },                  0, {} },
               Group { "REVERB",   { kReverbMix, kReverbDecay, -1, -1 },                0, {} },
               Group { "LEVELS",   { kInputVolume, kOutputVolume, -1, -1 },             0, {} },
               Group { "ENVELOPE", { kEnvelopeShape, kDroneOn, -1, -1 },                1, {} },
               Group { "LFO",      { kLfoFrequency, kLfoDistribution, kLfoOn, -1 },     1, {} },
               Group { "ECHO",     { kEchoSend, kEchoTime, kEchoFeedback, -1 },         1, {} } };

    for (int id = 0; id < kNumParams; ++id)
    {
        auto& cell = cells[(size_t) id];
        cell.paramId = id;
        const juce::String pid (paramInfo (id).id);

        if (paramInfo (id).toggle)
        {
            cell.toggle = std::make_unique<juce::ToggleButton> (juce::String (shortName (id)) + " on");
            cell.toggle->setButtonText (juce::String (shortName (id)).toUpperCase());
            addAndMakeVisible (*cell.toggle);
            cell.buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, pid, *cell.toggle);
        }
        else
        {
            cell.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            cell.slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            cell.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, kCellW - 8, 20);
            cell.slider->setTextBoxIsEditable (true);
            cell.slider->setMouseDragSensitivity (180);
            addAndMakeVisible (*cell.slider);
            cell.sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, pid, *cell.slider);

            // Show the same text as the host ("1.00 ms", "8.00 kHz"); the attachment already parses typed text.
            cell.slider->textFromValueFunction = [id] (double v) { return juce::String (formatValue (id, (float) v)); };
            cell.slider->updateText();
        }
    }

    setSize (kWidth, kHeight);
}

AudreyEditor::~AudreyEditor()
{
    setLookAndFeel (nullptr);   // before the look-and-feel member is destroyed
}

void AudreyEditor::resized()
{
    int rowTop[2] = { kTitleH, kTitleH + kGroupHeader + kCellH + kGroupPad + kGap };
    int x[2] = { kMargin, kMargin };

    for (auto& g : groups)
    {
        int n = 0;
        for (int id : g.ids) if (id >= 0) ++n;

        const int w = n * kCellW + kGroupPad * 2;
        g.bounds = { x[g.row], rowTop[g.row], w, kGroupHeader + kCellH + kGroupPad };

        int cx = g.bounds.getX() + kGroupPad;
        const int cy = g.bounds.getY() + kGroupHeader;
        for (int id : g.ids)
        {
            if (id < 0) continue;
            auto& cell = cells[(size_t) id];
            cell.bounds = { cx, cy, kCellW, kCellH };
            if (cell.slider != nullptr)
                cell.slider->setBounds (cell.bounds.withTrimmedTop (18));
            if (cell.toggle != nullptr)
                cell.toggle->setBounds (cell.bounds.withSizeKeepingCentre (kCellW - 8, 64));
            cx += kCellW;
        }
        x[g.row] += w + kGap;
    }
}

void AudreyEditor::paint (juce::Graphics& g)
{
    g.fillAll (AudreyLookAndFeel::background());

    g.setColour (AudreyLookAndFeel::text());
    g.setFont (juce::FontOptions (24.0f).withStyle ("Bold"));
    g.drawText ("UNOFFICIAL AUDREY", kMargin, 12, 360, 32, juce::Justification::centredLeft);
    g.setColour (AudreyLookAndFeel::dim());
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("feedback synth  |  play MIDI notes  |  CC 7 12 70 72 74 75 76 77 81 91 123",
                kMargin + 250, 16, getWidth() - kMargin * 2 - 250, 26, juce::Justification::centredRight);

    for (const auto& grp : groups)
    {
        g.setColour (AudreyLookAndFeel::panel());
        g.fillRoundedRectangle (grp.bounds.toFloat(), 10.0f);

        g.setColour (AudreyLookAndFeel::accent());
        g.setFont (juce::FontOptions (12.0f).withStyle ("Bold"));
        g.drawText (grp.title, grp.bounds.getX() + kGroupPad, grp.bounds.getY() + 4, grp.bounds.getWidth() - kGroupPad * 2, 20,
                    juce::Justification::centredLeft);

        g.setColour (AudreyLookAndFeel::dim());
        g.setFont (juce::FontOptions (12.0f));
        for (int id : grp.ids)
        {
            if (id < 0) continue;
            const auto& cell = cells[(size_t) id];
            if (cell.slider != nullptr)
                g.drawText (shortName (id), cell.bounds.getX(), cell.bounds.getY(), cell.bounds.getWidth(), 18, juce::Justification::centred);
        }
    }

    g.setColour (AudreyLookAndFeel::dim());
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Based on the open-source Audrey firmware (Infrasonic Audio / Synthux Academy, MIT). Not a Synthux Academy product.",
                kMargin, getHeight() - kFooterH - 4, getWidth() - kMargin * 2, 20, juce::Justification::centredLeft);
}
