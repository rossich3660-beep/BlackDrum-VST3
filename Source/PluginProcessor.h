#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "MembraneModel.h"

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

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState parameters;
    SnareMembraneModel membrane;

    std::atomic<int> lastMidiNote { -1 };
    std::atomic<int> lastMidiVelocity { 0 };

    std::atomic<float>* tuningParameter = nullptr;
    std::atomic<float>* dampingParameter = nullptr;
    std::atomic<float>* hitPositionParameter = nullptr;
    std::atomic<float>* levelParameter = nullptr;
    std::atomic<float>* bottomTuneParameter = nullptr;
    std::atomic<float>* airCouplingParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhysicalSnareAudioProcessor)
};
