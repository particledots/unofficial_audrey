#include "PluginEditor.h"

AudreyEditor::AudreyEditor (AudreyProcessor& p) : AudioProcessorEditor (p)
{
    setSize (420, 160);
}

void AudreyEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c1e));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f));
    g.drawFittedText ("Unofficial Audrey", getLocalBounds().removeFromTop (90), juce::Justification::centred, 1);
    g.setFont (juce::FontOptions (13.0f));
    g.setColour (juce::Colours::grey);
    g.drawFittedText ("Phase 1 skeleton: no controls yet. Play MIDI notes.",
                      getLocalBounds().withTrimmedTop (80), juce::Justification::centredTop, 2);
}
