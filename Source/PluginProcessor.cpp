#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include "SampleAnalyzer.h"

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
    membraneY1[0] = membraneY1[1] = membraneY2[0] = membraneY2[1] = membraneUpperY1[0] = membraneUpperY1[1] = membraneUpperY2[0] = membraneUpperY2[1] = membranePrev[0] = membranePrev[1] = 0.0f;
    resetPhaseVocoder();

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
            const float morphedMagnitude = magnitude * ((1.0f - morphAmount) + morphAmount * targetGain);

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
    resetPhaseVocoder();
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
                const float autoCurve = autoVelocityCurve.load();
                const float exponent = juce::jlimit(0.45f, 2.0f, 1.8f - 1.25f * response - 0.55f * autoCurve);
                const float shapedVelocity = std::pow(v, exponent);
                voiceGain = 0.12f + 0.88f * shapedVelocity * shapedVelocity;
                // Small pitch variation plus a brighter low-pass response for harder hits.
                playbackRate = (float)(sourceRate / outputRate) * (0.992f + 0.032f * std::pow(v, 1.8f - 1.25f * dynamicResponse.load()))
                    * (1.0f - autoPitchDrop.load() * 0.025f * (1.0f - v)) * hitPitchVariation;
                const float cutoff = 1400.0f + std::pow(v, 1.8f - 1.25f * dynamicResponse.load()) * 14400.0f
                    * (0.75f + 0.85f * autoBrightness.load());
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
                    for (int mode = 0; mode < 3; ++mode)
                    {
                        newVoice.wireY1[mode][vc] = 0.0f;
                        newVoice.wireY2[mode][vc] = 0.0f;
                    }
                    for (int mode = 0; mode < 6; ++mode)
                    {
                        newVoice.shellY1[mode][vc] = 0.0f;
                        newVoice.shellY2[mode][vc] = 0.0f;
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
        const float attackSoftening = autoAttackSoftening.load();
        const float snapBoost = autoSnap.load() * velocity * velocity * 0.18f;
        const float attackAmount = (0.04f + 0.24f * velocity + snapBoost
            + attackControl * 0.20f) * hitAttackVariation
            * (1.0f - attackSoftening * (1.0f - velocity) * 0.35f);
        const float transient = juce::jmax(0.05f, 1.0f + attackAmount * std::exp(-elapsed * 3.2f));
        const float autoTail = std::exp(-elapsed * autoTailShorten.load() * (1.0f - velocity) * 1.6f);
        const float tailShape = juce::jlimit(0.35f, 1.8f,
            (1.0f + sustainControl * (1.0f - std::exp(-elapsed * 2.5f))) * autoTail);
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

            // Each active voice gets its own noise stream and phase. This prevents
            // repeated MIDI hits from sharing an identical noise waveform.
            float wire = 0.0f;
            const float wireAmount = wireNoiseMix.load() * (0.35f + 0.65f * autoNoise.load());
            const int voiceLimitForLayers = juce::jlimit(1, 16, voiceCount.load());
            if (wireAmount > 0.0001f)
            {
                // Three short resonant wire modes. Each voice has independent
                // state, so repeated hits do not share an identical wire tail.
                const float wireFreqs[3] = { 1650.0f, 2850.0f, 4300.0f };
                const float wireRadii[3] = { 0.935f, 0.915f, 0.885f };

                for (int vi = 0; vi < voiceLimitForLayers; ++vi)
                {
                    auto& vce = voices[(size_t)vi];
                    if (vce.position < 0.0 || vce.position >= sample.getNumSamples())
                        continue;

                    vce.noiseSeed ^= vce.noiseSeed << 13;
                    vce.noiseSeed ^= vce.noiseSeed >> 17;
                    vce.noiseSeed ^= vce.noiseSeed << 5;
                    const float white = ((float)(vce.noiseSeed & 0x00ffffffu) / 8388607.5f) - 1.0f;
                    vce.noiseLow[fc] += 0.22f * (white - vce.noiseLow[fc]);
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
                            * (0.020f + 0.035f * vce.velocity)
                            * std::exp(-(float)vce.position
                                / (float)(sourceRate * (0.11f + 0.10f * vce.velocity)));
                    }
                    wire += voiceWire;
                }

                if (voiceLimitForLayers > 1)
                    wire *= 1.0f / std::sqrt((float)voiceLimitForLayers);
            }
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
            float shell = 0.0f;
            const float shellAmount = shellResonanceMix.load();
            if (shellAmount > 0.0001f)
            {
                // A separate damped shell/body resonator: low, mid and upper modes.
                // Its excitation and modal gain increase with MIDI velocity, while
                // a small membrane coupling makes the body follow membrane motion.
                const float shellFreqs[6] = { 120.0f, 205.0f, 315.0f, 510.0f, 860.0f, 1450.0f };
                const float shellRadii[6] = { 0.982f, 0.976f, 0.970f, 0.958f, 0.944f, 0.925f };
                const float shellGains[6] = { 0.060f, 0.045f, 0.038f, 0.030f, 0.022f, 0.014f };
                const int shellLimit = juce::jlimit(1, 16, voiceCount.load());
                for (int vi = 0; vi < shellLimit; ++vi)
                {
                    auto& vce = voices[(size_t) vi];
                    if (vce.position < 0.0 || vce.position >= sample.getNumSamples()) continue;
                    const float shellVelocity = juce::jlimit(0.0f, 1.0f, vce.velocity);
                    const int si0 = (int) vce.position;
                    const int si1 = juce::jmin(si0 + 1, sample.getNumSamples() - 1);
                    const float sf = (float) (vce.position - si0);
                    const float shellRaw = sample.getSample(sc, si0)
                        + (sample.getSample(sc, si1) - sample.getSample(sc, si0)) * sf;
                    const float impact = shellRaw + membrane * (0.35f + 0.85f * shellVelocity);
                    for (int mode = 0; mode < 6; ++mode)
                    {
                        const float freq = juce::jmin(shellFreqs[mode] * (0.985f + 0.035f * shellVelocity), 0.40f * sr);
                        const float w = 2.0f * juce::MathConstants<float>::pi * freq / sr;
                        const float radius = juce::jlimit(0.82f, 0.993f, shellRadii[mode] + 0.010f * shellVelocity);
                        const float drive = std::tanh(impact * (0.45f + 1.35f * shellVelocity));
                        const float y = drive * shellGains[mode] * (0.45f + 0.90f * shellVelocity)
                            + 2.0f * radius * std::cos(w) * vce.shellY1[mode][fc]
                            - radius * radius * vce.shellY2[mode][fc];
                        vce.shellY2[mode][fc] = vce.shellY1[mode][fc];
                        vce.shellY1[mode][fc] = juce::jlimit(-2.0f, 2.0f, y);
                        shell += vce.shellY1[mode][fc]
                            * std::exp(-(float) vce.position / (float) (sourceRate * (0.16f + 0.18f * shellVelocity)));
                    }
                }
                if (shellLimit > 1) shell *= 1.0f / std::sqrt((float) shellLimit);
                shell *= shellAmount;
            }

            const float phaseVocoder = processPhaseVocoder(
                spectralOut, phaseVocoderMix.load(),
                juce::jlimit(0.0f, 1.0f, hitVelocity));
            const float autoBody = 1.0f + autoBodyBoost.load() * velocity * 0.22f;
            const float saturationDrive = 1.0f + autoSaturation.load() * velocity * 0.8f;
            out.setSample(ch, i, std::tanh(
                (spectralOut * tailShape * autoBody + wire + membrane
                + phaseVocoder * phaseVocoderMix.load() * 0.85f + shell) * saturationDrive));
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

