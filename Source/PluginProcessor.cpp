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
    bodyExcitation[0] = bodyExcitation[1] = 0.0f;
    resonatorY1[0] = resonatorY1[1] = resonatorY2[0] = resonatorY2[1] = 0.0f;
    noiseLowState[0] = noiseLowState[1] = 0.0f;
    spectralLow[0] = spectralLow[1] = spectralPrev[0] = spectralPrev[1] = 0.0f;
    membraneY1[0] = membraneY1[1] = membraneY2[0] = membraneY2[1] = membraneUpperY1[0] = membraneUpperY1[1] = membraneUpperY2[0] = membraneUpperY2[1] = membranePrev[0] = membranePrev[1] = 0.0f;
    resetPhaseVocoder();
    compressorEnvelope[0] = compressorEnvelope[1] = 0.0f;
    compressorGain[0] = compressorGain[1] = 1.0f;
    for (auto& ch : roomDelayBuffer) ch.fill(0.0f);
    roomWritePositions.fill(0);
    roomDampingState[0] = roomDampingState[1] = 0.0f;
    roomInputState[0] = roomInputState[1] = 0.0f;
    physicalExciter[0] = physicalExciter[1] = 0.0f;
    physicalPrev[0] = physicalPrev[1] = 0.0f;
    physicalMembraneY1[0] = physicalMembraneY1[1] = physicalMembraneY2[0] = physicalMembraneY2[1] = 0.0f;
    for (auto& mode : physicalShellY1) for (auto& value : mode) value = 0.0f;
    for (auto& mode : physicalShellY2) for (auto& value : mode) value = 0.0f;

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

void BlackDrumAudioProcessor::resetPhaseVocoder()
{
    pvFFTBuffer.fill(0.0f);
    pvInputRing.fill(0.0f);
    pvOutputRing.fill(0.0f);
    pvNormRing.fill(0.0f);
    pvPreviousPhase.fill(0.0f);
    pvSynthesisPhase.fill(0.0f);
    pvInputWrite = 0;
    pvHopCounter = 0;
    pvSampleCounter = 0;
    pvPreviousFrameEnergy = 0.0f;
}

float BlackDrumAudioProcessor::processPhaseVocoder(float input, float morphAmount, float velocity)
{
    if (morphAmount <= 0.0001f)
        return 0.0f;

    constexpr float twoPi = 2.0f * juce::MathConstants<float>::pi;
    constexpr float invFFT = 1.0f / (float) pvFFTSize;
    constexpr float invBins = 1.0f / (float) (pvFFTSize / 2);

    pvInputRing[(size_t) pvInputWrite] = input;
    pvInputWrite = (pvInputWrite + 1) % pvFFTSize;
    ++pvHopCounter;
    ++pvSampleCounter;

    if (pvHopCounter >= pvHopSize && pvSampleCounter >= (uint64_t) pvFFTSize)
    {
        pvHopCounter = 0;

        // Analyze the most recent 1024 samples. The Hann window keeps the
        // overlap-add reconstruction stable while the phase accumulator keeps
        // neighboring frames phase-coherent.
        for (int n = 0; n < pvFFTSize; ++n)
        {
            const int ringIndex = (pvInputWrite + n) % pvFFTSize;
            const float window = 0.5f - 0.5f * std::cos(twoPi * (float) n / (float) pvFFTSize);
            pvFFTBuffer[(size_t) n] = pvInputRing[(size_t) ringIndex] * window;
            pvFFTBuffer[(size_t) (n + pvFFTSize)] = 0.0f;
        }

        // Detect a rising frame energy and temporarily reduce spectral reshaping.
        // This protects the snare attack while leaving the body/tail free to morph.
        float frameEnergy = 0.0f;
        for (int n = 0; n < pvFFTSize; ++n)
            frameEnergy += pvFFTBuffer[(size_t) n] * pvFFTBuffer[(size_t) n];
        frameEnergy = std::sqrt(frameEnergy / (float) pvFFTSize);
        const float energyRise = frameEnergy - pvPreviousFrameEnergy;
        const float transient = juce::jlimit(0.0f, 1.0f,
            energyRise / (0.015f + 0.35f * frameEnergy + 1.0e-6f));
        pvPreviousFrameEnergy += 0.20f * (frameEnergy - pvPreviousFrameEnergy);
        const float safeMorphAmount = morphAmount
            * (0.35f + 0.65f * (1.0f - transient));

        phaseVocoderFFT.performRealOnlyForwardTransform(pvFFTBuffer.data());

        for (int k = 0; k < pvBins; ++k)
        {
            float real = 0.0f;
            float imag = 0.0f;
            if (k == 0)
            {
                real = pvFFTBuffer[0];
            }
            else if (k == pvFFTSize / 2)
            {
                real = pvFFTBuffer[1];
            }
            else
            {
                real = pvFFTBuffer[(size_t) (2 * k)];
                imag = pvFFTBuffer[(size_t) (2 * k + 1)];
            }

            const float magnitude = std::sqrt(real * real + imag * imag) + 1.0e-9f;
            const float phase = std::atan2(imag, real);
            const float expectedAdvance = twoPi * (float) k * (float) pvHopSize * invFFT;
            float delta = phase - pvPreviousPhase[(size_t) k] - expectedAdvance;

            while (delta > juce::MathConstants<float>::pi)
                delta -= twoPi;
            while (delta < -juce::MathConstants<float>::pi)
                delta += twoPi;

            pvPreviousPhase[(size_t) k] = phase;
            const float trueAdvance = expectedAdvance + delta;
            pvSynthesisPhase[(size_t) k] += trueAdvance;

            const float normalizedFrequency = (float) k * invBins;
            // Morph the spectral envelope rather than simply changing volume:
            // stronger hits push energy toward the upper partials, while the
            // control amount determines how far the spectrum moves from the
            // original sample toward that velocity-shaped target.
            const float highLift = std::pow(normalizedFrequency, 1.35f) * (0.85f + 0.75f * velocity);
            const float lowTrim = (1.0f - normalizedFrequency) * (0.30f + 0.20f * (1.0f - velocity));
            const float targetGain = juce::jlimit(0.25f, 2.5f, 1.0f + highLift - lowTrim);
            const float morphedMagnitude = magnitude * ((1.0f - safeMorphAmount) + safeMorphAmount * targetGain);

            const float outReal = morphedMagnitude * std::cos(pvSynthesisPhase[(size_t) k]);
            const float outImag = morphedMagnitude * std::sin(pvSynthesisPhase[(size_t) k]);
            if (k == 0)
                pvFFTBuffer[0] = outReal;
            else if (k == pvFFTSize / 2)
                pvFFTBuffer[1] = outReal;
            else
            {
                pvFFTBuffer[(size_t) (2 * k)] = outReal;
                pvFFTBuffer[(size_t) (2 * k + 1)] = outImag;
            }
        }

        phaseVocoderFFT.performRealOnlyInverseTransform(pvFFTBuffer.data());

        // The frame is scheduled into a delayed output ring. This gives the
        // phase-vocoder layer enough look-back for analysis without delaying
        // the dry/transient path of the drum.
        const uint64_t frameStart = pvSampleCounter - (uint64_t) pvFFTSize + 1u;
        for (int n = 0; n < pvFFTSize; ++n)
        {
            const float window = 0.5f - 0.5f * std::cos(twoPi * (float) n / (float) pvFFTSize);
            const uint64_t absolute = frameStart + (uint64_t) n;
            const size_t index = (size_t) (absolute % (uint64_t) pvRingSize);
            pvOutputRing[index] += pvFFTBuffer[(size_t) n] * invFFT * window;
            pvNormRing[index] += window * window;
        }
    }

    if (pvSampleCounter <= (uint64_t) pvFFTSize)
        return 0.0f;

    const size_t readIndex = (size_t) ((pvSampleCounter - (uint64_t) pvFFTSize + 1u) % (uint64_t) pvRingSize);
    const float norm = pvNormRing[readIndex];
    const float output = norm > 1.0e-6f ? pvOutputRing[readIndex] / norm : 0.0f;
    pvOutputRing[readIndex] = 0.0f;
    pvNormRing[readIndex] = 0.0f;
    return output;
}

