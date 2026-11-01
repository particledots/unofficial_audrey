#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "AudreyController.h"

// Unofficial Audrey: the Audrey feedback-synth engine as a MIDI-controlled effect.
// Phase 2: 17 host-automatable parameters (see control/AudreyParams.h), smoothed, plus state save/restore.
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

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void renderRange (juce::AudioBuffer<float>& buffer, int start, int end);
    void handleMidi (const juce::MidiMessage& m);
    void syncParameters();

    audrey::Controller controller;
    std::atomic<float>* rawParam[audrey::kNumParams] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudreyProcessor)
};
