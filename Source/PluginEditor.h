#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class BlackDrumAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::FileDragAndDropTarget {
public:
 explicit BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor&);
 void paint(juce::Graphics&) override;
 void resized() override;
private:
 bool isInterestedInFileDrag(const juce::StringArray&) override;
 void filesDropped(const juce::StringArray&,int,int) override;
 void loadFrom(const juce::File&);
 BlackDrumAudioProcessor& processor;
 juce::TextButton loadButton{"LOAD SAMPLE"}, playButton{"PREVIEW"}, removeButton{"REMOVE"};
 juce::Label title, filename, hint;
 juce::Rectangle<int> dropArea;
 JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackDrumAudioProcessorEditor)
};