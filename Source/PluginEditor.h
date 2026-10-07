#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class PhysicalSnareAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit PhysicalSnareAudioProcessorEditor(PhysicalSnareAudioProcessor&);
    ~PhysicalSnareAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    PhysicalSnareAudioProcessor& processor;

    juce::Label title;
    juce::Label stageLabel;
    juce::Label midiLabel;
    juce::Label velocityLabel;
    juce::Label eventsLabel;
    juce::Label infoLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalSnareAudioProcessorEditor)
};
