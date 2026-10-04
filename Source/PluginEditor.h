#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class BlackDrumAudioProcessorEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget {
public:
 explicit BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor&);
 void paint(juce::Graphics&) override;
 void resized() override;
 bool isInterestedInFileDrag(const juce::StringArray&) override;
 void fileDragEnter(const juce::StringArray&,int,int) override;
 void fileDragExit(const juce::StringArray&) override;
 void filesDropped(const juce::StringArray&,int,int) override;
private:
 void loadFrom(const juce::File&);
 bool isSupportedAudioFile(const juce::File&) const;
 BlackDrumAudioProcessor& processor;
 std::unique_ptr<juce::FileChooser> fileChooser;
 juce::TextButton loadButton{"LOAD SAMPLE"}, playButton{"PREVIEW"}, removeButton{"REMOVE"};
 juce::Label title, filename, hint, voiceCountLabel, bodyMixLabel, spectralLabel, wireLabel, attackLabel, sustainLabel, dynamicLabel, membraneTitle, tensionLabel, stiffnessLabel, decayLabel, velocityLabel;
 juce::Slider voiceCountSlider, bodyMixSlider, spectralSlider, wireSlider, attackSlider, sustainSlider, dynamicSlider, tensionSlider, stiffnessSlider, decaySlider, velocitySlider;
    juce::ToggleButton membraneToggle { "MEMBRANE MODEL" };
 juce::Rectangle<int> dropArea;
 bool dragHover=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackDrumAudioProcessorEditor)
};