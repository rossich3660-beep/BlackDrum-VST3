#include "MembraneModel.h"
#include <cmath>

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
constexpr float kD60 = 6.90775527898f;
}

void SnareMembraneModel::prepare(double rate)
{
    sampleRate = rate > 1000.0 ? rate : 44100.0;
    reset();
}

void SnareMembraneModel::reset()
{
    ageCounter = 0;
    randomState = 0x6D2B79F5u;
    samplesSinceLastHit = 1000000;
    airDisplacement = 0.0f;
    airVelocity = 0.0f;
    lastBottomContact = 0.0f;

    for (auto* head : { &topHead, &bottomHead })
    {
        for (auto& voice : head->voices)
        {
            voice.active = false;
            voice.age = 0;
            voice.nonlinearAmount = 0.0f;
            voice.attackLevel = 0.0f;
            voice.attackDecayPerSample = 1.0f;
            voice.attackFilter = 0.0f;
            voice.attackPreviousNoise = 0.0f;

            for (auto& mode : voice.modes)
                mode = {};
        }
    }
}

void SnareMembraneModel::setParameters(
    float newTopTuningHz,
    float newDamping01,
    float newHitPosition01,
    float newBottomTuningHz,
    float newAirCoupling01)
{
    topTuningHz = juce::jlimit(90.0f, 360.0f, newTopTuningHz);
    bottomTuningHz = juce::jlimit(90.0f, 360.0f, newBottomTuningHz);
    damping01 = juce::jlimit(0.0f, 1.0f, newDamping01);
    hitPosition01 = juce::jlimit(0.0f, 1.0f, newHitPosition01);
    airCoupling01 = juce::jlimit(0.0f, 1.0f, newAirCoupling01);
}

int SnareMembraneModel::chooseVoice(Head& head) noexcept
{
    int oldest = 0;

    for (int i = 0; i < MaxVoices; ++i)
    {
        if (!head.voices[(size_t) i].active)
            return i;

        if (head.voices[(size_t) i].age < head.voices[(size_t) oldest].age)
            oldest = i;
    }

    return oldest;
}

int SnareMembraneModel::findActiveVoice(const Head& head) const noexcept
{
    int best = -1;
    unsigned int bestAge = 0;

    for (int i = 0; i < MaxVoices; ++i)
    {
        const auto& voice = head.voices[(size_t) i];

        if (!voice.active)
            continue;

        if (best < 0 || voice.age > bestAge)
        {
            best = i;
            bestAge = voice.age;
        }
    }

    return best;
}

void SnareMembraneModel::configureVoice(
    Voice& voice,
    float tuningHz,
    float baseDecaySeconds)
{
    constexpr float fundamentalZero = modeSpecs[0].zero;

    for (int i = 0; i < NumModes; ++i)
    {
        const auto& spec = modeSpecs[(size_t) i];
        auto& mode = voice.modes[(size_t) i];

        const float relativeFrequency = spec.zero / fundamentalZero;
        const float frequency = tuningHz * relativeFrequency;
        const float modeIndex = static_cast<float>(i);

        // Real membrane damping is frequency dependent: upper partials
        // disappear faster than the fundamental, especially as DAMP rises.
        const float spectralDamping =
            0.105f
            + 0.24f * damping01
            + 0.012f * damping01 * modeIndex;

        const float modeDecay =
            baseDecaySeconds
            / (1.0f
               + spectralDamping * modeIndex
               + 0.008f * damping01 * modeIndex * modeIndex);

        const float safeDecay = juce::jmax(0.05f, modeDecay);

        mode.phase = 0.0f;
        mode.phaseStep = kTwoPi * frequency / static_cast<float>(sampleRate);
        mode.decayPerSample =
            std::exp(-kD60 / (safeDecay * static_cast<float>(sampleRate)));
    }
}

