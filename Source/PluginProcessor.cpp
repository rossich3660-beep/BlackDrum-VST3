#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <algorithm>

namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float twoPi = 2.0f * pi;

    static float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, v); }

    static float modeFrequency(float base, int mode, float tune)
    {
        static constexpr float ratios[8] =
            { 1.00f, 1.59f, 2.14f, 2.92f, 3.87f, 4.83f, 6.11f, 7.35f };
        const float r = ratios[juce::jlimit(0, 7, mode)];
        return base * r * (0.88f + 0.24f * tune);
    }

    static float resonator(float input, float frequency, float radius,
                           float& y1, float& y2, double rate)
    {
        const float w = twoPi * frequency / (float) juce::jmax(1.0, rate);
        const float a1 = -2.0f * radius * std::cos(w);
        const float a2 = radius * radius;
        const float b0 = 1.0f - radius;
        const float y = b0 * input - a1 * y1 - a2 * y2;
        y2 = y1;
        y1 = y;
        return y;
    }
}

BlackDrumAudioProcessor::BlackDrumAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formats.registerBasicFormats();
}

void BlackDrumAudioProcessor::prepareToPlay(double rate, int)
{
    outputRate = rate > 0.0 ? rate : 44100.0;
    resetVoices();
}

void BlackDrumAudioProcessor::resetVoices()
{
    for (auto& v : voices)
        v = Voice{};
    voiceCounter = 0;
}

bool BlackDrumAudioProcessor::loadSample(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (reader == nullptr)
        return false;

    juce::AudioBuffer<float> newSample((int) reader->numChannels,
                                       (int) reader->lengthInSamples);
    if (!reader->read(&newSample, 0, (int) reader->lengthInSamples, 0, true, true))
        return false;

    {
        const juce::ScopedLock lock(sampleLock);
        sample = std::move(newSample);
        loadedFile = file;
        sourceRate = reader->sampleRate;
        analyzeReference();
    }

    resetVoices();
    return true;
}

juce::AudioBuffer<float> BlackDrumAudioProcessor::sampleCopy() const
{
    const juce::ScopedLock lock(sampleLock);
    return sample;
}

void BlackDrumAudioProcessor::analyzeReference()
{
    if (sample.getNumSamples() <= 0)
    {
        referenceWave.fill(0.0f);
        referenceEnv.fill(0.0f);
        return;
    }

    const int channels = sample.getNumChannels();
    const int n = sample.getNumSamples();

    double sumSq = 0.0;
    float peak = 0.0f;
    double weightedAbsIndex = 0.0;
    double absTotal = 0.0;
    double lowEnergy = 0.0, midEnergy = 0.0, highEnergy = 0.0;

    const int analysisStep = juce::jmax(1, n / 120000);
    for (int i = 0; i < n; i += analysisStep)
    {
        float x = 0.0f;
        for (int c = 0; c < channels; ++c)
            x += sample.getSample(c, i);
        x /= (float) channels;

        const float a = std::abs(x);
        sumSq += (double) x * x;
        peak = juce::jmax(peak, a);
        absTotal += a;
        weightedAbsIndex += a * (double) i;

        const float norm = (float) i / (float) juce::jmax(1, n - 1);
        if (norm < 0.18f) lowEnergy += (double) x * x;
        else if (norm < 0.55f) midEnergy += (double) x * x;
        else highEnergy += (double) x * x;
    }

    const double count = (double) ((n + analysisStep - 1) / analysisStep);
    referenceRms = std::sqrt((float) (sumSq / juce::jmax(1.0, count)));
    referencePeak = juce::jmax(0.1f, peak);

    const double totalBand = lowEnergy + midEnergy + highEnergy + 1.0e-12;
    referenceLow = (float) (lowEnergy / totalBand);
    referenceMid = (float) (midEnergy / totalBand);
    referenceHigh = (float) (highEnergy / totalBand);

    const double centerIndex = absTotal > 1.0e-9 ? weightedAbsIndex / absTotal : 0.12 * n;
    referenceDuration = juce::jlimit(0.08f, 1.0f,
        (float) ((double) n / juce::jmax(1.0, sourceRate)));
    referenceCentroid = 600.0f + 5200.0f * (float) (1.0 - juce::jlimit(0.0, 1.0, centerIndex / (double) n));

    const int transientSamples = juce::jmin(n, juce::jmax(128, (int) std::round(sourceRate * 0.035)));
    for (size_t i = 0; i < referenceWave.size(); ++i)
    {
        const int pos = (int) ((double) i * (double) juce::jmax(0, transientSamples - 1)
                               / (double) (referenceWave.size() - 1));
        float x = 0.0f;
        for (int c = 0; c < channels; ++c)
            x += sample.getSample(c, pos);
        referenceWave[i] = x / (float) channels / referencePeak;
    }

    for (size_t b = 0; b < referenceEnv.size(); ++b)
    {
        const int start = (int) ((double) b * n / referenceEnv.size());
        const int end = juce::jmax(start + 1, (int) ((double) (b + 1) * n / referenceEnv.size()));
        double e = 0.0;
        int countInBin = 0;
        for (int i = start; i < juce::jmin(end, n); i += juce::jmax(1, (end - start) / 8))
        {
            float x = 0.0f;
            for (int c = 0; c < channels; ++c)
                x += sample.getSample(c, i);
            x /= (float) channels;
            e += (double) x * x;
            ++countInBin;
        }
        referenceEnv[b] = std::sqrt((float) (e / juce::jmax(1, countInBin))) / referencePeak;
    }

    float maxEnv = 0.001f;
    for (auto e : referenceEnv) maxEnv = juce::jmax(maxEnv, e);
    for (auto& e : referenceEnv) e = juce::jlimit(0.0f, 1.0f, e / maxEnv);

    const float early = referenceEnv[juce::jmin<size_t>(20, referenceEnv.size() - 1)];
    referenceDecay = juce::jlimit(0.15f, 0.95f, 0.45f + 0.45f * (1.0f - early));
}

