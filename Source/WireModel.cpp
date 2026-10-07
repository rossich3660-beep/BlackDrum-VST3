#include "WireModel.h"
#include <cmath>

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
constexpr float kMaxForce = 5.0f;
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
    previousHeadVelocity = 0.0f;
    randomState = 0xA341316Cu;

    for (auto& wire : wires)
        wire = {};
}

void SnareWireModel::setParameters(
    float newSnareTension01,
    float newContact01,
    float newDamping01)
{
    snareTension01 = juce::jlimit(
        0.0f, 1.0f, newSnareTension01);

    contact01 = juce::jlimit(
        0.0f, 1.0f, newContact01);

    damping01 = juce::jlimit(
        0.0f, 1.0f, newDamping01);

    updateWires();
}

void SnareWireModel::updateWires()
{
    // A real wire is a tensioned string with several partials. The audible
    // result is therefore dominated by contact impulses and string velocity,
    // not by a bank of free-running sine oscillators.
    const float fundamentalHz =
        juce::jmap(
            snareTension01,
            260.0f,
            920.0f);

    for (int i = 0; i < NumWires; ++i)
    {
        auto& wire = wires[(size_t) i];

        const float wireScale =
            tensionScale[(size_t) i];

        const float gapVariation =
            gapScale[(size_t) i];

        wire.gap =
            0.0016f
            + 0.0028f * (1.0f - contact01)
            + 0.0010f * (gapVariation - 1.0f);

        wire.spring =
            1.8f
            + 4.6f * contact01
            + 0.6f * snareTension01;

        wire.contactDamping =
            0.055f
            + 0.11f * contact01
            + 0.05f * damping01;

        wire.roughness =
            0.0004f
            + 0.0016f * contact01
            + 0.0005f * std::abs(wireScale - 1.0f);

        wire.outputGain =
            (0.012f + 0.018f * contact01)
            / (1.0f + 0.11f * static_cast<float>(i));

        const float modeSpread =
            1.0f + 0.012f * static_cast<float>(i);

        for (int modeIndex = 0; modeIndex < NumStringModes; ++modeIndex)
        {
            auto& mode =
                wire.modes[(size_t) modeIndex];

            const float harmonic =
                static_cast<float>(modeIndex + 1);

            // Small stiffness/length irregularity makes the string bank
            // inharmonic without turning it into ten obvious pitches.
            const float stiffnessShift =
                1.0f
                + 0.012f * harmonic * harmonic
                + 0.002f * (wireScale - 1.0f);

            const float frequency =
                juce::jlimit(
                    180.0f,
                    5200.0f,
                    fundamentalHz
                    * wireScale
                    * harmonic
                    * modeSpread
                    * stiffnessShift);

            const float omega =
                kTwoPi * frequency;

            const float decaySeconds =
                juce::jlimit(
                    0.012f,
                    0.085f,
                    0.050f
                    * (1.08f - 0.38f * damping01)
                    * (0.78f + 0.44f * contact01)
                    / (1.0f + 0.13f * static_cast<float>(modeIndex)));

            mode.omega = omega;
            mode.damping =
                1.0f / decaySeconds;

            mode.weight =
                1.0f
                / (1.0f
                   + 0.42f
                     * static_cast<float>(modeIndex));

            if (modeIndex == 0)
                mode.weight *= 0.80f;
        }
    }
}

void SnareWireModel::trigger(
    float impactEnergy,
    float velocity01)
{
    const float velocity =
        juce::jlimit(0.0f, 1.0f, velocity01);

    const float impulse =
        juce::jlimit(
            0.0f,
            0.45f,
            impactEnergy
            * (0.012f + 0.025f * contact01)
            * (0.5f + 0.5f * velocity));

    triggerEnergy =
        juce::jmin(
            0.45f,
            triggerEnergy + impulse);

    triggerDecayPerSample =
        std::exp(
            -1.0f
            / (juce::jmap(
                velocity,
                0.015f,
                0.0040f)
               * static_cast<float>(sampleRate)));

    // A strike slightly redistributes the resting slack of each wire.
    for (int i = 0; i < NumWires; ++i)
    {
        auto& wire = wires[(size_t) i];

        const float variation =
            0.85f
            + 0.30f
              * (0.5f + 0.5f * randomBipolar());

        wire.contactState =
            juce::jlimit(
                0.0f,
                1.0f,
                wire.contactState
                + 0.08f
                  * contact01
                  * variation
                  * (0.35f + 0.65f * velocity));

        for (auto& mode : wire.modes)
        {
            mode.velocity +=
                0.0025f
                * randomBipolar()
                * velocity;
        }
    }
}

