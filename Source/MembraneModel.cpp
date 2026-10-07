#include "MembraneModel.h"
#include <cmath>

void SnareMembraneModel::prepare(double rate)
{
    sampleRate = rate > 1000.0 ? rate : 44100.0;
    reset();
}

void SnareMembraneModel::reset()
{
    ageCounter = 0;

    for (auto& voice : voices)
    {
        voice.active = false;
        voice.age = 0;

        for (auto& mode : voice.modes)
            mode = {};
    }
}

void SnareMembraneModel::setParameters(
    float newTuningHz,
    float newDamping01,
    float newHitPosition01)
{
    tuningHz = juce::jlimit(90.0f, 360.0f, newTuningHz);
    damping01 = juce::jlimit(0.0f, 1.0f, newDamping01);
    hitPosition01 = juce::jlimit(0.0f, 1.0f, newHitPosition01);
}

int SnareMembraneModel::chooseVoice() noexcept
{
    int oldest = 0;

    for (int i = 0; i < MaxVoices; ++i)
    {
        if (!voices[(size_t) i].active)
            return i;

        if (voices[(size_t) i].age < voices[(size_t) oldest].age)
            oldest = i;
    }

    return oldest;
}

void SnareMembraneModel::trigger(float velocity01)
{
    const float velocity = juce::jlimit(0.0f, 1.0f, velocity01);
    auto& voice = voices[(size_t) chooseVoice()];

    voice.active = true;
    voice.age = ++ageCounter;

    // The strike radius is measured from the membrane centre.
    // 0.0 = exact centre, 1.0 = rim.
    const float radius = juce::jlimit(0.0f, 0.98f, hitPosition01);

    std::array<float, NumModes> weights {};
    float energy = 0.0f;

    for (int i = 0; i < NumModes; ++i)
    {
        const auto& spec = modeSpecs[(size_t) i];
        const float besselArgument = spec.zero * radius;
        const float shape = besselJ(spec.angularOrder, besselArgument);

        // A small static modal weighting keeps the excitation stable while
        // retaining the important spatial behaviour: centre hits suppress
        // non-axisymmetric modes, while off-centre hits excite them.
        const float modalGain = 1.0f / (1.0f + 0.075f * (float) i);
        weights[(size_t) i] = shape * modalGain;
        energy += weights[(size_t) i] * weights[(size_t) i];
    }

    const float normalizer = energy > 1.0e-8f ? 1.0f / std::sqrt(energy) : 1.0f;

    // Nonlinear velocity-to-energy mapping: quiet strokes remain audible,
    // while strong strokes deposit substantially more energy into the head.
    const float impactEnergy = 0.075f + 1.05f * std::pow(velocity, 1.22f);

    const float baseDecaySeconds =
        juce::jmap(damping01, 0.0f, 1.0f, 1.85f, 0.42f);

    constexpr float twoPi = juce::MathConstants<float>::twoPi;
    constexpr float fundamentalZero = modeSpecs[0].zero;

    for (int i = 0; i < NumModes; ++i)
    {
        const auto& spec = modeSpecs[(size_t) i];
        auto& mode = voice.modes[(size_t) i];

        const float relativeFrequency = spec.zero / fundamentalZero;
        const float frequency = tuningHz * relativeFrequency;

        const float modeDecay =
            baseDecaySeconds / (1.0f + 0.105f * (float) i);

        const float safeDecay = juce::jmax(0.05f, modeDecay);
        const float d60 = -std::log(0.001f);

        mode.phase = 0.0f;
        mode.phaseStep = twoPi * frequency / (float) sampleRate;
        mode.decayPerSample = std::exp(-d60 / (safeDecay * (float) sampleRate));

        mode.amplitude = weights[(size_t) i] * normalizer * impactEnergy;

        // Higher modes are slightly more transient-like.
        mode.amplitude *= 1.0f + 0.018f * (float) i * velocity;
    }
}

float SnareMembraneModel::processSample()
{
    float mix = 0.0f;

    for (auto& voice : voices)
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

                if (mode.phase >= juce::MathConstants<float>::twoPi)
                    mode.phase -= juce::MathConstants<float>::twoPi;

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

    // Keep the linear modal model inside a predictable output envelope.
    return std::tanh(mix * 0.72f) * 0.72f;
}

float SnareMembraneModel::besselJ(float order, float x)
{
    return besselJn((int) order, x);
}

float SnareMembraneModel::besselJ0(float x)
{
    return besselJn(0, x);
}

float SnareMembraneModel::besselJ1(float x)
{
    return besselJn(1, x);
}

float SnareMembraneModel::besselJn(int order, float x)
{
    const int n = juce::jlimit(0, 8, order);

    if (std::abs(x) < 1.0e-6f)
        return n == 0 ? 1.0f : 0.0f;

    // Series evaluation is stable for the limited spatial range used by
    // the membrane modes (all arguments remain below about 10.2).
    float sum = 0.0f;

    const double halfX = (double) x * 0.5;
    double power = 1.0;

    for (int k = 0; k < n; ++k)
        power *= halfX;

    for (int k = 0; k < 24; ++k)
    {
        double factorialK = 1.0;
        for (int j = 2; j <= k; ++j)
            factorialK *= (double) j;

        double factorialNK = 1.0;
        for (int j = 2; j <= k + n; ++j)
            factorialNK *= (double) j;

        double numerator = power * std::pow(halfX * halfX, (double) k);
        double term = numerator / (factorialK * factorialNK);

        if (k & 1)
            term = -term;

        sum += (float) term;

        if (std::abs(term) < 1.0e-10)
            break;
    }

    return sum;
}