float BlackDrumAudioProcessor::referenceSample(float phase) const
{
    if (sample.getNumSamples() <= 0)
        return 0.0f;

    const float p = juce::jlimit(0.0f, 1.0f, phase);
    const float x = p * (float) (referenceWave.size() - 1);
    const int i = (int) x;
    const float f = x - (float) i;
    const int j = juce::jmin(i + 1, (int) referenceWave.size() - 1);
    return referenceWave[(size_t) i] + f * (referenceWave[(size_t) j] - referenceWave[(size_t) i]);
}

float BlackDrumAudioProcessor::referenceEnvelope(float phase) const
{
    const float p = juce::jlimit(0.0f, 1.0f, phase);
    const float x = p * (float) (referenceEnv.size() - 1);
    const int i = (int) x;
    const float f = x - (float) i;
    const int j = juce::jmin(i + 1, (int) referenceEnv.size() - 1);
    return referenceEnv[(size_t) i] + f * (referenceEnv[(size_t) j] - referenceEnv[(size_t) i]);
}

void BlackDrumAudioProcessor::triggerVoice(float velocity)
{
    Voice* selected = nullptr;
    for (auto& v : voices)
        if (!v.active) { selected = &v; break; }

    if (selected == nullptr)
    {
        selected = &voices[0];
        for (auto& v : voices)
            if (v.age > selected->age) selected = &v;
    }

    *selected = Voice{};
    selected->active = true;
    selected->age = voiceCounter++;
    selected->velocity = clamp01(velocity);
    selected->envelope = 1.0f;
    selected->noise = 0x9e3779b9u ^ (uint32_t) selected->age * 747796405u;
}