float BlackDrumAudioProcessor::processRoomReverb(float input, int channel, float velocity)
{
    const float mix = roomReverbMix.load();
    if (mix <= 0.0001f)
        return 0.0f;

    const int ch = juce::jlimit(0, 1, channel);
    float wet = 0.0f;

    // The previous implementation injected the same feedback state into all
    // four delay lines. That can become effectively regenerative at high mix.
    // Keep the room strictly decaying: feedback is capped well below unity and
    // is applied only to the delayed sample, never to the current input.
    const float feedback = juce::jlimit(0.0f, 0.68f, 0.52f + 0.12f * mix + 0.04f * velocity);
    const float damping = 0.22f + 0.20f * (1.0f - mix);
    const float inputSlew = 0.035f + 0.025f * mix;

    // Smooth the excitation so changing the knob cannot inject a discontinuity.
    roomInputState[(size_t)ch] += inputSlew * (input - roomInputState[(size_t)ch]);
    const float excitation = juce::jlimit(-1.0f, 1.0f, roomInputState[(size_t)ch]);

    for (int m = 0; m < roomDelayCount; ++m)
    {
        int& wp = roomWritePositions[(size_t)m];
        const int delay = roomDelayLengths[(size_t)m];
        const int readPos = (wp - delay + roomMaxDelay) % roomMaxDelay;
        const float delayed = roomDelayBuffer[(size_t)ch][(size_t)readPos];

        // One-pole damping in the feedback path guarantees progressive energy loss.
        roomDampingState[(size_t)ch] += damping * (delayed - roomDampingState[(size_t)ch]);
        const float reflected = roomDampingState[(size_t)ch];
        roomDelayBuffer[(size_t)ch][(size_t)wp] =
            excitation * 0.72f + reflected * feedback;

        wet += reflected * (0.16f + 0.035f * (float)m);
        wp = (wp + 1) % roomMaxDelay;
    }

    // Gentle velocity shaping: harder hits feel closer without creating a long tail.
    const float roomShape = 0.55f + 0.45f * velocity;
    const float safety = 0.72f - 0.20f * mix;
    return juce::jlimit(-0.35f, 0.35f, wet * mix * roomShape * safety);
}


