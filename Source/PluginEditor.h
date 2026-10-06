#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class BlackDrumAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::FileDragAndDropTarget
{
public:
    explicit BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor&);
    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;

private:
    void loadFrom(const juce::File&);
    void savePresetTo(const juce::File&);
    void loadPresetFrom(const juce::File&);
    bool isSupportedAudioFile(const juce::File&) const;
    void setupKnob(juce::Slider&, juce::Label&, const juce::String&, float,
                   std::function<void(float)>);
    void syncFromProcessor();

    BlackDrumAudioProcessor& processor;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::FileChooser> presetChooser;

    juce::Label title, sampleName, status;
    juce::TextButton loadButton{"LOAD SAMPLE"};
    juce::TextButton previewButton{"PREVIEW"};
    juce::TextButton savePresetButton{"SAVE"};
    juce::TextButton loadPresetButton{"LOAD"};

    juce::Label topTuneLabel, topDampLabel, bottomTuneLabel, bottomDampLabel;
    juce::Label diameterLabel, depthLabel, materialLabel, resonanceLabel;
    juce::Label wireTensionLabel, wireAmountLabel, wireDampingLabel, stickLabel;
    juce::Label sampleRefLabel, velocityLabel, roomLabel, masterLabel, voicesLabel;
    juce::Slider topTune, topDamp, bottomTune, bottomDamp;
    juce::Slider diameter, depth, material, resonance;
    juce::Slider wireTension, wireAmount, wireDamping, stick;
    juce::Slider sampleRef, velocity, room, master, voices;

    bool dragHover = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackDrumAudioProcessorEditor)
};
