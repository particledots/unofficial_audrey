#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

class AudreyEditor : public juce::AudioProcessorEditor
{
public:
    explicit AudreyEditor (AudreyProcessor&);
    void paint (juce::Graphics&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudreyEditor)
};