void SnareMembraneModel::trigger(float velocity01)
{
    const float velocity = juce::jlimit(0.0f, 1.0f, velocity01);

    const int intervalSamples = samplesSinceLastHit;
    samplesSinceLastHit = 0;

    // Real drum hits are repeatable, but never numerically identical.
    // Keep the variation small and correlated with the previous hit interval.
    const float repetitionMemory =
        std::exp(-static_cast<float>(intervalSamples)
                 / (0.075f * static_cast<float>(sampleRate)));

    const float tuningJitter =
        1.0f + 0.0055f * randomBipolar() * (0.65f + 0.35f * velocity);

    const float positionJitter =
        0.018f * randomBipolar() * (0.35f + 0.65f * velocity);

    const float decayJitter =
        1.0f + 0.035f * randomBipolar();

    const float energyJitter =
        1.0f + 0.022f * randomBipolar();

    auto& voice = topHead.voices[(size_t) chooseVoice(topHead)];

    voice.active = true;
    voice.age = ++ageCounter;

    voice.nonlinearAmount =
        juce::jlimit(
            0.012f,
            0.085f,
            0.022f + 0.050f * velocity
            + 0.012f * (0.5f + 0.5f * randomBipolar()));

    const float radius = juce::jlimit(
        0.0f,
        0.98f,
        hitPosition01 + positionJitter);

    std::array<float, NumModes> weights {};
    float energy = 0.0f;

    for (int i = 0; i < NumModes; ++i)
    {
        const auto& spec = modeSpecs[(size_t) i];
        const float shape = besselJ(spec.angularOrder, spec.zero * radius);
        const float modalGain =
            1.0f / (1.0f + 0.075f * static_cast<float>(i));

        // Harder hits excite more high-order membrane modes.
        const float highModePosition =
            static_cast<float>(i + 1)
            / static_cast<float>(NumModes);

        const float velocityBrightness =
            1.0f
            + 0.13f
              * velocity
              * std::pow(highModePosition, 1.22f)
              * (1.0f - 0.62f * damping01);

        const float hitToHitJitter =
            1.0f + 0.012f * randomBipolar();

        weights[(size_t) i] =
            shape * modalGain * velocityBrightness * hitToHitJitter;
        energy += weights[(size_t) i] * weights[(size_t) i];
    }

    const float normalizer =
        energy > 1.0e-8f ? 1.0f / std::sqrt(energy) : 1.0f;

    const float impactEnergy =
        juce::jlimit(
            0.06f,
            1.55f,
            (0.055f + 1.15f * std::pow(velocity, 1.28f))
            * energyJitter
            * (1.0f - 0.014f * repetitionMemory));

    // Contact is shortest on a hard hit and slightly softer on a light hit.
    // It is deliberately separate from the resonant membrane tail.
    const float attackTauSeconds =
        juce::jmap(
            velocity,
            0.0030f,
            0.00085f)
        * juce::jmap(
            damping01,
            1.0f,
            0.72f);

    voice.attackLevel =
        juce::jlimit(
            0.012f,
            0.34f,
            (0.045f + 0.24f * velocity)
            * (0.92f + 0.08f * energyJitter));

    voice.attackDecayPerSample =
        std::exp(
            -1.0f
            / (juce::jmax(0.0005f, attackTauSeconds)
               * static_cast<float>(sampleRate)));

    voice.attackFilter = 0.0f;
    voice.attackPreviousNoise = randomBipolar();

    const float baseDecaySeconds =
        juce::jmap(damping01, 0.0f, 1.0f, 1.85f, 0.42f)
        * decayJitter
        * (1.0f - 0.075f * velocity);

    configureVoice(
        voice,
        topTuningHz * tuningJitter,
        juce::jmax(0.10f, baseDecaySeconds));

    for (int i = 0; i < NumModes; ++i)
    {
        auto& mode = voice.modes[(size_t) i];

        mode.amplitude =
            weights[(size_t) i] * normalizer * impactEnergy;

        // Small phase differences stop repeated hits from lining up into
        // the same synthetic waveform.
        mode.phase +=
            0.035f * randomBipolar()
            + 0.020f * repetitionMemory * randomBipolar();
    }

    // A real hit changes the pressure inside the shell immediately.
    // Keeping the cavity state here makes fast MIDI repetitions interact
    // with the previous hit instead of resetting the acoustic space.
    if (airCoupling01 > 0.0f)
    {
        // A short pressure impulse makes the lower head respond to the hit,
        // rather than waiting for a weak steady-state feedback signal.
        airVelocity += 0.035f * impactEnergy * airCoupling01;
        airDisplacement +=
            0.00110f * impactEnergy * airCoupling01;

        exciteBottomFromHit(
            impactEnergy,
            velocity,
            airCoupling01);
    }
}