float BlackDrumAudioProcessor::processPhysicalSynth(float input, int channel, float velocity)
{
    const float mix = physicalSynthMix.load();
    if (mix <= 0.0001f) return 0.0f;
    const int ch = juce::jlimit(0, 1, channel);
    const float v = juce::jlimit(0.0f, 1.0f, velocity);

    const float edge = input - physicalPrev[ch];
    physicalPrev[ch] = input;
    physicalExciter[ch] += (0.075f + 0.055f * v) * (edge - physicalExciter[ch]);
    const float exciter = juce::jlimit(-1.0f, 1.0f, physicalExciter[ch] + 0.035f * input);

    const float membraneFreq = 155.0f + 105.0f * std::sqrt(v);
    const float membraneRadius = 0.965f + 0.014f * v;
    const float mw = 2.0f * juce::MathConstants<float>::pi * membraneFreq
                   / (float)juce::jmax(1.0, outputRate);
    const float mb = 1.0f - membraneRadius;
    const float ma1 = -2.0f * membraneRadius * std::cos(mw);
    const float ma2 = membraneRadius * membraneRadius;
    const float nonlinearExciter = std::tanh(exciter * (1.0f + 2.2f * v));
    const float membrane = mb * nonlinearExciter
                         - ma1 * physicalMembraneY1[ch]
                         - ma2 * physicalMembraneY2[ch];
    physicalMembraneY2[ch] = physicalMembraneY1[ch];
    physicalMembraneY1[ch] = membrane;

    constexpr float shellFreq[3] = { 120.0f, 225.0f, 405.0f };
    constexpr float shellGain[3] = { 0.20f, 0.13f, 0.075f };
    float shell = 0.0f;
    for (int mode = 0; mode < 3; ++mode)
    {
        const float frequency = shellFreq[mode] * (1.0f + 0.045f * v);
        const float radius = 0.962f - 0.006f * (float)mode + 0.008f * v;
        const float w = 2.0f * juce::MathConstants<float>::pi * frequency
                      / (float)juce::jmax(1.0, outputRate);
        const float b0 = 1.0f - radius;
        const float a1 = -2.0f * radius * std::cos(w);
        const float a2 = radius * radius;
        const float y = b0 * exciter - a1 * physicalShellY1[mode][ch]
                      - a2 * physicalShellY2[mode][ch];
        physicalShellY2[mode][ch] = physicalShellY1[mode][ch];
        physicalShellY1[mode][ch] = y;
        shell += y * shellGain[mode];
    }

    const float brightness = 0.72f + 0.58f * v;
    const float physical = std::tanh((membrane * 0.52f + shell) * brightness);
    return juce::jlimit(-0.30f, 0.30f, physical * mix * (0.72f + 0.55f * v));
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
    bodyExcitation[0] = bodyExcitation[1] = 0.0f;
    resetPhaseVocoder();
    compressorEnvelope[0] = compressorEnvelope[1] = 0.0f;
    compressorGain[0] = compressorGain[1] = 1.0f;
    for (auto& ch : roomDelayBuffer) ch.fill(0.0f);
    roomWritePositions.fill(0);
    roomDampingState[0] = roomDampingState[1] = 0.0f;
    physicalExciter[0] = physicalExciter[1] = 0.0f;
    physicalPrev[0] = physicalPrev[1] = 0.0f;
    physicalMembraneY1[0] = physicalMembraneY1[1] = physicalMembraneY2[0] = physicalMembraneY2[1] = 0.0f;
    for (auto& mode : physicalShellY1) for (auto& value : mode) value = 0.0f;
    for (auto& mode : physicalShellY2) for (auto& value : mode) value = 0.0f;
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
                const float response = dynamicResponse.load();
                const float exponent = 1.8f - 1.25f * response;
                const float shapedVelocity = std::pow(v, exponent);
                voiceGain = 0.12f + 0.88f * shapedVelocity * shapedVelocity;
                // Small pitch variation plus a brighter low-pass response for harder hits.
                playbackRate = (float)(sourceRate / outputRate) * (0.992f + 0.032f * std::pow(v, 1.8f - 1.25f * dynamicResponse.load())) * hitPitchVariation;
                const float cutoff = 1400.0f + std::pow(v, 1.8f - 1.25f * dynamicResponse.load()) * 14400.0f;
                filterCoefficient = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / sr);
                // Allocate a voice within the configured limit. Prefer an idle slot;
                // otherwise steal the oldest active voice, with no audio-thread allocation.
                const int limit = juce::jlimit(1, 16, voiceCount.load());
                int slot = -1;
                for (int vi = 0; vi < limit; ++vi)
                    if (voices[(size_t)vi].position < 0.0 || voices[(size_t)vi].position >= sample.getNumSamples())
                    { slot = vi; break; }
                if (slot < 0)
                {
                    slot = 0;
                    for (int vi = 1; vi < limit; ++vi)
                        if (voices[(size_t)vi].age < voices[(size_t)slot].age) slot = vi;
                }
                voices[(size_t)slot].position = 0.0;
                voices[(size_t)slot].velocity = hitVelocity;
                // Every hit receives its own deterministic micro-variation.  The
                // variation is intentionally small so the instrument remains musical.
                auto nextHitRandom = [this]() -> float
                {
                    noiseState ^= noiseState << 13;
                    noiseState ^= noiseState >> 17;
                    noiseState ^= noiseState << 5;
                    return (float)(noiseState & 0x00ffffffu) / 8388607.5f - 1.0f;
                };
                auto& newVoice = voices[(size_t)slot];
                newVoice.phaseMod0 = nextHitRandom() * juce::MathConstants<float>::pi;
                newVoice.phaseMod1 = nextHitRandom() * juce::MathConstants<float>::pi;
                newVoice.noiseSeed = noiseState ^ (uint32_t)(++voiceAge * 747796405u);
                for (int vc = 0; vc < 2; ++vc)
                {
                    newVoice.membraneY1[vc] = std::sin(newVoice.phaseMod0) * 0.0025f;
                    newVoice.membraneY2[vc] = std::sin(newVoice.phaseMod0 - 0.13f) * 0.0025f;
                    newVoice.membraneUpperY1[vc] = std::sin(newVoice.phaseMod1) * 0.0015f;
                    newVoice.membraneUpperY2[vc] = std::sin(newVoice.phaseMod1 - 0.17f) * 0.0015f;
                    newVoice.membranePrev[vc] = 0.0f;
                    newVoice.noiseLow[vc] = 0.0f;
                    newVoice.wirePrev[vc] = 0.0f;
                    newVoice.collisionEnergy[vc] = 0.0f;
                    newVoice.collisionEnv[vc] = 0.0f;
                    for (int mode = 0; mode < 2; ++mode)
                    {
                        newVoice.collisionY1[mode][vc] = 0.0f;
                        newVoice.collisionY2[mode][vc] = 0.0f;
                    }
                    for (int wire = 0; wire < Voice::snareStringCount; ++wire)
                    {
                        newVoice.snareStringDisplacement[wire][vc] = 0.0f;
                        newVoice.snareStringVelocity[wire][vc] = 0.0f;
                        newVoice.snareStringEnergy[wire][vc] = 0.0f;
                        newVoice.snareStringGate[wire][vc] = 0.0f;
                        newVoice.snareStringPrevMembrane[wire][vc] = 0.0f;
                        newVoice.snareStringPending[wire][vc] = 0.0f;
                        newVoice.snareStringDelay[wire][vc] = 0;
                        newVoice.snareStringNoise[wire][vc] = 0.0f;
                    }
                    for (int mode = 0; mode < 3; ++mode)
                    {
                        newVoice.wireY1[mode][vc] = 0.0f;
                        newVoice.wireY2[mode][vc] = 0.0f;
                    }
                }
                newVoice.age = voiceAge;
                playbackPosition = 0.0;
                filterState[0] = filterState[1] = 0.0f;
                resonatorY1[0] = resonatorY1[1] = resonatorY2[0] = resonatorY2[1] = 0.0f;
                noiseLowState[0] = noiseLowState[1] = 0.0f;
                spectralLow[0] = spectralLow[1] = spectralPrev[0] = spectralPrev[1] = 0.0f;
                membraneY1[0] = membraneY1[1] = membraneY2[0] = membraneY2[1] = membranePrev[0] = membranePrev[1] = 0.0f;
            }
            ++event;
        }

        bool anyActiveVoice = false;
        for (int vi = 0; vi < juce::jlimit(1, 16, voiceCount.load()); ++vi)
            if (voices[(size_t)vi].position >= 0.0 && voices[(size_t)vi].position < sample.getNumSamples())
            { anyActiveVoice = true; break; }
        if (!anyActiveVoice) continue;

        const int idx = (int)playbackPosition;
        const int next = juce::jmin(idx + 1, sample.getNumSamples() - 1);
        const float frac = (float)(playbackPosition - idx);
        const float elapsed = (float)(playbackPosition / juce::jmax(1.0, sourceRate * 0.035));
        const float velocity = std::pow(hitVelocity, 1.8f - 1.25f * dynamicResponse.load());
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
            float raw = 0.0f;
            int activeCount = 0;
            const int limit = juce::jlimit(1, 16, voiceCount.load());
            for (int vi = 0; vi < limit; ++vi)
            {
                const double pos = voices[(size_t)vi].position;
                if (pos < 0.0 || pos >= sample.getNumSamples()) continue;
                const int voiceIdx = (int)pos;
                const int voiceNext = juce::jmin(voiceIdx + 1, sample.getNumSamples() - 1);
                const float voiceFrac = (float)(pos - voiceIdx);
                const float a = sample.getSample(sc, voiceIdx);
                const float b = sample.getSample(sc, voiceNext);
                raw += a + (b - a) * voiceFrac;
                ++activeCount;
            }
            if (activeCount > 1) raw *= 1.0f / std::sqrt((float)activeCount);

            filterState[fc] += filterCoefficient * (raw - filterState[fc]);
            const float bright = filterState[fc];
            // Body resonance must be driven by a smoothed excitation, not the
            // raw sample edge. Driving the resonator directly from sharp sample
            // discontinuities was creating random-sounding short transients.
            const float bodyDriveCoeff = 0.010f + 0.018f * velocity;
            bodyExcitation[fc] += bodyDriveCoeff * (raw - bodyExcitation[fc]);
            const float bodyDrive = bodyExcitation[fc];
            const float body = resonatorB0 * bodyDrive
                             - resonatorA1 * resonatorY1[fc] - resonatorA2 * resonatorY2[fc];
            resonatorY2[fc] = resonatorY1[fc];
            resonatorY1[fc] = body;

            // Keep the original sample as the main signal. Body resonance is an
            // additive, low-level component rather than a crossfade replacement.
            const float bodyLevel = 0.22f + 0.18f * velocity;
            const float blended = raw + body * resonanceMix * bodyLevel;
            const float physicalLayer = processPhysicalSynth(blended, fc, velocity);
            const float shaped = std::tanh((blended + (bright - raw) * (0.10f + 0.16f * velocity) + physicalLayer)
                                           * voiceGain * transient * tailShape * 1.10f);

            // Lightweight spectral resynthesis-inspired layer: split the source into
            // low tonal body, high-frequency residual and transient difference.
            const float specAmount = 0.0f;
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

            // Each active voice gets its own noise stream and phase. This prevents
            // repeated MIDI hits from sharing an identical noise waveform.
            float wire = 0.0f;
            float wireCollision = 0.0f;
            const float wireAmount = wireNoiseMix.load();
            const float collisionAmount = wireCollisionMix.load();
            const int voiceLimitForLayers = juce::jlimit(1, 16, voiceCount.load());
            if (wireAmount > 0.0001f || collisionAmount > 0.0001f)
            {
                // Three short resonant wire modes. Each voice has independent
                // state, so repeated hits do not share an identical wire tail.
                const float wireFreqs[3] = { 1250.0f, 1900.0f, 2650.0f };
                const float wireRadii[3] = { 0.925f, 0.905f, 0.875f };

                for (int vi = 0; vi < voiceLimitForLayers; ++vi)
                {
                    auto& vce = voices[(size_t)vi];
                    if (vce.position < 0.0 || vce.position >= sample.getNumSamples())
                        continue;

                    vce.noiseSeed ^= vce.noiseSeed << 13;
                    vce.noiseSeed ^= vce.noiseSeed >> 17;
                    vce.noiseSeed ^= vce.noiseSeed << 5;
                    const float white = ((float)(vce.noiseSeed & 0x00ffffffu) / 8388607.5f) - 1.0f;
                    vce.noiseLow[fc] += 0.30f * (white - vce.noiseLow[fc]);
                    const float bandNoise = white - vce.noiseLow[fc];

                    const int vi0 = (int)vce.position;
                    const int vi1 = juce::jmin(vi0 + 1, sample.getNumSamples() - 1);
                    const float vf = (float)(vce.position - vi0);
                    const float voiceRaw = sample.getSample(sc, vi0)
                        + (sample.getSample(sc, vi1) - sample.getSample(sc, vi0)) * vf;
                    const float impact = voiceRaw - vce.wirePrev[fc];
                    vce.wirePrev[fc] = voiceRaw;

                    const float decay = std::exp(-(float)vce.position
                        / (float)(sourceRate * (0.075f + 0.10f * vce.velocity)));
                    const float excitation = (0.010f + 0.090f * std::pow(vce.velocity, 1.15f))
                        * decay * wireAmount * hitNoiseVariation;
                    float voiceWire = bandNoise * excitation;

                    for (int mode = 0; mode < 3; ++mode)
                    {
                        const float freq = juce::jmin(
                            wireFreqs[mode] * (0.96f + 0.08f * vce.velocity), 0.40f * sr);
                        const float w = 2.0f * juce::MathConstants<float>::pi * freq / sr;
                        const float r = juce::jlimit(0.80f, 0.97f,
                            wireRadii[mode] + 0.018f * vce.velocity);
                        const float drive = std::tanh(
                            impact * (0.35f + 1.15f * vce.velocity)
                            * (1.0f + 0.5f * hitResonanceVariation));
                        const float y = drive * (0.010f + 0.018f * vce.velocity)
                            + 2.0f * r * std::cos(w) * vce.wireY1[mode][fc]
                            - r * r * vce.wireY2[mode][fc];
                        vce.wireY2[mode][fc] = vce.wireY1[mode][fc];
                        vce.wireY1[mode][fc] = juce::jlimit(-1.5f, 1.5f, y);
                        voiceWire += vce.wireY1[mode][fc]
                            * (0.012f + 0.022f * vce.velocity)
                            * std::exp(-(float)vce.position
                                / (float)(sourceRate * (0.11f + 0.10f * vce.velocity)));
                    }
                    wire += voiceWire;

                    // Full virtual snare-bed model:
                    // membrane motion -> 18 individual wire masses -> thresholded
                    // collisions -> collision energy -> noise gating. The wires are
                    // deliberately lightweight and dissipative rather than a free-running
                    // feedback network, so quiet ghost notes can excite them without
                    // risking runaway energy.
                    if (collisionAmount > 0.0001f)
                    {
                        const float membraneProxy = 0.78f * bright + 0.22f * voiceRaw;
                        float stringGateSum = 0.0f;
                        float stringCollision = 0.0f;

                        for (int wireIndex = 0; wireIndex < Voice::snareStringCount; ++wireIndex)
                        {
                            const float p = ((float)wireIndex + 0.5f)
                                / (float)Voice::snareStringCount;
                            // Position-dependent coupling approximates the fact that
                            // different wires sample different membrane modes.
                            const float spatial = 0.55f
                                + 0.45f * std::sin(juce::MathConstants<float>::pi * p);
                            // Deterministic per-wire manufacturing/tension variation.
                            const float variation = 0.78f
                                + 0.44f * (float)((wireIndex * 37 + 11) % 101) / 100.0f;
                            const float sensitivity = variation * (0.82f + 0.36f * vce.velocity);
                            const float threshold = (0.00045f
                                + 0.00175f * (1.0f - vce.velocity))
                                * (1.28f - 0.34f * sensitivity);

                            float& displacement = vce.snareStringDisplacement[wireIndex][fc];
                            float& stringVelocity = vce.snareStringVelocity[wireIndex][fc];
                            float& energy = vce.snareStringEnergy[wireIndex][fc];
                            float& gate = vce.snareStringGate[wireIndex][fc];
                            float& pending = vce.snareStringPending[wireIndex][fc];
                            int& delay = vce.snareStringDelay[wireIndex][fc];
                            float& noiseState = vce.snareStringNoise[wireIndex][fc];

                            // Each wire sees a slightly different local membrane motion.
                            const float phaseOffset = vce.phaseMod0
                                + (float)wireIndex * 0.371f
                                + (float)vce.position * (0.00007f + 0.000015f * p);
                            const float localMembrane = membraneProxy
                                * (spatial * (0.82f + 0.18f * std::sin(phaseOffset)));
                            const float membraneVelocity = localMembrane
                                - vce.snareStringPrevMembrane[wireIndex][fc];
                            vce.snareStringPrevMembrane[wireIndex][fc] = localMembrane;

                            // A short spring/mass approximation. The damping is deliberately
                            // strong enough to guarantee that energy decays between contacts.
                            const float stiffness = 0.018f
                                + 0.014f * sensitivity
                                + 0.010f * vce.velocity;
                            const float damping = 0.070f
                                + 0.050f * (1.0f - sensitivity * 0.35f)
                                + 0.025f * (1.0f - vce.velocity);
                            const float coupling = (0.030f + 0.055f * vce.velocity)
                                * sensitivity * spatial;

                            const float relativeMotion = localMembrane - displacement;
                            stringVelocity += coupling * relativeMotion;
                            stringVelocity *= (1.0f - damping);
                            displacement += stringVelocity;
                            displacement = juce::jlimit(-0.045f, 0.045f, displacement);

                            const float relativeVelocity = membraneVelocity * spatial
                                - stringVelocity;
                            const float penetration = std::abs(displacement) - threshold;

                            float collisionKick = 0.0f;

                            // Contact is one-sided: a wire only "strikes" when it has
                            // enough displacement and relative velocity. A per-wire
                            // pseudo-random threshold makes the 18 contacts decorrelate.
                            if (penetration > 0.0f
                                && std::abs(relativeVelocity) > (0.00018f + 0.00032f * (1.0f - vce.velocity)))
                            {
                                const float collisionVelocity = juce::jlimit(0.0f, 1.0f,
                                    std::abs(relativeVelocity) * (9.0f + 7.0f * vce.velocity)
                                    * sensitivity);
                                const float impulse = juce::jlimit(0.0f, 0.012f,
                                    penetration * (0.16f + 0.24f * vce.velocity)
                                    + collisionVelocity * (0.00045f + 0.0009f * vce.velocity));
                                collisionKick = impulse;

                                // 0..~2 ms delay per wire. This prevents all wires from
                                // firing on the same sample and creates a real wire-bed
                                // spread in time.
                                const int wireDelay = (wireIndex * 7 + (int)(std::abs(phaseOffset) * 5.0f))
                                    % juce::jmax(1, Voice::snareStringMaxDelay);
                                pending = juce::jlimit(0.0f, 0.035f, pending + impulse);
                                delay = juce::jmax(delay, wireDelay);

                                // Bounce with energy loss: no regenerative feedback.
                                stringVelocity *= -(0.68f + 0.12f * vce.velocity);
                                displacement *= 0.72f;
                            }

                            if (delay > 0)
                            {
                                --delay;
                            }
                            else if (pending > 0.0f)
                            {
                                energy += pending * (0.34f + 0.34f * vce.velocity);
                                pending = 0.0f;
                            }

                            // Stable collision-energy envelope.  Do not feed the
                            // current energy back into itself: that turns the wire bed
                            // into a regenerative feedback loop when the knob is raised.
                            const float collisionInput = juce::jlimit(0.0f, 1.0f,
                                collisionKick * (24.0f + 14.0f * sensitivity));
                            const float energyAttack = 0.18f + 0.08f * vce.velocity;
                            const float energyRelease = 0.010f + 0.006f * (1.0f - vce.velocity);
                            energy += energyAttack * collisionInput;
                            energy *= (1.0f - energyRelease);
                            energy = juce::jlimit(0.0f, 0.65f, energy);

                            const float gateTarget = juce::jlimit(0.0f, 1.0f,
                                energy * (1.20f + 0.55f * vce.velocity)
                                + std::abs(displacement) * 7.0f);
                            const float gateCoeff = gateTarget > gate
                                ? 0.16f + 0.10f * vce.velocity
                                : 0.010f + 0.008f * (1.0f - vce.velocity);
                            gate += gateCoeff * (gateTarget - gate);
                            gate = juce::jlimit(0.0f, 1.0f, gate);

                            // Use the same seeded stochastic source as the rest of the
                            // wire bed instead of a position-dependent sine. The sine produced
                            // a faint pitched/metallic whistle when many wires were active together.
                            const float wireNoiseSource = vce.noiseLow[fc];
                            noiseState += 0.22f * (wireNoiseSource - noiseState);
                            const float wireNoise = noiseState;
                            const float noiseGate = gate * (0.18f + 0.82f * energy);
                            const float noiseGain = (0.0018f + 0.0040f * vce.velocity)
                                * sensitivity * noiseGate;
                            stringCollision += wireNoise * noiseGain;

                            // Do not synthesize a fixed high-frequency sine here.
                            // That produced a bell-like whistle when several wires
                            // lined up in the upper partials. The collision layer is
                            // intentionally noise/impact based instead.
                            const float contactBurst = collisionKick * gate
                                * (0.0012f + 0.0024f * vce.velocity);
                            stringCollision += noiseState * contactBurst;

                            stringGateSum += gate;
                        }

                        const float averageGate = stringGateSum
                            / (float)Voice::snareStringCount;
                        // The old white-noise layer is now physically gated by the
                        // virtual wire bed. At collision mix = 0 it behaves exactly as
                        // before; at 100% it becomes collision-driven rather than a
                        // free-running noise generator.
                        const float collisionGate = juce::jlimit(0.0f, 1.0f,
                            0.10f + 1.55f * averageGate);
                        const float noiseCoupling = (1.0f - collisionAmount)
                            + collisionAmount * collisionGate;
                        wire *= noiseCoupling;
                        wireCollision += stringCollision;
                    }
                }

                if (voiceLimitForLayers > 1)
                {
                    const float voiceNorm = 1.0f / std::sqrt((float)voiceLimitForLayers);
                    wire *= voiceNorm;
                    wireCollision *= voiceNorm;
                }
            }
            wireCollision *= collisionAmount;
            float membrane = 0.0f;
            if (membraneEnabled.load())
            {
                const float tension = membraneTension.load();
                const float stiffness = membraneStiffness.load();
                const float decayControl = membraneDecay.load();
                const float velocitySense = membraneVelocity.load();
                const int layerLimit = juce::jlimit(1, 16, voiceCount.load());

                for (int vi = 0; vi < layerLimit; ++vi)
                {
                    auto& vce = voices[(size_t)vi];
                    if (vce.position < 0.0 || vce.position >= sample.getNumSamples())
                        continue;

                    const float modeledVelocity = juce::jlimit(0.0f, 1.0f,
                        vce.velocity * (1.0f - velocitySense) + std::pow(vce.velocity, 1.8f - 1.25f * dynamicResponse.load()) * velocitySense);

                    // Nonlinear membrane: displacement and velocity temporarily
                    // increase effective tension, making hard hits brighter and tighter.
                    const float displacement = juce::jlimit(0.0f, 1.0f,
                        std::abs(vce.membraneY1[fc]) * 3.0f);
                    const float nonlinearTension = juce::jlimit(0.0f, 1.0f,
                        tension + (0.10f + 0.30f * stiffness) * displacement
                        + 0.10f * modeledVelocity * displacement);
                    const float f0 = 105.0f + 185.0f * nonlinearTension
                        + 35.0f * modeledVelocity;
                    const float f1 = juce::jmin(0.42f * sr,
                        f0 * (2.05f + 1.25f * stiffness + 0.22f * displacement));
                    const float radius = juce::jlimit(0.91f, 0.997f,
                        0.94f + 0.057f * decayControl
                        - 0.018f * displacement * modeledVelocity);
                    const float voiceIdxPos = (float)vce.position;
                    const float rawDrive = raw - vce.membranePrev[fc];
                    vce.membranePrev[fc] = raw;
                    const float drive = std::tanh(rawDrive
                        * (1.0f + 1.6f * modeledVelocity
                        * (0.45f + 0.55f * stiffness)));

                    const float w0 = 2.0f * juce::MathConstants<float>::pi * f0 / sr;
                    const float w1 = 2.0f * juce::MathConstants<float>::pi * f1 / sr;

                    // Per-hit phase offsets are injected as a tiny, deterministic
                    // modulation of each resonant mode. They are different for every
                    // note-on, so repeated strikes do not line up perfectly.
                    const float phase0 = vce.phaseMod0 + voiceIdxPos * 0.00011f;
                    const float phase1 = vce.phaseMod1 + voiceIdxPos * 0.00017f;
                    const float mode0 = drive * (0.025f + 0.11f * modeledVelocity)
                        + 2.0f * radius * std::cos(w0 + 0.0025f * std::sin(phase0)) * vce.membraneY1[fc]
                        - radius * radius * vce.membraneY2[fc];
                    vce.membraneY2[fc] = vce.membraneY1[fc];
                    vce.membraneY1[fc] = juce::jlimit(-4.0f, 4.0f, mode0);

                    const float upperRadius = radius * (0.965f - 0.025f * stiffness);
                    const float mode1 = drive * (0.008f + 0.025f * stiffness * modeledVelocity)
                        + 2.0f * upperRadius * std::cos(w1 + 0.0035f * std::sin(phase1)) * vce.membraneUpperY1[fc]
                        - upperRadius * upperRadius * vce.membraneUpperY2[fc];
                    vce.membraneUpperY2[fc] = vce.membraneUpperY1[fc];
                    vce.membraneUpperY1[fc] = juce::jlimit(-4.0f, 4.0f, mode1);

                    const float env = std::exp(-(float)vce.position / (float)(sourceRate * (0.08f + 0.14f * decayControl)));
                    membrane += (vce.membraneY1[fc] * 0.18f + vce.membraneUpperY1[fc] * 0.08f) * env;
                }

                if (layerLimit > 1)
                    membrane *= 1.0f / std::sqrt((float)layerLimit);
            }
            const float phaseVocoder = processPhaseVocoder(
                spectralOut, phaseVocoderMix.load(),
                juce::jlimit(0.0f, 1.0f, hitVelocity));
            float living = spectralOut * tailShape + wire + wireCollision + membrane
                + phaseVocoder * phaseVocoderMix.load() * 0.85f;

            // Parallel, velocity-aware compression. The dry transient remains intact;
            // the compressed branch mainly fills the body and decaying tail.
            const float compMix = compressorMix.load();
            const float level = std::abs(living);
            const float envCoeff = level > compressorEnvelope[(size_t)fc] ? 0.018f : 0.0025f;
            compressorEnvelope[(size_t)fc] += envCoeff * (level - compressorEnvelope[(size_t)fc]);
            const float threshold = 0.16f - 0.05f * velocity;
            const float over = juce::jmax(0.0f, compressorEnvelope[(size_t)fc] - threshold);
            const float ratio = 2.0f + 4.0f * compMix;
            const float targetGain = over > 0.0f
                ? std::pow(juce::jmax(0.05f, threshold / (threshold + over)), 1.0f - 1.0f / ratio)
                : 1.0f;
            const float gainCoeff = targetGain < compressorGain[(size_t)fc] ? 0.035f : 0.006f;
            compressorGain[(size_t)fc] += gainCoeff * (targetGain - compressorGain[(size_t)fc]);
            const float compressed = living * compressorGain[(size_t)fc] * (1.0f + 0.10f * compMix * (1.0f - velocity));
            living += compressed * (0.16f + 0.34f * compMix);

            // Short room reflections are velocity-shaped so the reverb adds depth
            // and "air" rather than a long synthetic wash.
            living += processRoomReverb(living, fc, velocity);

            out.setSample(ch, i, std::tanh(living));
        }
        const int activeLimit = juce::jlimit(1, 16, voiceCount.load());
        for (int vi = 0; vi < activeLimit; ++vi)
            if (voices[(size_t)vi].position >= 0.0)
            {
                voices[(size_t)vi].position += playbackRate;
                if (voices[(size_t)vi].position >= sample.getNumSamples())
                    voices[(size_t)vi].position = -1.0;
            }
        playbackPosition += playbackRate;
    }
}

