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
        rawParam[i] = apvts.getRawParameterValue (paramInfo (i).id);
}

bool AudreyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo()
                           || in == juce::AudioChannelSet::mono();
}

void AudreyProcessor::syncParameters()
{
    for (int i = 0; i < kNumParams; ++i)
        controller.setParam (i, rawParam[i]->load());
}

void AudreyProcessor::prepareToPlay (double sampleRate, int)
{
    syncParameters();   // start at the host's current values, not the defaults
    controller.prepare (static_cast<float> (sampleRate));
}

void AudreyProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (! controller.ready()) return;

    if (m.isNoteOn())
        controller.noteOn (m.getNoteNumber());
    else if (m.isNoteOff())
        controller.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        controller.allNotesOff();
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

    syncParameters();

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