SampleFeatures BlackDrumAudioProcessor::getSampleFeatures() const
{
    return sampleFeatures;
}

AutoSettings BlackDrumAudioProcessor::getAutoSettings() const
{
    AutoSettings s;
    s.brightnessAmt = autoBrightness.load();
    s.snapAmt = autoSnap.load();
    s.noiseLayerAmt = autoNoise.load();
    s.attackSoftening = autoAttackSoftening.load();
    s.tailShorten = autoTailShorten.load();
    s.pitchDropAmt = autoPitchDrop.load();
    s.saturationAmt = autoSaturation.load();
    s.bodyBoostAmt = autoBodyBoost.load();
    s.velocityCurve = autoVelocityCurve.load();
    return s;
}

void BlackDrumAudioProcessor::applyAutoSettings(const AutoSettings& s, bool markAsManual)
{
    autoBrightness.store(autoClamp(s.brightnessAmt));
    autoSnap.store(autoClamp(s.snapAmt));
    autoNoise.store(autoClamp(s.noiseLayerAmt));
    autoAttackSoftening.store(autoClamp(s.attackSoftening));
    autoTailShorten.store(autoClamp(s.tailShorten));
    autoPitchDrop.store(autoClamp(s.pitchDropAmt));
    autoSaturation.store(autoClamp(s.saturationAmt));
    autoBodyBoost.store(autoClamp(s.bodyBoostAmt));
    autoVelocityCurve.store(autoClamp(s.velocityCurve));
    if (markAsManual)
        manualAutoEdits.store(true);
}

