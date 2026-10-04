#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

BlackDrumAudioProcessor::BlackDrumAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    formats.registerBasicFormats();
}

void BlackDrumAudioProcessor::prepareToPlay(double rate, int)
{
    outputRate = rate > 0.0 ? rate : 44100.0;
    playbackPosition = -1.0;
    filterState[0] = filterState[1] = 0.0f;
    resonatorY1[0] = resonatorY1[1] = resonatorY2[0] = resonatorY2[1] = 0.0f;
    noiseLowState[0] = noiseLowState[1] = 0.0f;
    spectralLow[0] = spectralLow[1] = spectralPrev[0] = spectralPrev[1] = 0.0f;

    // Stable, gently damped resonator centered in the snare's body range.
    const float frequency = 185.0f;
    const float radius = 0.965f;
    const float w = 2.0f * juce::MathConstants<float>::pi * frequency / (float)outputRate;
    resonatorB0 = (1.0f - radius) * 0.5f;
    resonatorB1 = 0.0f;
    resonatorB2 = -resonatorB0;
    resonatorA1 = -2.0f * radius * std::cos(w);
    resonatorA2 = radius * radius;
}

bool BlackDrumAudioProcessor::loadSample(const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(f));
    if (!r || r->lengthInSamples <= 0 || r->lengthInSamples > (juce::int64)r->sampleRate * 120.0)
        return false;
    juce::AudioBuffer<float> temp((int)r->numChannels, (int)r->lengthInSamples);
    r->read(&temp, 0, (int)r->lengthInSamples, 0, true, true);
    const juce::ScopedLock lock(sampleLock);
    sample = std::move(temp);
    sourceRate = r->sampleRate;
    loadedFile = f;
    playbackPosition = -1.0;
    filterState[0] = filterState[1] = 0.0f;
    resonatorY1[0] = resonatorY1[1] = resonatorY2[0] = resonatorY2[1] = 0.0f;
    return true;
}

juce::AudioBuffer<float> BlackDrumAudioProcessor::sampleCopy() const
{
    const juce::ScopedLock lock(sampleLock);
    juce::AudioBuffer<float> c(sample.getNumChannels(), sample.getNumSamples());
    c.makeCopyOf(sample);
    return c;
}

