#include "PluginProcessor.h"
#include "PluginEditor.h"

AudreyProcessor::AudreyProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool AudreyProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo()
                           || in == juce::AudioChannelSet::mono();
}

void AudreyProcessor::prepareToPlay (double sampleRate, int)
{
    auto e = std::make_unique<infrasonic::FeedbackSynth::Engine>();
    engineReady = e->Init (static_cast<float> (sampleRate));
    if (engineReady)
    {
        // Fixed Phase-1 settings (same as the headless test). Real parameters: Phase 2.
        e->SetStringPitch (40.0f);
        e->SetFeedbackGain (-6.0f);
        e->SetFeedbackDelay (0.01f);
        e->SetFeedbackLPFCutoff (8000.0f);
        e->SetFeedbackHPFCutoff (250.0f);
        e->SetReverbMix (0.3f);
        e->SetReverbFeedback (0.6f);
        e->SetOutputLevel (0.5f);
        e->SetInputLevel (0.0f);
    }
    heldNote = -1;
    engine = std::move (e);
}

void AudreyProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (engine == nullptr || ! engineReady) return;

    if (m.isNoteOn())
    {
        heldNote = m.getNoteNumber();
        engine->SetStringPitch (static_cast<float> (heldNote));
        engine->NoteOn();
    }
    else if (m.isNoteOff())
    {
        if (m.getNoteNumber() == heldNote)
        {
            heldNote = -1;
            engine->NoteOff();
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        heldNote = -1;
        engine->NoteOff();
    }
}

void AudreyProcessor::renderRange (juce::AudioBuffer<float>& buffer, int start, int end)
{
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = numOut > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = start; i < end; ++i)
    {
        float in = 0.0f;
        if (numIn >= 2)      in = 0.5f * (ch0[i] + buffer.getReadPointer (1)[i]);
        else if (numIn == 1) in = ch0[i];

        float l = 0.0f, r = 0.0f;
        engine->Process (in, l, r);
        ch0[i] = l;
        if (ch1 != nullptr) ch1[i] = r;
    }
}

void AudreyProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    if (engine == nullptr || ! engineReady || getTotalNumOutputChannels() < 1)
    {
        buffer.clear();
        return;
    }

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

juce::AudioProcessorEditor* AudreyProcessor::createEditor()
{
    return new AudreyEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudreyProcessor();
}