float BlackDrumAudioProcessor::renderVoice(Voice& v, int channel)
{
    const float vel = clamp01(v.velocity);
    const float velocityCurve = std::pow(vel, 0.70f + 0.55f * velocityResponse.load());
    const float tune = topTuning.load();
    const float bottomTune = bottomTuning.load();
    const float diameter = shellDiameter.load();
    const float depth = shellDepth.load();
    const float material = shellMaterial.load();
    const float wires = wireAmount.load();
    const float wireT = wireTension.load();
    const float wireD = wireDamping.load();
    const float res = resonance.load();
    const float hardness = stickHardness.load();
    const float refMix = sampleInfluence.load();

    const float ageSeconds = (float) (v.age / juce::jmax(1.0, outputRate));
    const float attackPhase = juce::jlimit(0.0f, 1.0f,
        (float) (v.age / juce::jmax(1.0, outputRate * 0.035)));
    const float referenceHit = referenceSample(attackPhase);
    const float refEnv = referenceEnvelope(
        juce::jlimit(0.0f, 1.0f, ageSeconds / juce::jmax(0.08f, referenceDuration)));

    v.noise ^= v.noise << 13;
    v.noise ^= v.noise >> 17;
    v.noise ^= v.noise << 5;
    const float white = ((float) (v.noise & 0xffffu) / 32767.5f) - 1.0f;

    const float transient = std::tanh(
        (referenceHit * refMix + white * (1.0f - refMix) * 0.34f)
        * (1.1f + 2.4f * velocityCurve) * (0.65f + 0.7f * hardness));

    const float headBase = 155.0f + 155.0f * tune + 0.018f * referenceCentroid;
    const float bottomBase = 190.0f + 145.0f * bottomTune + 0.012f * referenceCentroid;
    const float topRadius = juce::jlimit(0.885f, 0.997f,
        0.982f - 0.070f * topDamping.load() + 0.010f * velocityCurve);
    const float bottomRadius = juce::jlimit(0.875f, 0.997f,
        0.978f - 0.075f * bottomDamping.load());

    const float topExciter = transient * (0.80f + 0.45f * velocityCurve);
    const float bottomExciter = transient * (0.25f + 0.28f * res);

    const float top = resonator(topExciter, headBase, topRadius,
                                v.topY1, v.topY2, outputRate);
    const float bottom = resonator(bottomExciter, bottomBase, bottomRadius,
                                   v.bottomY1, v.bottomY2, outputRate);

    float modal = 0.0f;
    for (int m = 0; m < 8; ++m)
    {
        const float freq = modeFrequency(headBase, m, tune)
            * (1.0f + 0.045f * (material - 0.5f) + 0.03f * velocityCurve);
        const float radius = juce::jlimit(0.88f, 0.998f,
            0.990f - 0.055f * topDamping.load()
            - 0.008f * (float) m - 0.035f * depth);
        const float gain = (m == 0 ? 0.52f : 0.18f / (1.0f + 0.25f * m))
            * (0.7f + 0.6f * velocityCurve);
        modal += resonator(topExciter, freq, radius,
                           v.modeY1[(size_t) m], v.modeY2[(size_t) m], outputRate) * gain;
    }

    float shell = 0.0f;
    static constexpr float shellRatios[4] = { 1.0f, 1.34f, 1.91f, 2.67f };
    for (int m = 0; m < 4; ++m)
    {
        const float base = 95.0f + 105.0f * (1.0f - depth) + 170.0f * diameter;
        const float freq = base * shellRatios[m] * (0.82f + 0.36f * material);
        const float radius = juce::jlimit(0.86f, 0.996f,
            0.965f + 0.018f * material - 0.015f * depth - 0.006f * m);
        const float gain = (0.20f / (1.0f + 0.35f * m)) * (0.7f + 0.6f * res);
        shell += resonator(topExciter * 0.72f, freq, radius,
                           v.shellY1[(size_t) m], v.shellY2[(size_t) m], outputRate) * gain;
    }

    const float wireExciter = 0.55f * bottom + 0.45f * transient
        + white * (0.16f + 0.34f * velocityCurve) * refEnv;

    const float wireFreq = 1850.0f + 3300.0f * wireT
        + 800.0f * referenceHigh;
    const float wireRadius = juce::jlimit(0.72f, 0.995f,
        0.945f + 0.035f * wireT - 0.10f * wireD);
    const float wire1 = resonator(wireExciter, wireFreq, wireRadius,
                                  v.wire1, v.wire2, outputRate);
    const float wire2 = resonator(wireExciter * 0.55f, wireFreq * 1.61f,
                                  juce::jlimit(0.70f, 0.992f, wireRadius - 0.012f),
                                  v.wire3, v.wire2, outputRate);
    const float wire = (wire1 + 0.55f * wire2) * wires
        * (0.45f + 0.75f * velocityCurve);

    const float tonal = top * 1.10f + bottom * 0.55f + modal + shell * (0.8f + 0.5f * res);
    const float noiseLayer = wire + white * (0.045f + 0.08f * referenceHigh) * refEnv;
    const float headBalance = 0.82f + 0.20f * (1.0f - diameter);
    const float roomScale = 1.0f + 0.10f * room.load();

    const float naturalDecay = std::exp(-ageSeconds *
        (1.6f + 4.0f * topDamping.load() + 2.0f * bottomDamping.load()));
    const float referenceTail = 0.55f + 0.45f * refEnv;
    const float level = velocityCurve * naturalDecay * referenceTail * roomScale;

    const float out = std::tanh((tonal * headBalance + noiseLayer * (0.72f + 0.38f * res)) * level);
    ++v.age;

    if (ageSeconds > 2.5f || (std::abs(out) < 1.0e-5f && ageSeconds > 0.35f))
        v.active = false;

    return out;
}

void BlackDrumAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    juce::MidiBuffer::Iterator iterator(midi);
    juce::MidiMessage message;
    int eventPosition = -1;
    int nextEventPosition = 0;
    bool hasEvent = iterator.getNextEvent(message, nextEventPosition);

    for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        while (hasEvent && nextEventPosition <= sampleIndex)
        {
            if (message.isNoteOn())
                triggerVoice(message.getFloatVelocity());
            hasEvent = iterator.getNextEvent(message, nextEventPosition);
        }

        float mono = 0.0f;
        const int count = juce::jlimit(1, 16, voiceCount.load());
        int rendered = 0;
        for (int i = 0; i < count; ++i)
        {
            auto& v = voices[(size_t) i];
            if (v.active)
            {
                mono += renderVoice(v, 0);
                ++rendered;
            }
        }

        if (rendered > 1)
            mono /= std::sqrt((float) rendered);

        mono *= master.load();
        mono = juce::jlimit(-0.98f, 0.98f, mono);

        if (buffer.getNumChannels() > 0)
            buffer.setSample(0, sampleIndex, mono);
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, sampleIndex, mono * (0.98f + 0.02f * room.load()));

        (void) eventPosition;
    }

    midi.clear();
}

juce::ValueTree BlackDrumAudioProcessor::makeStateTree() const
{
    juce::ValueTree s("BlackSnareState");
    s.setProperty("version", 1, nullptr);
    s.setProperty("samplePath", loadedFile.getFullPathName(), nullptr);
    s.setProperty("topTuning", topTuning.load(), nullptr);
    s.setProperty("topDamping", topDamping.load(), nullptr);
    s.setProperty("bottomTuning", bottomTuning.load(), nullptr);
    s.setProperty("bottomDamping", bottomDamping.load(), nullptr);
    s.setProperty("shellDiameter", shellDiameter.load(), nullptr);
    s.setProperty("shellDepth", shellDepth.load(), nullptr);
    s.setProperty("shellMaterial", shellMaterial.load(), nullptr);
    s.setProperty("wireTension", wireTension.load(), nullptr);
    s.setProperty("wireAmount", wireAmount.load(), nullptr);
    s.setProperty("wireDamping", wireDamping.load(), nullptr);
    s.setProperty("resonance", resonance.load(), nullptr);
    s.setProperty("stickHardness", stickHardness.load(), nullptr);
    s.setProperty("sampleInfluence", sampleInfluence.load(), nullptr);
    s.setProperty("velocityResponse", velocityResponse.load(), nullptr);
    s.setProperty("room", room.load(), nullptr);
    s.setProperty("master", master.load(), nullptr);
    s.setProperty("voiceCount", voiceCount.load(), nullptr);
    return s;
}

juce::ValueTree BlackDrumAudioProcessor::createPresetState() const
{
    return makeStateTree();
}

bool BlackDrumAudioProcessor::restoreStateTree(const juce::ValueTree& s)
{
    if (!s.isValid() || !s.hasType("BlackSnareState"))
        return false;

    auto getF = [&s](const char* key, float fallback)
    {
        return (float) s.getProperty(key, fallback);
    };

    setTopTuning(getF("topTuning", 0.50f));
    setTopDamping(getF("topDamping", 0.34f));
    setBottomTuning(getF("bottomTuning", 0.48f));
    setBottomDamping(getF("bottomDamping", 0.55f));
    setShellDiameter(getF("shellDiameter", 0.50f));
    setShellDepth(getF("shellDepth", 0.50f));
    setShellMaterial(getF("shellMaterial", 0.58f));
    setWireTension(getF("wireTension", 0.62f));
    setWireAmount(getF("wireAmount", 0.72f));
    setWireDamping(getF("wireDamping", 0.42f));
    setResonance(getF("resonance", 0.52f));
    setStickHardness(getF("stickHardness", 0.55f));
    setSampleInfluence(getF("sampleInfluence", 0.72f));
    setVelocityResponse(getF("velocityResponse", 0.82f));
    setRoom(getF("room", 0.12f));
    setMaster(getF("master", 0.90f));
    setVoiceCount((int) s.getProperty("voiceCount", 8));

    const juce::String path = s.getProperty("samplePath", "").toString();
    if (path.isNotEmpty())
        loadSample(juce::File(path));

    return true;
}

bool BlackDrumAudioProcessor::applyPresetState(const juce::ValueTree& state)
{
    return restoreStateTree(state);
}

void BlackDrumAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = makeStateTree().createXml())
        copyXmlToBinary(*xml, destData);
}

void BlackDrumAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        restoreStateTree(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* BlackDrumAudioProcessor::createEditor()
{
    return new BlackDrumAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BlackDrumAudioProcessor();
}
