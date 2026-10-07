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
    void styleSlider(juce::Slider&);

    PhysicalSnareAudioProcessor& processor;

    juce::Label title;
    juce::Label stageLabel;
    juce::Label statusLabel;

    juce::Slider tuneSlider;
    juce::Slider dampingSlider;
    juce::Slider hitPositionSlider;
    juce::Slider levelSlider;
    juce::Slider bottomTuneSlider;
    juce::Slider airSlider;
    juce::Slider shellSlider;
    juce::Slider depthSlider;
    juce::Slider snareSlider;
    juce::Slider wireSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> tuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dampingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> hitPositionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bottomTuneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> airAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> shellAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> snareAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wireAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalSnareAudioProcessorEditor)
};
