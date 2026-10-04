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
    void setBodyMix(float value) { bodyMix.store(juce::jlimit(0.0f, 1.0f, value)); }
    float getBodyMix() const { return bodyMix.load(); }
    void setWireNoiseMix(float value) { wireNoiseMix.store(juce::jlimit(0.0f, 1.0f, value)); }
    float getWireNoiseMix() const { return wireNoiseMix.load(); }
    void setSpectralMix(float value) { spectralMix.store(juce::jlimit(0.0f, 1.0f, value)); }
    float getSpectralMix() const { return spectralMix.load(); }
    void setTransient(float v) { transientAmount.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getTransient() const { return transientAmount.load(); }
    void setSustain(float v) { sustainAmount.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getSustain() const { return sustainAmount.load(); }
    void setDynamicResponse(float v) { dynamicResponse.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getDynamicResponse() const { return dynamicResponse.load(); }
    void setMembraneEnabled(bool v) { membraneEnabled.store(v); }
    bool getMembraneEnabled() const { return membraneEnabled.load(); }
    void setMembraneTension(float v) { membraneTension.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getMembraneTension() const { return membraneTension.load(); }
    void setMembraneStiffness(float v) { membraneStiffness.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getMembraneStiffness() const { return membraneStiffness.load(); }
    void setMembraneDecay(float v) { membraneDecay.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getMembraneDecay() const { return membraneDecay.load(); }
    void setMembraneVelocity(float v) { membraneVelocity.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getMembraneVelocity() const { return membraneVelocity.load(); }

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
    std::atomic<float> bodyMix { 0.35f };
    std::atomic<float> wireNoiseMix { 0.0f };
    float hitPitchVariation = 1.0f;
    float hitAttackVariation = 1.0f;
    float hitResonanceVariation = 1.0f;
    float hitNoiseVariation = 1.0f;
    std::atomic<float> spectralMix { 0.0f };
    std::atomic<float> transientAmount { 0.5f };
    std::atomic<float> sustainAmount { 0.5f };
    std::atomic<float> dynamicResponse { 0.5f };
    std::atomic<bool> membraneEnabled { false };
    std::atomic<float> membraneTension { 0.5f }, membraneStiffness { 0.4f }, membraneDecay { 0.5f }, membraneVelocity { 0.6f };
    float membraneY1[2] = { 0.0f, 0.0f }, membraneY2[2] = { 0.0f, 0.0f };
    float membranePrev[2] = { 0.0f, 0.0f };
    float spectralLow[2] = { 0.0f, 0.0f };
    float spectralPrev[2] = { 0.0f, 0.0f };
    uint32_t noiseState = 0x6d2b79f5u;
    float noiseLowState[2] = { 0.0f, 0.0f };
    juce::CriticalSection sampleLock;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackDrumAudioProcessor)
};