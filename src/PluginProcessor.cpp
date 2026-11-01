#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace audrey;

juce::AudioProcessorValueTreeState::ParameterLayout AudreyProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamInfo& p = paramInfo (i);
        const juce::ParameterID pid { p.id, 1 };   // version hint 1: keep ids stable across releases

        if (p.toggle)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, p.name, p.def >= 0.5f));
            continue;
        }

        juce::NormalisableRange<float> range (
            p.min, p.max,
            [i] (float, float, float norm)  { return toReal (i, norm); },
            [i] (float, float, float value) { return toNormalised (i, value); },
            [i] (float, float, float value) { return juce::jlimit (paramInfo (i).min, paramInfo (i).max, value); });

        auto attrs = juce::AudioParameterFloatAttributes()
                         .withLabel (p.unit)
                         .withStringFromValueFunction ([] (float v, int)
                         {
                             const float a = std::abs (v);
                             return juce::String (v, a < 1.0f ? 3 : (a < 100.0f ? 2 : 1));
                         });

        layout.add (std::make_unique<juce::AudioParameterFloat> (pid, p.name, range, p.def, attrs));
    }
    return layout;
}

AudreyProcessor::AudreyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AudreyState", createLayout())
{
    for (int i = 0; i < kNumParams; ++i)
    {
        rawParam[i] = apvts.getRawParameterValue (paramInfo (i).id);
        ccPending[i].store (std::nanf (""));
        ccEcho[i].store (std::nanf (""));
        lastSeen[i] = rawParam[i]->load();
    }
    startTimerHz (30);
}

bool AudreyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo()
                           || in == juce::AudioChannelSet::mono();
}

// Pushes host parameter changes to the controller. Only a *change* of the host value is pushed, so a value
// set by a MIDI CC stays until the host/UI moves that parameter. force = take every host value as it is.
void AudreyProcessor::syncParameters (bool force)
{
    for (int i = 0; i < kNumParams; ++i)
    {
        const float raw = rawParam[i]->load();

        if (force)
        {
            lastSeen[i] = raw;
            ccEcho[i].store (std::nanf (""));
            controller.setParam (i, raw);
            continue;
        }

        if (raw == lastSeen[i]) continue;
        lastSeen[i] = raw;

        // The timer writes CC values to the host; seeing one of those come back is not a host change.
        const float echo = ccEcho[i].load();
        if (std::isfinite (echo) && std::abs (raw - echo) <= 1.0e-4f * (std::abs (echo) + 1.0e-3f))
        {
            ccEcho[i].store (std::nanf (""));
            continue;
        }

        ccPending[i].store (std::nanf (""));   // the host/UI moved it: an older CC value must not overwrite that
        controller.setParam (i, raw);
    }
}

// Message thread: show CC-driven parameter changes to the host (UI, automation recording).
void AudreyProcessor::timerCallback()
{
    for (int i = 0; i < kNumParams; ++i)
    {
        const float v = ccPending[i].exchange (std::nanf (""));
        if (! std::isfinite (v)) continue;

        if (auto* param = apvts.getParameter (paramInfo (i).id))
        {
            ccEcho[i].store (v);
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 (v));
            param->endChangeGesture();
        }
    }
}

void AudreyProcessor::prepareToPlay (double sampleRate, int)
{
    syncParameters (true);   // start at the host's current values, not the defaults
    controller.prepare (static_cast<float> (sampleRate));
}

void AudreyProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (! controller.ready()) return;

    if (m.isNoteOn())
        controller.noteOn (m.getNoteNumber());
    else if (m.isNoteOff())
        controller.noteOff (m.getNoteNumber());
    else if (m.isAllSoundOff())
        controller.allNotesOff();
    else if (m.isController())
    {
        // Touch CC map (7, 12, 70, 72, 74, 75, 76, 77, 81, 91; 123 = all notes off)
        const auto res = controller.handleCC (m.getControllerNumber(), m.getControllerValue());
        if (res.paramId >= 0)
            ccPending[res.paramId].store (res.real);
    }
}

void AudreyProcessor::renderRange (juce::AudioBuffer<float>& buffer, int start, int end)
{
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = numOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* in1 = numIn >= 2 ? buffer.getReadPointer (1) : nullptr;

    for (int i = start; i < end; ++i)
    {
        float in = 0.0f;
        if (numIn >= 2)      in = 0.5f * (ch0[i] + in1[i]);
        else if (numIn == 1) in = ch0[i];

        float l = 0.0f, r = 0.0f;
        controller.process (in, l, r);
        ch0[i] = l;
        if (ch1 != nullptr) ch1[i] = r;
    }
}

void AudreyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    if (! controller.ready() || getTotalNumOutputChannels() < 1)
    {
        buffer.clear();
        return;
    }

    syncParameters (false);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (pos, numSamples, meta.samplePosition);
        renderRange (buffer, pos, at);
        pos = at;
        handleMidi (meta.getMessage());
    }
    renderRange (buffer, pos, numSamples);
}

void AudreyProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void AudreyProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* AudreyProcessor::createEditor()
{
    return new AudreyEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudreyProcessor();
}