void SnareMembraneModel::exciteBottomFromHit(
    float impactEnergy,
    float velocity01,
    float coupling)
{
    const int voiceIndex = chooseVoice(bottomHead);
    auto& voice = bottomHead.voices[(size_t) voiceIndex];

    voice.active = true;
    voice.age = ++ageCounter;

    const float tuningJitter =
        1.0f + 0.0045f * randomBipolar();

    const float baseDecaySeconds =
        juce::jmap(damping01, 0.0f, 1.0f, 1.55f, 0.34f)
        * (0.98f + 0.04f * randomBipolar());

    configureVoice(
        voice,
        bottomTuningHz * tuningJitter,
        juce::jmax(0.09f, baseDecaySeconds));

    voice.nonlinearAmount =
        juce::jlimit(
            0.008f,
            0.055f,
            0.012f + 0.028f * velocity01);

    const float directEnergy =
        impactEnergy
        * (0.10f + 0.34f * coupling)
        * (0.70f + 0.30f * velocity01);

    for (int i = 0; i < NumModes; ++i)
    {
        auto& mode = voice.modes[(size_t) i];
        const auto& spec = modeSpecs[(size_t) i];

        const float transferShape =
            (1.0f / (1.0f + 0.13f * static_cast<float>(i)))
            * (spec.angularOrder == 0 ? 1.0f : 0.80f);

        mode.amplitude =
            directEnergy
            * transferShape
            * (1.0f + 0.018f * randomBipolar());

        mode.phase += 0.025f * randomBipolar();
    }
}

void SnareMembraneModel::driveBottomFromAir(float pressure, float coupling)
{
    if (std::abs(pressure) < 1.0e-7f)
        return;

    int voiceIndex = findActiveVoice(bottomHead);

    if (voiceIndex < 0)
    {
        voiceIndex = chooseVoice(bottomHead);
        auto& voice = bottomHead.voices[(size_t) voiceIndex];

        voice.active = true;
        voice.age = ++ageCounter;

        const float baseDecaySeconds =
            juce::jmap(damping01, 0.0f, 1.0f, 1.55f, 0.34f);

        configureVoice(
            voice,
            bottomTuningHz * (1.0f + 0.003f * randomBipolar()),
            baseDecaySeconds);

        voice.nonlinearAmount = 0.018f + 0.018f * coupling;

        for (auto& mode : voice.modes)
            mode.amplitude = 0.0f;
    }

    auto& voice = bottomHead.voices[(size_t) voiceIndex];

    // The previous value was far too small to make the lower head
    // perceptible. This remains a bounded pressure coupling, but gives
    // AIR a real acoustic consequence and lets BOTTOM change the pitch
    // of the coupled lower-head resonances.
    const float baseDrive = 0.10f + 0.18f * coupling;

    for (int i = 0; i < NumModes; ++i)
    {
        const auto& spec = modeSpecs[(size_t) i];
        auto& mode = voice.modes[(size_t) i];

        const float modeWeight =
            (1.0f / (1.0f + 0.11f * static_cast<float>(i))) *
            (spec.angularOrder == 0 ? 1.0f : 0.88f);

        mode.amplitude += pressure * coupling * baseDrive * modeWeight;

        // Bound the accumulated modal energy so aggressive MIDI streams
        // cannot destabilize the cavity/head feedback loop.
        mode.amplitude = juce::jlimit(-0.16f, 0.16f, mode.amplitude);
    }
}