juce::AudioProcessorEditor* BlackDrumAudioProcessor::createEditor()
{
    return new BlackDrumAudioProcessorEditor(*this);
}

juce::ValueTree BlackDrumAudioProcessor::makeStateTree() const
{
    juce::ValueTree state("BlackDrumState");
    state.setProperty("version", 3, nullptr);
    state.setProperty("samplePath", loadedFile.getFullPathName(), nullptr);
    state.setProperty("voiceCount", getVoiceCount(), nullptr);
    state.setProperty("bodyMix", getBodyMix(), nullptr);
    state.setProperty("wireNoiseMix", getWireNoiseMix(), nullptr);
    state.setProperty("spectralMix", 0.0f, nullptr);
    state.setProperty("phaseVocoderMix", getPhaseVocoderMix(), nullptr);
    state.setProperty("compressorMix", getCompressorMix(), nullptr);
    state.setProperty("roomReverbMix", getRoomReverbMix(), nullptr);
    state.setProperty("physicalSynthMix", getPhysicalSynthMix(), nullptr);
    state.setProperty("wireCollisionMix", getWireCollisionMix(), nullptr);
    state.setProperty("transient", getTransient(), nullptr);
    state.setProperty("sustain", getSustain(), nullptr);
    state.setProperty("dynamicResponse", getDynamicResponse(), nullptr);
    state.setProperty("membraneEnabled", getMembraneEnabled(), nullptr);
    state.setProperty("membraneTension", getMembraneTension(), nullptr);
    state.setProperty("membraneStiffness", getMembraneStiffness(), nullptr);
    state.setProperty("membraneDecay", getMembraneDecay(), nullptr);
    state.setProperty("membraneVelocity", getMembraneVelocity(), nullptr);
    return state;
}