juce::AudioProcessorEditor* BlackDrumAudioProcessor::createEditor()
{
    return new BlackDrumAudioProcessorEditor(*this);
}

juce::ValueTree BlackDrumAudioProcessor::makeStateTree() const
{
    juce::ValueTree state("BlackDrumState");
    state.setProperty("version", 4, nullptr);
    state.setProperty("samplePath", loadedFile.getFullPathName(), nullptr);
    state.setProperty("voiceCount", getVoiceCount(), nullptr);
    state.setProperty("bodyMix", getBodyMix(), nullptr);
    state.setProperty("wireNoiseMix", getWireNoiseMix(), nullptr);
    state.setProperty("spectralMix", getSpectralMix(), nullptr);
    state.setProperty("phaseVocoderMix", getPhaseVocoderMix(), nullptr);
    state.setProperty("shellResonanceMix", getShellResonanceMix(), nullptr);
    state.setProperty("transient", getTransient(), nullptr);
    state.setProperty("sustain", getSustain(), nullptr);
    state.setProperty("dynamicResponse", getDynamicResponse(), nullptr);
    state.setProperty("membraneEnabled", getMembraneEnabled(), nullptr);
    state.setProperty("membraneTension", getMembraneTension(), nullptr);
    state.setProperty("membraneStiffness", getMembraneStiffness(), nullptr);
    state.setProperty("membraneDecay", getMembraneDecay(), nullptr);
    state.setProperty("membraneVelocity", getMembraneVelocity(), nullptr);
    state.setProperty("autoBrightness", autoBrightness.load(), nullptr);
    state.setProperty("autoSnap", autoSnap.load(), nullptr);
    state.setProperty("autoNoise", autoNoise.load(), nullptr);
    state.setProperty("autoAttackSoftening", autoAttackSoftening.load(), nullptr);
    state.setProperty("autoTailShorten", autoTailShorten.load(), nullptr);
    state.setProperty("autoPitchDrop", autoPitchDrop.load(), nullptr);
    state.setProperty("autoSaturation", autoSaturation.load(), nullptr);
    state.setProperty("autoBodyBoost", autoBodyBoost.load(), nullptr);
    state.setProperty("autoVelocityCurve", autoVelocityCurve.load(), nullptr);
    state.setProperty("manualAutoEdits", manualAutoEdits.load(), nullptr);
    state.setProperty("featureDurationMs", sampleFeatures.durationMs, nullptr);
    state.setProperty("featureAttackMs", sampleFeatures.attackMs, nullptr);
    state.setProperty("featureDecayT60Ms", sampleFeatures.decayT60Ms, nullptr);
    state.setProperty("featureTailEnergyRatio", sampleFeatures.tailEnergyRatio, nullptr);
    state.setProperty("featureCentroidHz", sampleFeatures.centroidHz, nullptr);
    state.setProperty("featureBodyEnergy", sampleFeatures.bodyEnergy, nullptr);
    state.setProperty("featureSnapEnergy", sampleFeatures.snapEnergy, nullptr);
    state.setProperty("featureAirEnergy", sampleFeatures.airEnergy, nullptr);
    state.setProperty("featureFundamentalHz", sampleFeatures.fundamentalHz, nullptr);
    state.setProperty("featureToneToNoise", sampleFeatures.toneToNoise, nullptr);
    state.setProperty("featureRingAmount", sampleFeatures.ringAmount, nullptr);
    state.setProperty("featureCrestFactorDb", sampleFeatures.crestFactorDb, nullptr);
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
    setSpectralMix((float) state.getProperty("spectralMix", getSpectralMix()));
    setPhaseVocoderMix((float) state.getProperty("phaseVocoderMix", getPhaseVocoderMix()));
    setShellResonanceMix((float) state.getProperty("shellResonanceMix", getShellResonanceMix()));
    setTransient((float) state.getProperty("transient", getTransient()));
    setSustain((float) state.getProperty("sustain", getSustain()));
    setDynamicResponse((float) state.getProperty("dynamicResponse", getDynamicResponse()));
    setMembraneEnabled((bool) state.getProperty("membraneEnabled", getMembraneEnabled()));
    setMembraneTension((float) state.getProperty("membraneTension", getMembraneTension()));
    setMembraneStiffness((float) state.getProperty("membraneStiffness", getMembraneStiffness()));
    setMembraneDecay((float) state.getProperty("membraneDecay", getMembraneDecay()));
    setMembraneVelocity((float) state.getProperty("membraneVelocity", getMembraneVelocity()));
    autoBrightness.store(autoClamp((float)state.getProperty("autoBrightness", autoBrightness.load())));
    autoSnap.store(autoClamp((float)state.getProperty("autoSnap", autoSnap.load())));
    autoNoise.store(autoClamp((float)state.getProperty("autoNoise", autoNoise.load())));
    autoAttackSoftening.store(autoClamp((float)state.getProperty("autoAttackSoftening", autoAttackSoftening.load())));
    autoTailShorten.store(autoClamp((float)state.getProperty("autoTailShorten", autoTailShorten.load())));
    autoPitchDrop.store(autoClamp((float)state.getProperty("autoPitchDrop", autoPitchDrop.load())));
    autoSaturation.store(autoClamp((float)state.getProperty("autoSaturation", autoSaturation.load())));
    autoBodyBoost.store(autoClamp((float)state.getProperty("autoBodyBoost", autoBodyBoost.load())));
    autoVelocityCurve.store(autoClamp((float)state.getProperty("autoVelocityCurve", autoVelocityCurve.load())));
    manualAutoEdits.store((bool)state.getProperty("manualAutoEdits", false));
    sampleFeatures.durationMs = (float)state.getProperty("featureDurationMs", 0.0f);
    sampleFeatures.attackMs = (float)state.getProperty("featureAttackMs", 0.0f);
    sampleFeatures.decayT60Ms = (float)state.getProperty("featureDecayT60Ms", 0.0f);
    sampleFeatures.tailEnergyRatio = autoClamp((float)state.getProperty("featureTailEnergyRatio", 0.0f));
    sampleFeatures.centroidHz = (float)state.getProperty("featureCentroidHz", 0.0f);
    sampleFeatures.bodyEnergy = autoClamp((float)state.getProperty("featureBodyEnergy", 0.0f));
    sampleFeatures.snapEnergy = autoClamp((float)state.getProperty("featureSnapEnergy", 0.0f));
    sampleFeatures.airEnergy = autoClamp((float)state.getProperty("featureAirEnergy", 0.0f));
    sampleFeatures.fundamentalHz = (float)state.getProperty("featureFundamentalHz", 0.0f);
    sampleFeatures.toneToNoise = autoClamp((float)state.getProperty("featureToneToNoise", 0.0f));
    sampleFeatures.ringAmount = autoClamp((float)state.getProperty("featureRingAmount", 0.0f));
    sampleFeatures.crestFactorDb = (float)state.getProperty("featureCrestFactorDb", 0.0f);

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