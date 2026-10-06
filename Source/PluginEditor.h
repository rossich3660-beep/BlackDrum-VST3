#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "SampleAnalyzer.h"
#include <array>

class BlackDrumAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::FileDragAndDropTarget,
                                       private juce::AsyncUpdater
{
public:
    explicit BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor&);
    ~BlackDrumAudioProcessorEditor() override;
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
    void startAnalysis(bool forceApply);
    void handleAsyncUpdate() override;
    void syncAutoControls(const AutoSettings&);
    void syncEngineControls();

    struct AnalysisJob;
    BlackDrumAudioProcessor& processor;
    std::unique_ptr<juce::ThreadPool> analysisPool;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::FileChooser> presetChooser;

    std::array<std::atomic<float>, 12> pendingFeatures {};
    std::array<std::atomic<float>, 9> pendingAuto {};
    std::atomic<bool> analysisReady { false };
    std::atomic<bool> analysisRunning { false };
    bool analysisForceApply = true;

    juce::Label title, subtitle, sampleLabel, analysisStatus, featureSummary, hint, voiceCountLabel;
    juce::Label bodyMixLabel, spectralLabel, wireLabel, phaseVocoderLabel, shellLabel;
    juce::Label attackLabel, sustainLabel, dynamicLabel, membraneTitle, tensionLabel, stiffnessLabel, decayLabel, velocityLabel;
    juce::Label autoTitle, autoHint, brightnessLabel, snapLabel, noiseLayerLabel, attackSoftLabel, tailShortenLabel;
    juce::Label pitchDropLabel, saturationLabel, bodyBoostLabel, velocityCurveLabel;

    juce::Slider voiceCountSlider, bodyMixSlider, spectralSlider, wireSlider, phaseVocoderSlider, shellSlider;
    juce::Slider attackSlider, sustainSlider, dynamicSlider, tensionSlider, stiffnessSlider, decaySlider, velocitySlider;
    juce::Slider brightnessSlider, snapSlider, noiseLayerSlider, attackSoftSlider, tailShortenSlider;
    juce::Slider pitchDropSlider, saturationSlider, bodyBoostSlider, velocityCurveSlider;

    juce::ToggleButton membraneToggle { "MEMBRANE MODEL" };
    juce::TextButton autoButton { "AUTO" }, loadButton { "LOAD SAMPLE" }, playButton { "PREVIEW" };
    juce::TextButton removeButton { "REMOVE" }, savePresetButton { "SAVE PRESET" }, loadPresetButton { "LOAD PRESET" };

    juce::Rectangle<int> dropArea;
    bool dragHover = false;

    JUCE_DECLARE_WEAK_REFERENCEABLE(BlackDrumAudioProcessorEditor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackDrumAudioProcessorEditor)
};
