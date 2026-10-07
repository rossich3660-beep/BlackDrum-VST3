#pragma once
#include <JuceHeader.h>
#include <atomic>

class PhysicalSnareAudioProcessor : public juce::AudioProcessor
{
public:
    PhysicalSnareAudioProcessor();
    ~PhysicalSnareAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Physical Snare"; }
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

    int getLastMidiNote() const noexcept { return lastMidiNote.load(std::memory_order_relaxed); }
    int getLastMidiVelocity() const noexcept { return lastMidiVelocity.load(std::memory_order_relaxed); }
    int getMidiEventCount() const noexcept { return midiEventCount.load(std::memory_order_relaxed); }

private:
    double currentSampleRate = 44100.0;
    int currentBlockSize = 0;

    std::atomic<int> lastMidiNote { -1 };
    std::atomic<int> lastMidiVelocity { 0 };
    std::atomic<int> midiEventCount { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalSnareAudioProcessor)
};
