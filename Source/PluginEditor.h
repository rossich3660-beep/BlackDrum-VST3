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
 void savePresetTo(const juce::File&);
 void loadPresetFrom(const juce::File&);
 bool isSupportedAudioFile(const juce::File&) const;
 BlackDrumAudioProcessor& processor;
 std::unique_ptr<juce::FileChooser> fileChooser;
 std::unique_ptr<juce::FileChooser> presetChooser;
 juce::TextButton loadButton{"LOAD SAMPLE"}, playButton{"PREVIEW"}, removeButton{"REMOVE"};
 juce::TextButton savePresetButton{"SAVE PRESET"}, loadPresetButton{"LOAD PRESET"};
 juce::Label title, filename, hint, voiceCountLabel, bodyMixLabel, spectralLabel, wireLabel, phaseVocoderLabel, attackLabel, sustainLabel, dynamicLabel, membraneTitle, tensionLabel, stiffnessLabel, decayLabel, velocityLabel, compressorLabel, roomReverbLabel, physicalSynthLabel, wireCollisionLabel;
 juce::Slider voiceCountSlider, bodyMixSlider, spectralSlider, wireSlider, phaseVocoderSlider, attackSlider, sustainSlider, dynamicSlider, tensionSlider, stiffnessSlider, decaySlider, velocitySlider, compressorSlider, roomReverbSlider, physicalSynthSlider, wireCollisionSlider;
    juce::ToggleButton membraneToggle { "MEMBRANE MODEL" };
 juce::Rectangle<int> dropArea;
 bool dragHover=false;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackDrumAudioProcessorEditor)
};