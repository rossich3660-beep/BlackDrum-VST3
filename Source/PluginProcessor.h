#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

class BlackDrumAudioProcessor : public juce::AudioProcessor
{
public:
    BlackDrumAudioProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "blackSnare"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.5; }

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

    void setTopTuning(float v) { topTuning.store(v); }
    float getTopTuning() const { return topTuning.load(); }
    void setTopDamping(float v) { topDamping.store(v); }
    float getTopDamping() const { return topDamping.load(); }
    void setBottomTuning(float v) { bottomTuning.store(v); }
    float getBottomTuning() const { return bottomTuning.load(); }
    void setBottomDamping(float v) { bottomDamping.store(v); }
    float getBottomDamping() const { return bottomDamping.load(); }
    void setShellDiameter(float v) { shellDiameter.store(v); }
    float getShellDiameter() const { return shellDiameter.load(); }
    void setShellDepth(float v) { shellDepth.store(v); }
    float getShellDepth() const { return shellDepth.load(); }
    void setShellMaterial(float v) { shellMaterial.store(v); }
    float getShellMaterial() const { return shellMaterial.load(); }
    void setWireTension(float v) { wireTension.store(v); }
    float getWireTension() const { return wireTension.load(); }
    void setWireAmount(float v) { wireAmount.store(v); }
    float getWireAmount() const { return wireAmount.load(); }
    void setWireDamping(float v) { wireDamping.store(v); }
    float getWireDamping() const { return wireDamping.load(); }
    void setResonance(float v) { resonance.store(v); }
    float getResonance() const { return resonance.load(); }
    void setStickHardness(float v) { stickHardness.store(v); }
    float getStickHardness() const { return stickHardness.load(); }
    void setSampleInfluence(float v) { sampleInfluence.store(v); }
    float getSampleInfluence() const { return sampleInfluence.load(); }
    void setVelocityResponse(float v) { velocityResponse.store(v); }
    float getVelocityResponse() const { return velocityResponse.load(); }
    void setRoom(float v) { room.store(v); }
    float getRoom() const { return room.load(); }
    void setMaster(float v) { master.store(v); }
    float getMaster() const { return master.load(); }
    void setVoiceCount(int v) { voiceCount.store(juce::jlimit(1, 16, v)); }
    int getVoiceCount() const { return voiceCount.load(); }

    juce::ValueTree createPresetState() const;
    bool applyPresetState(const juce::ValueTree&);

private:
    struct Voice
    {
        bool active = false;
        double age = 0.0;
        float velocity = 0.5f;
        float phase = 0.0f;
        float envelope = 0.0f;
        float refPos = 0.0f;
        uint32_t noise = 0x12345678u;
        float topY1 = 0.0f, topY2 = 0.0f;
        float bottomY1 = 0.0f, bottomY2 = 0.0f;
        std::array<float, 8> modeY1 {};
        std::array<float, 8> modeY2 {};
        std::array<float, 4> shellY1 {};
        std::array<float, 4> shellY2 {};
        float wire1 = 0.0f, wire2 = 0.0f, wire3 = 0.0f;
        float exciter = 0.0f;
    };

    void triggerVoice(float velocity);
    float renderVoice(Voice&, int channel);
    float referenceSample(float phase) const;
    float referenceEnvelope(float phase) const;
    void analyzeReference();
    void resetVoices();
    juce::ValueTree makeStateTree() const;
    bool restoreStateTree(const juce::ValueTree&);

    juce::AudioFormatManager formats;
    juce::AudioBuffer<float> sample;
    juce::File loadedFile;
    juce::CriticalSection sampleLock;

    double sourceRate = 44100.0;
    double outputRate = 44100.0;

    std::array<float, 2048> referenceWave {};
    std::array<float, 512> referenceEnv {};
    float referenceRms = 0.25f;
    float referencePeak = 0.9f;
    float referenceLow = 0.5f;
    float referenceMid = 0.4f;
    float referenceHigh = 0.25f;
    float referenceCentroid = 1800.0f;
    float referenceDecay = 0.45f;
    float referenceDuration = 0.25f;

    std::array<Voice, 16> voices {};
    uint64_t voiceCounter = 0;

    std::atomic<int> voiceCount { 8 };
    std::atomic<float> topTuning { 0.50f };
    std::atomic<float> topDamping { 0.34f };
    std::atomic<float> bottomTuning { 0.48f };
    std::atomic<float> bottomDamping { 0.55f };
    std::atomic<float> shellDiameter { 0.50f };
    std::atomic<float> shellDepth { 0.50f };
    std::atomic<float> shellMaterial { 0.58f };
    std::atomic<float> wireTension { 0.62f };
    std::atomic<float> wireAmount { 0.72f };
    std::atomic<float> wireDamping { 0.42f };
    std::atomic<float> resonance { 0.52f };
    std::atomic<float> stickHardness { 0.55f };
    std::atomic<float> sampleInfluence { 0.72f };
    std::atomic<float> velocityResponse { 0.82f };
    std::atomic<float> room { 0.12f };
    std::atomic<float> master { 0.90f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BlackDrumAudioProcessor)
};