void SnareMembraneModel::applyTopAirFeedback(float pressure, float coupling)
{
    const int voiceIndex = findActiveVoice(topHead);

    if (voiceIndex < 0)
        return;

    auto& voice = topHead.voices[(size_t) voiceIndex];

    for (int i = 0; i < NumModes; ++i)
    {
        auto& mode = voice.modes[(size_t) i];

        const float feedbackWeight =
            i == 0 ? 1.0f : 0.35f / (1.0f + 0.08f * static_cast<float>(i));

        mode.amplitude -=
            pressure * coupling * 0.00016f * feedbackWeight;

        mode.amplitude = juce::jlimit(-0.16f, 0.16f, mode.amplitude);
    }
}

float SnareMembraneModel::processHead(Head& head)
{
    float mix = 0.0f;

    for (auto& voice : head.voices)
    {
        if (!voice.active)
            continue;

        float voiceOutput = 0.0f;
        bool audible = false;

        // Short stick/material contact. The filtered stochastic component
        // gives the attack texture of a real hit without becoming hiss.
        if (voice.attackLevel > 1.0e-5f)
        {
            const float noise = randomBipolar();

            voice.attackFilter =
                0.78f * voice.attackFilter
                + 0.22f * noise;

            const float highPassed =
                noise
                - voice.attackPreviousNoise * 0.92f;

            voice.attackPreviousNoise = noise;

            const float contact =
                0.58f * voice.attackFilter
                + 0.42f * highPassed;

            voiceOutput += voice.attackLevel * contact;

            voice.attackLevel *= voice.attackDecayPerSample;
        }

        for (auto& mode : voice.modes)
        {
            if (std::abs(mode.amplitude) > 1.0e-7f
            || voice.attackLevel > 1.0e-5f)
            {
                audible = true;

                const float amplitudeAbs =
                    std::abs(mode.amplitude);

                // Membrane tension rises with displacement, so loud hits
                // slightly raise the instantaneous modal frequency instead
                // of producing a perfectly static oscillator.
                const float nonlinearPitch =
                    1.0f
                    + voice.nonlinearAmount
                      * amplitudeAbs * amplitudeAbs;

                const float fundamental =
                    std::sin(mode.phase);

                const float softSecondHarmonic =
                    0.012f
                    * voice.nonlinearAmount
                    * amplitudeAbs
                    * std::sin(mode.phase * 2.0f);

                voiceOutput +=
                    mode.amplitude
                    * (fundamental + softSecondHarmonic);

                mode.phase += mode.phaseStep * nonlinearPitch;

                if (mode.phase >= kTwoPi)
                    mode.phase -= kTwoPi;

                const float nonlinearLoss =
                    1.0f
                    - juce::jlimit(
                        0.0f,
                        0.015f,
                        0.0018f
                        * voice.nonlinearAmount
                        * amplitudeAbs);

                mode.amplitude *=
                    mode.decayPerSample * nonlinearLoss;
            }
        }

        if (!audible)
        {
            voice.active = false;
            voice.nonlinearAmount = 0.0f;
            voice.attackLevel = 0.0f;
            continue;
        }

        mix += voiceOutput;
    }

    return std::tanh(mix * 0.72f) * 0.72f;
}

