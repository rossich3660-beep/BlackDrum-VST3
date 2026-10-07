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

    void styleSlider(juce::Slider&, const juce::String& suffix);

    PhysicalSnareAudioProcessor& processor;

    juce::Label title;
    juce::Label stageLabel;
    juce::Label midiLabel;
    juce::Label velocityLabel;
    juce::Label eventsLabel;
    juce::Label infoLabel;

    juce::Slider tuneSlider;
    juce::Slider dampingSlider;
    juce::Slider hitPositionSlider;
    juce::Slider levelSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dampingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hitPositionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalSnareAudioProcessorEditor)
};
