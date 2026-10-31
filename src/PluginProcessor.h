#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include "FeedbackSynthEngine.h"

// Phase 1 skeleton: the Audrey engine runs with fixed settings, MIDI notes set the
// pitch and trigger the envelope. Host-automatable parameters come in Phase 2.
class AudreyProcessor : public juce::AudioProcessor
{
public:
    AudreyProcessor();
    ~AudreyProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    void renderRange (juce::AudioBuffer<float>& buffer, int start, int end);
    void handleMidi (const juce::MidiMessage& m);

    std::unique_ptr<infrasonic::FeedbackSynth::Engine> engine;
    bool engineReady = false;
    int heldNote = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudreyProcessor)
};