float SnareMembraneModel::processSample()
{
    if (samplesSinceLastHit < 1000000000)
        ++samplesSinceLastHit;

    const float topBeforeCoupling = processHead(topHead);
    const float bottomBeforeCoupling = processHead(bottomHead);

    // Normalised cavity dynamics. The shell cavity stores acoustic energy
    // and exchanges it with both heads. This is deliberately conservative
    // at Stage 3 so later shell/snare-wire stages can add energy safely.
    const float difference =
        juce::jlimit(-1.0f, 1.0f, topBeforeCoupling - bottomBeforeCoupling);

    const float cavityFrequencyHz =
        juce::jlimit(700.0f, 1400.0f, 950.0f + 1.15f * bottomTuningHz);

    const float dt = 1.0f / static_cast<float>(sampleRate);
    const float omega =
        kTwoPi * cavityFrequencyHz;

    const float cavityDamping =
        juce::jmap(damping01, 0.0f, 1.0f, 0.055f, 0.16f);

    const float stiffness = omega * omega;
    const float coupling = airCoupling01;

    // The cavity is driven by the membrane difference, with the AIR knob
    // controlling the actual transfer into the acoustic volume.
    const float cavityDriveGain = 0.015f * coupling;

    const float acceleration =
        ((difference * cavityDriveGain) - airDisplacement) * stiffness * 0.08f
        - cavityDamping * omega * airVelocity;

    airVelocity += acceleration * dt;
    airDisplacement += airVelocity * dt;

    airDisplacement = juce::jlimit(-0.08f, 0.08f, airDisplacement);
    airVelocity = juce::jlimit(-90.0f, 90.0f, airVelocity);

    const float pressure =
        juce::jlimit(-0.16f, 0.16f, airDisplacement * 3.2f);

    driveBottomFromAir(pressure, coupling);
    applyTopAirFeedback(pressure, coupling);

    const float top = topBeforeCoupling - pressure * (0.012f + 0.010f * coupling);
    // Air-driven bottom modal energy is applied on the following sample;
    // do not process the whole bottom head a second time in the same sample.
    // Bottom-head audibility follows the actual air coupling. This makes
    // BOTTOM a meaningful tonal control while AIR still controls how much
    // of that lower-head resonance reaches the final sound.
    const float bottomOutputGain =
        0.30f + 0.95f * coupling;

    const float bottom =
        bottomBeforeCoupling * bottomOutputGain
        + pressure * (0.018f + 0.050f * coupling);

    lastBottomContact =
        juce::jlimit(
            -1.0f,
            1.0f,
            bottomBeforeCoupling
            + pressure * (0.035f + 0.045f * coupling));

    const float coupledMix =
        top + bottom * (0.55f + 0.25f * coupling);

    return std::tanh(coupledMix * 0.88f) * 0.78f;
}

float SnareMembraneModel::randomBipolar() noexcept
{
    unsigned int x = randomState;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;

    randomState = x;

    const float unit =
        static_cast<float>(x & 0x00ffffffu)
        / 16777215.0f;

    return unit * 2.0f - 1.0f;
}

float SnareMembraneModel::besselJ(int order, float x)
{
    return besselJn(order, x);
}

float SnareMembraneModel::besselJn(int order, float x)
{
    const int n = juce::jlimit(0, 8, order);

    if (std::abs(x) < 1.0e-6f)
        return n == 0 ? 1.0f : 0.0f;

    const double halfX = static_cast<double>(x) * 0.5;
    double power = 1.0;

    for (int k = 0; k < n; ++k)
        power *= halfX;

    float sum = 0.0f;

    for (int k = 0; k < 24; ++k)
    {
        double factorialK = 1.0;
        for (int j = 2; j <= k; ++j)
            factorialK *= static_cast<double>(j);

        double factorialNK = 1.0;
        for (int j = 2; j <= k + n; ++j)
            factorialNK *= static_cast<double>(j);

        double numerator =
            power * std::pow(halfX * halfX, static_cast<double>(k));

        double term =
            numerator / (factorialK * factorialNK);

        if (k & 1)
            term = -term;

        sum += static_cast<float>(term);

        if (std::abs(term) < 1.0e-10)
            break;
    }

    return sum;
}
