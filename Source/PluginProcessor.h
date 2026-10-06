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
    void setVoiceCount(int value) { voiceCount.store(juce::jlimit(1, 16, value)); }
    int getVoiceCount() const { return voiceCount.load(); }
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
    void setPhaseVocoderMix(float v) { phaseVocoderMix.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getPhaseVocoderMix() const { return phaseVocoderMix.load(); }
    void setCompressorMix(float v) { compressorMix.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getCompressorMix() const { return compressorMix.load(); }
    void setRoomReverbMix(float v) { roomReverbMix.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getRoomReverbMix() const { return roomReverbMix.load(); }
    void setPhysicalSynthMix(float v) { physicalSynthMix.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getPhysicalSynthMix() const { return physicalSynthMix.load(); }
    void setWireCollisionMix(float v) { wireCollisionMix.store(juce::jlimit(0.0f, 1.0f, v)); }
    float getWireCollisionMix() const { return wireCollisionMix.load(); }
    juce::ValueTree createPresetState() const;
    bool applyPresetState(const juce::ValueTree&);

private:
    juce::AudioFormatManager formats;
    juce::AudioBuffer<float> sample;
    juce::File loadedFile;
    struct Voice
    {
        double position = -1.0;
        float velocity = 0.5f;
        uint64_t age = 0;
        float phaseMod0 = 0.0f;
        float phaseMod1 = 0.0f;
        uint32_t noiseSeed = 0x6d2b79f5u;
        float membraneY1[2] = { 0.0f, 0.0f };
        float membraneY2[2] = { 0.0f, 0.0f };
        float membraneUpperY1[2] = { 0.0f, 0.0f };
        float membraneUpperY2[2] = { 0.0f, 0.0f };
        float membranePrev[2] = { 0.0f, 0.0f };
        float noiseLow[2] = { 0.0f, 0.0f };
        float wirePrev[2] = { 0.0f, 0.0f };
        float collisionEnergy[2] = { 0.0f, 0.0f };
        float collisionEnv[2] = { 0.0f, 0.0f };
        float collisionY1[2][2] = {};
        float collisionY2[2][2] = {};
        float wireY1[3][2] = {};
        float wireY2[3][2] = {};

    };
    std::array<Voice, 16> voices{};
    std::atomic<int> voiceCount { 7 };
    uint64_t voiceAge = 0;
    double sourceRate=44100.0, outputRate=44100.0, playbackPosition=-1.0;
    float voiceGain=1.0f, playbackRate=1.0f, filterCoefficient=1.0f;
    float filterState[2]={0.0f,0.0f};
    float bodyExcitation[2]={0.0f,0.0f};
    // A damped, low-level resonator adds a little drum-body energy without replacing the sample.
    float resonatorY1[2]={0.0f,0.0f}, resonatorY2[2]={0.0f,0.0f};
    float resonatorB0=0.0f, resonatorB1=0.0f, resonatorB2=0.0f;
    float resonatorA1=0.0f, resonatorA2=0.0f;
    float hitVelocity=0.5f;
    std::atomic<float> bodyMix { 0.07f };
    std::atomic<float> wireNoiseMix { 0.08f };
    float hitPitchVariation = 1.0f;
    float hitAttackVariation = 1.0f;
    float hitResonanceVariation = 1.0f;
    float hitNoiseVariation = 1.0f;
    std::atomic<float> spectralMix { 0.0f };
    // Phase-vocoder spectral morphing: a deliberately small STFT layer blended
    // with the direct drum signal. It uses fixed-size, allocation-free state.
    std::atomic<float> phaseVocoderMix { 1.0f };
    // Living dynamics: the compressor is intentionally a parallel, velocity-aware
    // layer so it brings up body/tail without flattening the hit transient.
    std::atomic<float> compressorMix { 1.0f };
    std::array<float, 2> compressorEnvelope { 0.0f, 0.0f };
    std::array<float, 2> compressorGain { 1.0f, 1.0f };
    // Small fixed Schroeder-style room: several short feedback delays plus
    // cross-channel diffusion. No allocations or locks occur in processBlock.
    std::atomic<float> roomReverbMix { 1.0f };
    // Hybrid physical/modal synthesis layer. Default is bypassed so the
    // existing BlackDrum sound is unchanged until the user turns it up.
    std::atomic<float> physicalSynthMix { 1.0f };
    std::atomic<float> wireCollisionMix { 0.35f };
    float physicalExciter[2] = { 0.0f, 0.0f };
    float physicalPrev[2] = { 0.0f, 0.0f };
    float physicalMembraneY1[2] = { 0.0f, 0.0f };
    float physicalMembraneY2[2] = { 0.0f, 0.0f };
    float physicalShellY1[3][2] = {};
    float physicalShellY2[3][2] = {};
    static constexpr int roomDelayCount = 4;
    static constexpr int roomMaxDelay = 4096;
    std::array<std::array<float, roomMaxDelay>, 2> roomDelayBuffer {};
    std::array<int, roomDelayCount> roomDelayLengths { 1499, 1877, 2333, 2861 };
    std::array<int, roomDelayCount> roomWritePositions { 0, 0, 0, 0 };
    std::array<float, 2> roomDampingState { 0.0f, 0.0f };
    std::array<float, 2> roomInputState { 0.0f, 0.0f };
    static constexpr int pvFFTSize = 1024;
    static constexpr int pvHopSize = 128;
    static constexpr int pvBins = pvFFTSize / 2 + 1;
    static constexpr int pvRingSize = 4096;
    juce::dsp::FFT phaseVocoderFFT { 10 };
    std::array<float, pvFFTSize * 2> pvFFTBuffer {};
    std::array<float, pvFFTSize> pvInputRing {};
    std::array<float, pvRingSize> pvOutputRing {};
    std::array<float, pvRingSize> pvNormRing {};
    std::array<float, pvBins> pvPreviousPhase {};
    std::array<float, pvBins> pvSynthesisPhase {};
    int pvInputWrite = 0;
    int pvHopCounter = 0;
    uint64_t pvSampleCounter = 0;
    std::atomic<float> transientAmount { 0.0f };
    std::atomic<float> sustainAmount { 0.87f };
    std::atomic<float> dynamicResponse { 0.87f };
    std::atomic<bool> membraneEnabled { false };
    std::atomic<float> membraneTension { 0.47f }, membraneStiffness { 1.0f }, membraneDecay { 0.51f }, membraneVelocity { 1.0f };
    float membraneY1[2] = { 0.0f, 0.0f }, membraneY2[2] = { 0.0f, 0.0f };
    float membraneUpperY1[2] = { 0.0f, 0.0f }, membraneUpperY2[2] = { 0.0f, 0.0f };
    float membranePrev[2] = { 0.0f, 0.0f };
    float spectralLow[2] = { 0.0f, 0.0f };
    float spectralPrev[2] = { 0.0f, 0.0f };
    uint32_t noiseState = 0x6d2b79f5u;
    float noiseLowState[2] = { 0.0f, 0.0f };
    juce::CriticalSection sampleLock;
    juce::ValueTree makeStateTree() const;
    bool restoreStateTree(const juce::ValueTree&);
    void resetPhaseVocoder();
    float processPhaseVocoder(float input, float morphAmount, float velocity);
    float processRoomReverb(float input, int channel, float velocity);
    float processPhysicalSynth(float input, int channel, float velocity);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackDrumAudioProcessor)
};