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
    airDisplacement = 0.0f;
    airVelocity = 0.0f;

    for (auto* head : { &topHead, &bottomHead })
    {
        for (auto& voice : head->voices)
        {
            voice.active = false;
            voice.age = 0;

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
        const float modeDecay =
            baseDecaySeconds / (1.0f + 0.105f * static_cast<float>(i));

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
    auto& voice = topHead.voices[(size_t) chooseVoice(topHead)];

    voice.active = true;
    voice.age = ++ageCounter;

    const float radius = juce::jlimit(0.0f, 0.98f, hitPosition01);

    std::array<float, NumModes> weights {};
    float energy = 0.0f;

    for (int i = 0; i < NumModes; ++i)
    {
        const auto& spec = modeSpecs[(size_t) i];
        const float shape = besselJ(spec.angularOrder, spec.zero * radius);
        const float modalGain = 1.0f / (1.0f + 0.075f * static_cast<float>(i));

        weights[(size_t) i] = shape * modalGain;
        energy += weights[(size_t) i] * weights[(size_t) i];
    }

    const float normalizer =
        energy > 1.0e-8f ? 1.0f / std::sqrt(energy) : 1.0f;

    const float impactEnergy =
        0.075f + 1.05f * std::pow(velocity, 1.22f);

    const float baseDecaySeconds =
        juce::jmap(damping01, 0.0f, 1.0f, 1.85f, 0.42f);

    configureVoice(voice, topTuningHz, baseDecaySeconds);

    for (int i = 0; i < NumModes; ++i)
    {
        auto& mode = voice.modes[(size_t) i];

        mode.amplitude =
            weights[(size_t) i] * normalizer * impactEnergy;

        mode.amplitude *=
            1.0f + 0.018f * static_cast<float>(i) * velocity;
    }

    // A real hit changes the pressure inside the shell immediately.
    // Keeping the cavity state here makes fast MIDI repetitions interact
    // with the previous hit instead of resetting the acoustic space.
    if (airCoupling01 > 0.0f)
    {
        airVelocity += 0.018f * impactEnergy * airCoupling01;
        airDisplacement += 0.00035f * impactEnergy * airCoupling01;
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

        configureVoice(voice, bottomTuningHz, baseDecaySeconds);

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

        for (auto& mode : voice.modes)
        {
            if (std::abs(mode.amplitude) > 1.0e-7f)
            {
                audible = true;
                voiceOutput += mode.amplitude * std::sin(mode.phase);

                mode.phase += mode.phaseStep;

                if (mode.phase >= kTwoPi)
                    mode.phase -= kTwoPi;

                mode.amplitude *= mode.decayPerSample;
            }
        }

        if (!audible)
        {
            voice.active = false;
            continue;
        }

        mix += voiceOutput;
    }

    return std::tanh(mix * 0.72f) * 0.72f;
}

float SnareMembraneModel::processSample()
{
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
    const float bottom =
        bottomBeforeCoupling * (0.78f + 0.55f * coupling)
        + pressure * (0.018f + 0.050f * coupling);

    return std::tanh((top + bottom * 0.80f) * 0.86f) * 0.78f;
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