float SnareWireModel::processSample(
    float bottomDisplacement,
    float bottomVelocity)
{
    const float dt =
        1.0f / static_cast<float>(sampleRate);

    const float head =
        juce::jlimit(
            -1.0f,
            1.0f,
            bottomDisplacement);

    const float headVelocity =
        juce::jlimit(
            -25.0f,
            25.0f,
            bottomVelocity);

    triggerEnergy *= triggerDecayPerSample;

    float outputVelocity = 0.0f;
    float outputContact = 0.0f;

    for (int i = 0; i < NumWires; ++i)
    {
        auto& wire = wires[(size_t) i];

        float displacement = 0.0f;
        float velocity = 0.0f;

        for (auto& mode : wire.modes)
        {
            displacement += mode.displacement * mode.weight;
            velocity += mode.velocity * mode.weight;
        }

        const float relativePosition =
            head - displacement - wire.gap;

        const float relativeVelocity =
            headVelocity - velocity;

        // One-sided contact: the head can compress the wire against the
        // snare bed, but the wire can freely lift away from the head.
        const float penetration =
            juce::jmax(
                0.0f,
                relativePosition);

        const float closingVelocity =
            juce::jmax(
                0.0f,
                relativeVelocity);

        const float openingVelocity =
            juce::jmax(
                0.0f,
                -relativeVelocity);

        const float contactBuild =
            juce::jlimit(
                0.0f,
                1.0f,
                0.74f * wire.contactState
                + 0.22f * penetration * 18.0f
                + 0.10f * closingVelocity);

        wire.contactState =
            contactBuild
            * (0.985f - 0.020f * damping01);

        // Kelvin-Voigt contact: spring force plus velocity-dependent
        // compression. No contact means no force.
        const float nonlinearSpring =
            wire.spring
            * penetration
            * (1.0f + 2.4f * penetration);

        const float damperForce =
            wire.contactDamping
            * closingVelocity;

        // Re-contact is deliberately softer than continuous compression.
        const float impactForce =
            (nonlinearSpring + damperForce)
            * (0.42f + 0.88f * contact01)
            * (0.35f + 0.65f * contactBuild);

        const float friction =
            contactBuild
            * std::tanh(
                relativeVelocity
                * (0.55f + 1.10f * contact01))
            * (0.003f + 0.008f * contact01);

        const float force =
            juce::jlimit(
                -kMaxForce,
                kMaxForce,
                impactForce + friction + triggerEnergy * 0.025f);

        const float contactImpulse =
            force - wire.previousForce;

        wire.previousForce =
            force;

        for (auto& mode : wire.modes)
        {
            const float modalDrive =
                force * mode.weight;

            const float acceleration =
                modalDrive
                - 2.0f * mode.damping * mode.velocity
                - mode.omega * mode.omega * mode.displacement;

            mode.velocity +=
                acceleration * dt;

            mode.displacement +=
                mode.velocity * dt;
        }

        // When the head opens away, the wire can snap out of contact. The
        // stored state decays naturally instead of being forced to follow it.
        if (openingVelocity > 0.0f)
        {
            wire.contactState *=
                std::exp(
                    -openingVelocity
                    * (2.0f + 4.0f * contact01)
                    * dt);
        }

        const float microRoughness =
            randomBipolar()
            * wire.roughness
            * contactBuild
            * (0.18f + 0.42f * std::abs(relativeVelocity));

        outputVelocity +=
            velocity
            * wire.outputGain;

        outputContact +=
            contactImpulse
            * wire.outputGain
            * (0.008f + 0.014f * contact01)
            + microRoughness;
    }

    previousHeadVelocity = headVelocity;

    // The microphone hears mainly velocity/recontact energy, not the static
    // displacement of the wire. A gentle saturation keeps bursts natural.
    // Wires should read as contact texture riding on the snare, not as
    // an independent pitched instrument.
    const float raw =
        0.34f * outputVelocity
        + 0.66f * outputContact;

    return std::tanh(raw * 2.0f) * 0.28f;
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
