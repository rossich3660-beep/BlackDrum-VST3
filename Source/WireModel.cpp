#include "WireModel.h"
#include <cmath>

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
}

void SnareWireModel::prepare(double rate)
{
    sampleRate = rate > 1000.0 ? rate : 44100.0;
    reset();
    updateWires();
}

void SnareWireModel::reset()
{
    triggerEnergy = 0.0f;
    triggerDecayPerSample = 0.97f;
    randomState = 0xA341316Cu;

    for (auto& wire : wires)
        wire = {};
}

void SnareWireModel::setParameters(
    float newSnareTension01,
    float newContact01,
    float newDamping01)
{
    snareTension01 =
        juce::jlimit(0.0f, 1.0f, newSnareTension01);

    contact01 =
        juce::jlimit(0.0f, 1.0f, newContact01);

    damping01 =
        juce::jlimit(0.0f, 1.0f, newDamping01);

    updateWires();
}

void SnareWireModel::updateWires()
{
    // Snare wires are intentionally in a broad, high-frequency band.
    // Their irregularity keeps the group from behaving like one oscillator.
    const float baseFrequency =
        juce::jmap(
            snareTension01,
            1750.0f,
            3600.0f);

    for (int i = 0; i < NumWires; ++i)
    {
        auto& wire = wires[(size_t) i];

        const float index =
            static_cast<float>(i);

        const float frequency =
            juce::jlimit(
                1200.0f,
                6200.0f,
                baseFrequency
                * wireRatios[(size_t) i]
                * wireIrregularity[(size_t) i]);

        wire.cosine =
            std::cos(
                kTwoPi
                * frequency
                / static_cast<float>(sampleRate));

        const float highWire =
            index
            / static_cast<float>(NumWires - 1);

        const float decaySeconds =
            juce::jlimit(
                0.018f,
                0.15f,
                0.095f
                * (1.05f - 0.32f * highWire)
                * (1.08f - 0.28f * damping01)
                * (0.78f + 0.42f * snareTension01));

        wire.radius =
            std::exp(
                -1.0f
                / (decaySeconds
                   * static_cast<float>(sampleRate)));

        wire.gain =
            (0.030f + 0.035f * contact01)
            / (1.0f + 0.09f * index);
    }
}

void SnareWireModel::trigger(
    float impactEnergy,
    float velocity01)
{
    const float velocity =
        juce::jlimit(0.0f, 1.0f, velocity01);

    const float impulse =
        impactEnergy
        * (0.008f + 0.022f * contact01)
        * (0.45f + 0.55f * velocity);

    triggerEnergy =
        juce::jlimit(
            0.0f,
            0.35f,
            triggerEnergy + impulse);

    triggerDecayPerSample =
        std::exp(
            -1.0f
            / (juce::jmap(
                velocity,
                0.012f,
                0.0035f)
               * static_cast<float>(sampleRate)));

    for (auto& wire : wires)
    {
        wire.contactState =
            juce::jlimit(
                0.0f,
                0.7f,
                wire.contactState
                + 0.04f
                  * contact01
                  * (0.5f + 0.5f * velocity));

        // Small phase-state perturbation creates different re-contact
        // timing between wires after repeated hits.
        wire.y1 += 0.008f * randomBipolar() * velocity;
    }
}

float SnareWireModel::processSample(
    float bottomDisplacement)
{
    const float bottom =
        juce::jlimit(-1.0f, 1.0f, bottomDisplacement);

    triggerEnergy *= triggerDecayPerSample;

    float output = 0.0f;

    for (int i = 0; i < NumWires; ++i)
    {
        auto& wire = wires[(size_t) i];

        // The wire follows the lower head only while it is in contact.
        // The positive part produces one-sided mechanical compression.
        const float relativeMotion =
            bottom - 0.52f * wire.y1;

        const float threshold =
            0.008f
            + 0.025f * (1.0f - contact01);

        const float compression =
            juce::jmax(
                0.0f,
                relativeMotion - threshold);

        const float closingVelocity =
            relativeMotion - wire.previousContact;

        wire.previousContact =
            relativeMotion;

        const float slip =
            juce::jmax(
                0.0f,
                closingVelocity)
            * (0.55f + 0.85f * contact01);

        const float recontact =
            juce::jlimit(
                0.0f,
                1.0f,
                0.86f * wire.contactState
                + 0.22f * compression
                + 0.08f * juce::jmax(0.0f, slip));

        wire.contactState =
            recontact
            * (0.992f - 0.012f * damping01);

        // Tangential friction produces the characteristic wire buzz.
        const float frictionNoise =
            randomBipolar()
            * compression
            * (0.015f + 0.045f * contact01);

        const float contactDrive =
            std::tanh(
                3.2f * (
                    compression
                    * (0.65f + 0.95f * contact01)
                    + slip * 0.32f
                    + triggerEnergy
                    * 0.12f));

        const float radius = wire.radius;
        const float feedback =
            2.0f * radius * wire.cosine * wire.y1
            - radius * radius * wire.y2;

        const float excitation =
            (1.0f - radius * radius)
            * contactDrive;

        const float y0 =
            feedback
            + excitation
            + frictionNoise * 0.035f;

        wire.y2 = wire.y1;
        wire.y1 = juce::jlimit(-1.2f, 1.2f, y0);

        output +=
            wire.y1
            * wire.gain
            * (0.65f + 0.35f * recontact);
    }

    // A little nonlinear saturation keeps the ten-wire bank together as one
    // physical snare system rather than sounding like ten separate tones.
    return std::tanh(output * 2.4f) * 0.62f;
}

float SnareWireModel::randomBipolar() noexcept
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