void BlackDrumAudioProcessor::processBlock(juce::AudioBuffer<float>& out, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals no;
    out.clear();
    const juce::ScopedLock lock(sampleLock);
    if (sample.getNumSamples() == 0)
        return;

    auto event = midi.cbegin();
    const auto end = midi.cend();
    const float sr = (float)juce::jmax(1.0, outputRate);

    for (int i = 0; i < out.getNumSamples(); ++i)
    {
        while (event != end && (*event).samplePosition <= i)
        {
            const auto msg = (*event).getMessage();
            if (msg.isNoteOn())
            {
                hitVelocity = juce::jlimit(0.0f, 1.0f, msg.getFloatVelocity());
                const float v = hitVelocity;
                // Small, bounded per-hit variation. The same MIDI velocity remains the
                // main driver; randomness only prevents identical repeated strikes.
                auto nextRandom = [this]() -> float
                {
                    noiseState ^= noiseState << 13; noiseState ^= noiseState >> 17; noiseState ^= noiseState << 5;
                    return (float)(noiseState & 0x00ffffffu) / 8388607.5f - 1.0f;
                };
                hitPitchVariation = 1.0f + nextRandom() * (0.0015f + 0.0025f * v);
                hitAttackVariation = 1.0f + nextRandom() * (0.035f + 0.035f * v);
                hitResonanceVariation = 1.0f + nextRandom() * (0.04f + 0.06f * v);
                hitNoiseVariation = 1.0f + nextRandom() * (0.10f + 0.12f * v);
                voiceGain = 0.16f + 0.84f * v * v;
                // Small pitch variation plus a brighter low-pass response for harder hits.
                playbackRate = (float)(sourceRate / outputRate) * (0.992f + 0.032f * v) * hitPitchVariation;
                const float cutoff = 1800.0f + v * 13800.0f;
                filterCoefficient = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / sr);
                playbackPosition = 0.0;
                filterState[0] = filterState[1] = 0.0f;
                resonatorY1[0] = resonatorY1[1] = resonatorY2[0] = resonatorY2[1] = 0.0f;
                noiseLowState[0] = noiseLowState[1] = 0.0f;
                spectralLow[0] = spectralLow[1] = spectralPrev[0] = spectralPrev[1] = 0.0f;
            }
            ++event;
        }

        if (playbackPosition < 0.0 || playbackPosition >= (double)sample.getNumSamples())
            continue;

        const int idx = (int)playbackPosition;
        const int next = juce::jmin(idx + 1, sample.getNumSamples() - 1);
        const float frac = (float)(playbackPosition - idx);
        const float elapsed = (float)(playbackPosition / juce::jmax(1.0, sourceRate * 0.035));
        const float velocity = hitVelocity;
        // A short velocity-scaled transient lift; decays smoothly over the first ~35 ms.
        const float attackControl = (transientAmount.load() - 0.5f) * 1.4f;
        const float sustainControl = (sustainAmount.load() - 0.5f) * 1.2f;
        const float attackAmount = (0.04f + 0.24f * velocity + attackControl * 0.20f) * hitAttackVariation;
        const float transient = juce::jmax(0.05f, 1.0f + attackAmount * std::exp(-elapsed * 3.2f));
        const float tailShape = juce::jlimit(0.35f, 1.8f, 1.0f + sustainControl * (1.0f - std::exp(-elapsed * 2.5f)));
        // Harder strikes excite slightly more modeled body, kept deliberately subtle.
        const float resonanceMix = juce::jlimit(0.0f, 1.0f, bodyMix.load() * hitResonanceVariation);

        for (int ch = 0; ch < out.getNumChannels(); ++ch)
        {
            const int sc = juce::jmin(ch, sample.getNumChannels() - 1);
            const int fc = juce::jmin(ch, 1);
            const float a = sample.getSample(sc, idx);
            const float b = sample.getSample(sc, next);
            const float raw = a + (b - a) * frac;

            filterState[fc] += filterCoefficient * (raw - filterState[fc]);
            const float bright = filterState[fc];
            const float body = resonatorB0 * raw + resonatorB1 * 0.0f + resonatorB2 * 0.0f
                             - resonatorA1 * resonatorY1[fc] - resonatorA2 * resonatorY2[fc];
            resonatorY2[fc] = resonatorY1[fc];
            resonatorY1[fc] = body;

            const float blended = raw * (1.0f - resonanceMix) + body * resonanceMix;
            const float shaped = std::tanh((blended + (bright - raw) * (0.10f + 0.16f * velocity))
                                           * voiceGain * transient * tailShape * 1.10f);

            // Lightweight spectral resynthesis-inspired layer: split the source into
            // low tonal body, high-frequency residual and transient difference.
            const float specAmount = spectralMix.load();
            spectralLow[fc] += 0.075f * (raw - spectralLow[fc]);
            const float tonal = spectralLow[fc];
            const float residual = raw - tonal;
            const float transientPart = raw - spectralPrev[fc];
            spectralPrev[fc] = raw;
            const float transientEnv = std::exp(-elapsed * 5.0f);
            const float spectralLayer = tonal * (0.92f + 0.24f * velocity)
                + residual * (0.72f + 0.50f * velocity)
                + transientPart * transientEnv * (0.10f + 0.20f * velocity);
            const float spectralOut = shaped * (1.0f - specAmount)
                + std::tanh(spectralLayer * voiceGain * transient * 1.10f) * specAmount;

            // Snare-wire layer: deterministic filtered noise, excited by the hit and
            // shaped by the sample's own decay. No allocation or locking in the audio loop.
            float wire = 0.0f;
            const float wireAmount = wireNoiseMix.load();
            if (wireAmount > 0.0001f)
            {
                noiseState ^= noiseState << 13;
                noiseState ^= noiseState >> 17;
                noiseState ^= noiseState << 5;
                const float white = ((float)(noiseState & 0x00ffffffu) / 8388607.5f) - 1.0f;
                noiseLowState[fc] += 0.22f * (white - noiseLowState[fc]);
                const float bandNoise = white - noiseLowState[fc];
                const float decay = std::exp(-playbackPosition / (float)(sourceRate * (0.10f + 0.12f * velocity)));
                const float excitation = (0.012f + 0.095f * velocity) * decay * wireAmount * hitNoiseVariation;
                wire = bandNoise * excitation;
            }
            out.setSample(ch, i, std::tanh(spectralOut * tailShape + wire));
        }
        playbackPosition += playbackRate;
    }
}

juce::AudioProcessorEditor* BlackDrumAudioProcessor::createEditor()
{
    return new BlackDrumAudioProcessorEditor(*this);
}

void BlackDrumAudioProcessor::getStateInformation(juce::MemoryBlock& d)
{
    juce::MemoryOutputStream s(d, true);
    s.writeString(loadedFile.getFullPathName());
}

void BlackDrumAudioProcessor::setStateInformation(const void* data, int size)
{
    juce::MemoryInputStream s(data, (size_t)size, false);
    auto p = s.readString();
    if (p.isNotEmpty())
        loadSample(juce::File(p));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BlackDrumAudioProcessor();
}