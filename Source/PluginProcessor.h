#pragma once
#include <JuceHeader.h>

class BlackDrumAudioProcessor : public juce::AudioProcessor {
public:
    BlackDrumAudioProcessor();
    void prepareToPlay(double, int) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "BlackDrum"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    bool loadSample(const juce::File&);
    juce::String sampleName() const { return loadedFile.getFileName(); }
    juce::AudioBuffer<float> sampleCopy() const;
    double sampleRateOfFile() const { return sourceRate; }

private:
    juce::AudioFormatManager formats;
    juce::AudioBuffer<float> sample;
    juce::File loadedFile;
    double sourceRate=44100.0, outputRate=44100.0, playbackPosition=-1.0;
    float voiceGain=1.0f, playbackRate=1.0f, filterCoefficient=1.0f;
    float filterState[2]={0.0f,0.0f};
    // A damped, low-level resonator adds a little drum-body energy without replacing the sample.
    float resonatorY1[2]={0.0f,0.0f}, resonatorY2[2]={0.0f,0.0f};
    float resonatorB0=0.0f, resonatorB1=0.0f, resonatorB2=0.0f;
    float resonatorA1=0.0f, resonatorA2=0.0f;
    float hitVelocity=0.5f;
    juce::CriticalSection sampleLock;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackDrumAudioProcessor)
};