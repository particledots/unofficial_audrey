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
    setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1a1a1d));
    setColour (juce::TextEditor::textColourId, text());
    setColour (juce::TextEditor::outlineColourId, accent());
    setColour (juce::TextEditor::focusedOutlineColourId, accent());
    setColour (juce::CaretComponent::caretColourId, accent());
}

void AudreyLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                          float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 1.0f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // value ring, just outside the knob
    const float ringR = radius - 0.5f;
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, ringR, ringR, 0.0f, startAngle, angle, true);
    g.setColour (dim().withAlpha (0.45f));
    g.strokePath (track, juce::PathStrokeType (1.2f * uiScale, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    g.setColour (slider.isEnabled() ? line() : dim());
    g.strokePath (value, juce::PathStrokeType (2.4f * uiScale, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // black skirt, white cap
    const float skirtR = radius * 0.86f, capR = radius * 0.72f;
    g.setColour (juce::Colour (0xff020203));
    g.fillEllipse (centre.x - skirtR, centre.y - skirtR, skirtR * 2.0f, skirtR * 2.0f);
    g.setColour (knob());
    g.fillEllipse (centre.x - capR, centre.y - capR, capR * 2.0f, capR * 2.0f);

    // pointer notch on the cap
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f * uiScale, -capR * 0.92f, 3.2f * uiScale, capR * 0.5f, 1.6f * uiScale);
    g.setColour (juce::Colour (0xff17171a));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

juce::Label* AudreyLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textColourId, text());
    l->setColour (juce::Label::outlineWhenEditingColourId, accent());
    l->setFont (juce::FontOptions (12.0f));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

void AudreyLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    if (label.isBeingEdited()) return;   // the text editor draws itself
    g.setColour (label.findColour (juce::Label::textColourId));
    g.setFont (juce::FontOptions (12.0f * uiScale));
    g.drawFittedText (label.getText(), label.getLocalBounds().reduced (2, 0), label.getJustificationType(), 1,
                      label.getMinimumHorizontalScale());
}

void AudreyLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const bool on = b.getToggleState();
    const auto area = b.getLocalBounds().toFloat();
    const float u = uiScale;
    const auto sw = juce::Rectangle<float> (38.0f * u, 18.0f * u).withCentre ({ area.getCentreX(), area.getY() + 14.0f * u });

    g.setColour (on ? knob() : juce::Colour (0xff17171a));
    g.fillRoundedRectangle (sw, 9.0f * u);
    g.setColour (highlighted ? text() : line().withAlpha (0.8f));
    g.drawRoundedRectangle (sw, 9.0f * u, 1.2f * u);

    const float d = 12.0f * u;
    const float kx = on ? sw.getRight() - 3.0f * u - d : sw.getX() + 3.0f * u;
    g.setColour (on ? juce::Colour (0xff17171a) : line());
    g.fillEllipse (kx, sw.getCentreY() - d * 0.5f, d, d);

    g.setColour (on ? text() : dim());
    g.setFont (juce::FontOptions (12.5f * u));
    g.drawText (b.getButtonText(), area.withTrimmedTop (28.0f * u), juce::Justification::centredTop);
}

// ---------------------------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------------------------
namespace
{
    constexpr float kRefW = 1200.0f, kRefH = 840.0f;   // layout is designed on this canvas, then scaled
    constexpr float kScale = 0.85f;                    // scale of the default window
    constexpr float kAspect = kRefW / kRefH;
    constexpr int kMinWidth = 612, kMaxWidth = 2040;
    constexpr int kWidth  = (int) (kRefW * kScale);    // 1020

    struct Place { int id; float x, y, dia; };

    // Signal flow: Frequency -> string ring (Feedback Gain) -> reverb -> filters -> Body -> back to the string,
    // then the echo on the right. Extra Touch controls run along the bottom.
    const Place kPlaces[] = {
        { kFeedbackGain,    280, 195, 128 },
        { kPitchOffset,     138, 362,  96 },
        { kReverbMix,       563, 108,  68 },
        { kReverbDecay,     565, 278,  68 },
        { kLpfCutoff,       420, 360,  68 },
        { kHpfCutoff,       707, 362,  68 },
        { kBody,            550, 440, 118 },
        { kBodyTune,        707, 480,   0 },
        { kEchoSend,        980, 278,  68 },
        { kEchoFeedback,    850, 360,  68 },
        { kEchoTime,        990, 440, 118 },
        { kOutputVolume,    990, 108,  68 },
        { kInputVolume,     130, 700,  68 },
        { kEnvelopeShape,   270, 700,  68 },
        { kDroneOn,         380, 700,   0 },
        { kLfoFrequency,    560, 700,  68 },
        { kLfoDistribution, 700, 700,  68 },
        { kLfoOn,           810, 700,   0 },
    };

