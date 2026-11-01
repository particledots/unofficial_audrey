#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "AudreyController.h"

// Unofficial Audrey: the Audrey feedback-synth engine as a MIDI-controlled effect.
// Phase 2: 17 host-automatable parameters (see control/AudreyParams.h), smoothed, the Touch MIDI CC map,
// and state save/restore. A CC moves the sound on the audio thread and the matching host parameter a few
// milliseconds later on the message thread (the host is never called from the audio thread).
class AudreyProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    AudreyProcessor();
    ~AudreyProcessor() override { stopTimer(); }

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

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void renderRange (juce::AudioBuffer<float>& buffer, int start, int end);
    void handleMidi (const juce::MidiMessage& m);
    void syncParameters (bool force);
    void timerCallback() override;

    audrey::Controller controller;
    std::atomic<float>* rawParam[audrey::kNumParams] = {};

    // Host <-> CC bookkeeping (see syncParameters / timerCallback)
    std::atomic<float> ccPending[audrey::kNumParams];   // real value waiting to be shown to the host (NaN = none)
    std::atomic<float> ccEcho[audrey::kNumParams];      // value the timer just wrote to the host: not a host change
    float lastSeen[audrey::kNumParams] = {};            // audio thread only

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudreyProcessor)
};