juce::ValueTree BlackDrumAudioProcessor::createPresetState() const
{
    return makeStateTree();
}

bool BlackDrumAudioProcessor::restoreStateTree(const juce::ValueTree& state)
{
    if (!state.isValid() || state.getType() != juce::Identifier("BlackDrumState"))
        return false;

    setVoiceCount((int) state.getProperty("voiceCount", getVoiceCount()));
    setBodyMix((float) state.getProperty("bodyMix", getBodyMix()));
    setWireNoiseMix((float) state.getProperty("wireNoiseMix", getWireNoiseMix()));
    setSpectralMix(0.0f);
    setPhaseVocoderMix((float) state.getProperty("phaseVocoderMix", getPhaseVocoderMix()));
    setCompressorMix((float) state.getProperty("compressorMix", getCompressorMix()));
    setRoomReverbMix((float) state.getProperty("roomReverbMix", getRoomReverbMix()));
    setPhysicalSynthMix((float) state.getProperty("physicalSynthMix", getPhysicalSynthMix()));
    setWireCollisionMix((float) state.getProperty("wireCollisionMix", getWireCollisionMix()));
    setTransient((float) state.getProperty("transient", getTransient()));
    setSustain((float) state.getProperty("sustain", getSustain()));
    setDynamicResponse((float) state.getProperty("dynamicResponse", getDynamicResponse()));
    setMembraneEnabled((bool) state.getProperty("membraneEnabled", getMembraneEnabled()));
    setMembraneTension((float) state.getProperty("membraneTension", getMembraneTension()));
    setMembraneStiffness((float) state.getProperty("membraneStiffness", getMembraneStiffness()));
    setMembraneDecay((float) state.getProperty("membraneDecay", getMembraneDecay()));
    setMembraneVelocity((float) state.getProperty("membraneVelocity", getMembraneVelocity()));

    const auto path = state.getProperty("samplePath").toString();
    if (path.isNotEmpty())
        loadSample(juce::File(path));
    return true;
}

bool BlackDrumAudioProcessor::applyPresetState(const juce::ValueTree& state)
{
    return restoreStateTree(state);
}

void BlackDrumAudioProcessor::getStateInformation(juce::MemoryBlock& d)
{
    const auto state = makeStateTree();
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, d);
}

void BlackDrumAudioProcessor::setStateInformation(const void* data, int size)
{
    if (data == nullptr || size <= 0)
        return;

    if (auto xml = getXmlFromBinary(data, size))
    {
        const auto state = juce::ValueTree::fromXml(*xml);
        if (restoreStateTree(state))
            return;
    }

    // Backward compatibility with older BlackDrum states that contained only the sample path.
    juce::MemoryInputStream legacy(data, (size_t) size, false);
    const auto p = legacy.readString();
    if (p.isNotEmpty())
        loadSample(juce::File(p));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BlackDrumAudioProcessor();
}