    const char* shortName (int id)
    {
        switch (id)
        {
            case kPitchOffset:     return "frequency offset";
            case kFeedbackGain:    return "feedback gain";
            case kBody:            return "body";
            case kLpfCutoff:       return "lowpass";
            case kHpfCutoff:       return "highpass";
            case kReverbMix:       return "reverb mix";
            case kReverbDecay:     return "decay";
            case kOutputVolume:    return "volume";
            case kInputVolume:     return "input";
            case kEnvelopeShape:   return "envelope shape";
            case kLfoFrequency:    return "lfo rate";
            case kLfoDistribution: return "lfo distribution";
            case kLfoOn:           return "lfo";
            case kDroneOn:         return "drone";
            case kEchoSend:        return "echo send";
            case kEchoTime:        return "echo time";
            case kEchoFeedback:    return "echo feedback";
            case kBodyTune:        return "body tune";
            default:               return "";
        }
    }

}

AudreyEditor::AudreyEditor (AudreyProcessor& p) : juce::AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&laf);

    for (const auto& pl : kPlaces)
    {
        auto& cell = cells[(size_t) pl.id];
        cell.paramId = pl.id;
        cell.x = pl.x; cell.y = pl.y; cell.dia = pl.dia;
        const juce::String pid (paramInfo (pl.id).id);

        if (paramInfo (pl.id).toggle)
        {
            cell.toggle = std::make_unique<juce::ToggleButton> (juce::String (shortName (pl.id)) + " on");
            cell.toggle->setButtonText (shortName (pl.id));
            addAndMakeVisible (*cell.toggle);
            cell.buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, pid, *cell.toggle);
        }
        else
        {
            cell.slider = std::make_unique<juce::Slider> (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
            cell.slider->setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            cell.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, valueH);
            cell.slider->setTextBoxIsEditable (true);
            cell.slider->setMouseDragSensitivity (180);
            addAndMakeVisible (*cell.slider);
            cell.sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (p.apvts, pid, *cell.slider);

            // Show the same text as the host ("1.00 ms", "8.00 kHz"); the attachment already parses typed text.
            const int id = pl.id;
            cell.slider->textFromValueFunction = [id] (double v) { return juce::String (formatValue (id, (float) v)); };
            cell.slider->updateText();
        }
    }

    // resizable, keeping the panel's proportions; the last size is kept in the saved state
    constrainer.setFixedAspectRatio ((double) kAspect);
    constrainer.setSizeLimits (kMinWidth, (int) (kMinWidth / kAspect), kMaxWidth, (int) (kMaxWidth / kAspect));
    setConstrainer (&constrainer);
    setResizable (true, true);

    int w = kWidth;
    const juce::var saved = p.apvts.state.getProperty ("editorWidth");
    if (saved.isInt() || saved.isInt64() || saved.isDouble()) w = juce::jlimit (kMinWidth, kMaxWidth, (int) saved);
    setSize (w, (int) std::lround ((double) w / kAspect));
}

AudreyEditor::~AudreyEditor()
{
    setLookAndFeel (nullptr);   // before the look-and-feel member is destroyed
}

