#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

// Interim editor: JUCE's generic parameter sliders. The real UI replaces this later in Phase 2.
class AudreyEditor : public juce::GenericAudioProcessorEditor
{
public:
    explicit AudreyEditor (AudreyProcessor& p) : juce::GenericAudioProcessorEditor (p) { setSize (440, 720); }
};