void AudreyEditor::resized()
{
    const float S = (float) getWidth() / kRefW;     // reference canvas -> pixels
    const float u = S / kScale;                      // 1.0 at the default size
    laf.setUiScale (u);
    valueH = juce::jmax (10, (int) std::lround (16.0f * u));
    auto sc = [S] (float x, float y) { return juce::Point<float> { x * S, y * S }; };
    if (getWidth() > 0) proc.apvts.state.setProperty ("editorWidth", getWidth(), nullptr);

    for (auto& cell : cells)
    {
        if (cell.paramId < 0) continue;
        const auto c = sc (cell.x, cell.y);

        if (cell.slider != nullptr)
        {
            const float d = cell.dia * S;
            const float w = juce::jmax (d, 84.0f * u);
            cell.slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, (int) (80.0f * u), valueH);
            cell.slider->setBounds (juce::Rectangle<float> (c.x - w * 0.5f, c.y - d * 0.5f, w, d + (float) valueH).toNearestInt());
            cell.labelBounds = { c.x - 70.0f * u, c.y + d * 0.5f + (float) valueH + 1.0f, 140.0f * u, 16.0f * u };
        }
        else if (cell.toggle != nullptr)
        {
            cell.toggle->setBounds (juce::Rectangle<float> (c.x - 60.0f * u, c.y - 14.0f * u, 120.0f * u, 50.0f * u).toNearestInt());
        }
    }

    // ---- ring graphics: our own concentric-arc textures around the three big knobs (fixed seed = stable look)
    ringArt.clear();
    juce::Random rnd (20241031);
    auto rings = [&] (float cx, float cy, float r0, float r1, float step, float centreAngle, float halfSpan)
    {
        const auto c = sc (cx, cy);
        for (float r = r0; r <= r1; r += step * (0.7f + 0.6f * rnd.nextFloat()))
        {
            const float span = halfSpan * (0.65f + 0.35f * rnd.nextFloat());
            const float a0 = centreAngle - span + (rnd.nextFloat() - 0.5f) * 0.5f;
            const float a1 = centreAngle + span + (rnd.nextFloat() - 0.5f) * 0.5f;
            ringArt.startNewSubPath (c.x + std::sin (a0) * r * S, c.y - std::cos (a0) * r * S);
            ringArt.addCentredArc (c.x, c.y, r * S, r * S, 0.0f, a0, a1, true);
        }
    };
    const float pi = juce::MathConstants<float>::pi;
    rings (280, 195, 72, 172, 3.4f, pi, pi * 0.999f);   // feedback gain: almost full circles
    rings (550, 440, 72, 152, 6.0f, pi, pi * 0.62f);    // body: lower arcs
    rings (990, 440, 72, 142, 6.5f, pi * 0.85f, pi * 0.50f);   // echo time: lower right arcs
    rings (990, 108, 40, 66, 5.5f, pi * 0.25f, pi * 0.40f);    // volume: small upper right arcs

    // ---- signal-flow hairlines
    flowLines.clear();
    arrowHeads.clear();
    auto arrow = [&] (juce::Point<float> tip, float dirX, float dirY)   // dir = direction of travel
    {
        const float s = 6.0f * u;
        const juce::Point<float> back { tip.x - dirX * s, tip.y - dirY * s };
        arrowHeads.startNewSubPath (tip);
        arrowHeads.lineTo (back.x - dirY * s * 0.55f, back.y + dirX * s * 0.55f);
        arrowHeads.lineTo (back.x + dirY * s * 0.55f, back.y - dirX * s * 0.55f);
        arrowHeads.closeSubPath();
    };
    auto poly = [&] (std::initializer_list<juce::Point<float>> pts)
    {
        bool first = true;
        for (auto pt : pts)
        {
            const auto q = sc (pt.x, pt.y);
            if (first) flowLines.startNewSubPath (q); else flowLines.lineTo (q);
            first = false;
        }
    };
    poly ({ { 450, 195 }, { 500, 195 }, { 500, 108 }, { 523, 108 } });   arrow (sc (530, 108), 1.0f, 0.0f);   // string -> reverb
    poly ({ { 563, 186 }, { 563, 236 } });                                  arrow (sc (563, 242), 0.0f, 1.0f);   // mix -> decay
    poly ({ { 565, 350 }, { 565, 378 }, { 482, 378 } });                    arrow (sc (480, 378), -1.0f, 0.0f);  // reverb -> filters
    poly ({ { 410, 560 }, { 300, 560 }, { 300, 372 } });                    arrow (sc (300, 366), 0.0f, -1.0f);  // body -> back to the string
}

void AudreyEditor::paint (juce::Graphics& g)
{
    const float u = (float) getWidth() / kRefW / kScale;
    g.fillAll (AudreyLookAndFeel::background());

    g.setColour (AudreyLookAndFeel::line().withAlpha (0.55f));
    g.strokePath (ringArt, juce::PathStrokeType (0.9f * u));

    g.setColour (AudreyLookAndFeel::line().withAlpha (0.7f));
    g.strokePath (flowLines, juce::PathStrokeType (1.0f * u));
    g.fillPath (arrowHeads);

    // dark backing behind each value + name so the ring graphics never run through the text
    g.setColour (AudreyLookAndFeel::background().withAlpha (0.9f));
    for (const auto& cell : cells)
    {
        if (cell.slider == nullptr) continue;
        const float top = (float) cell.slider->getBottom() - (float) valueH - 1.0f;
        const float cx = cell.labelBounds.getCentreX();
        g.fillRoundedRectangle (cx - 52.0f * u, top, 104.0f * u, cell.labelBounds.getBottom() + 2.0f * u - top, 6.0f * u);
    }

    // names under the value boxes
    g.setFont (juce::FontOptions (13.5f * u));
    for (const auto& cell : cells)
    {
        if (cell.slider == nullptr) continue;
        g.setColour (AudreyLookAndFeel::text());
        g.drawText (shortName (cell.paramId), cell.labelBounds, juce::Justification::centred);
    }

    // title and notes
    g.setColour (AudreyLookAndFeel::text());
    g.setFont (juce::FontOptions (21.0f * u));
    g.drawText ("unofficial audrey", (int) (24 * u), (int) (16 * u), (int) (300 * u), (int) (28 * u), juce::Justification::centredLeft);

    g.setColour (AudreyLookAndFeel::dim());
    g.setFont (juce::FontOptions (11.5f * u));
    g.drawText ("play MIDI notes  |  CC 7 12 70 72 74 75 76 77 81 91 123", getWidth() - (int) (440 * u), (int) (20 * u), (int) (416 * u), (int) (20 * u), juce::Justification::centredRight);
    g.drawText ("Based on the open-source Audrey firmware (Infrasonic Audio / Synthux Academy, MIT). Not a Synthux Academy product.",
                (int) (24 * u), getHeight() - (int) (26 * u), getWidth() - (int) (48 * u), (int) (18 * u), juce::Justification::centredLeft);
}
