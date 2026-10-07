#include "ShellModel.h"
#include <cmath>

namespace
{
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
constexpr float kD60 = 6.90775527898f;
}

void SnareShellModel::prepare(double rate)
{
    sampleRate = rate > 1000.0 ? rate : 44100.0;
    reset();
    updateModes();
}

void SnareShellModel::reset()
{
    impactState = 0.0f;
    previousExcitation = 0.0f;
    impactDecayPerSample = 0.98f;

    for (auto& mode : modes)
        mode = {};
}

void SnareShellModel::setParameters(
    float newTopTuningHz,
    float newBottomTuningHz,
    float newDamping01,
    float newShell01,
    float newDepth01)
{
    topTuningHz =
        juce::jlimit(90.0f, 360.0f, newTopTuningHz);

    bottomTuningHz =
        juce::jlimit(90.0f, 360.0f, newBottomTuningHz);

    damping01 =
        juce::jlimit(0.0f, 1.0f, newDamping01);

    shell01 =
        juce::jlimit(0.0f, 1.0f, newShell01);

    depth01 =
        juce::jlimit(0.0f, 1.0f, newDepth01);

    updateModes();
}

void SnareShellModel::updateModes()
{
    // The shell body is locked to the two heads, but sits somewhat above
    // their fundamental region. Depth lowers the body modes; shell stiffness
    // raises them and opens the upper partials.
    const float headAverage =
        0.5f * (topTuningHz + bottomTuningHz);

    const float depthScale =
        1.02f - 0.18f * depth01;

    const float stiffnessScale =
        0.94f + 0.12f * shell01;

    const float bodyFundamental =
        juce::jlimit(
            90.0f,
            320.0f,
            headAverage * depthScale * stiffnessScale);

    for (int i = 0; i < NumModes; ++i)
    {
        auto& mode = modes[(size_t) i];

        const float index =
            static_cast<float>(i);

        const float frequency =
            juce::jlimit(
                90.0f,
                1800.0f,
                bodyFundamental
                * modeRatios[(size_t) i]);

        const float shellDamping =
            0.48f
            + 0.95f * damping01
            + 0.20f * (1.0f - shell01);

        const float depthDamping =
            0.20f * depth01 * (0.5f + index / NumModes);

        const float decaySeconds =
            juce::jlimit(
                0.055f,
                0.80f,
                (0.52f
                 * (1.05f + 0.85f * shell01)
                 * (0.80f + 0.30f * depth01))
                / (shellDamping + 0.12f * index + depthDamping));

        mode.cosine =
            std::cos(
                kTwoPi
                * frequency
                / static_cast<float>(sampleRate));

        mode.radius =
            std::exp(
                -kD60
                / (decaySeconds
                   * static_cast<float>(sampleRate)));

        const float highMode =
            static_cast<float>(i)
            / static_cast<float>(NumModes - 1);

        mode.drive =
            (1.0f - mode.radius)
            * (0.28f + 0.42f * shell01)
            * (1.0f - 0.48f * highMode * damping01);

        mode.outputGain =
            (0.055f + 0.075f * shell01)
            / (1.0f + 0.24f * index);
    }
}

void SnareShellModel::trigger(
    float impactEnergy,
    float velocity01)
{
    const float velocity =
        juce::jlimit(0.0f, 1.0f, velocity01);

    // A hit couples mechanical energy into the shell through the bearing
    // edges and lugs. The short pulse is separate from membrane resonance.
    const float shellImpact =
        impactEnergy
        * (0.018f + 0.038f * shell01)
        * (0.70f + 0.30f * velocity);

    impactState =
        juce::jlimit(
            -0.8f,
            0.8f,
            impactState + shellImpact);

    impactDecayPerSample =
        std::exp(
            -1.0f
            / (juce::jmap(
                velocity,
                        0.0060f,
                0.0026f)
               * static_cast<float>(sampleRate)));
}

float SnareShellModel::processSample(float membraneSample)
{
    const float contact = impactState;
    impactState *= impactDecayPerSample;

    // The shell responds to both the slow membrane displacement and the
    // sharp edge/contact component. This is a force-like drive, not a
    // simple static EQ.
    const float membraneDifference =
        membraneSample - previousExcitation * 0.96f;

    previousExcitation = membraneSample;

    const float excitation =
        0.56f * membraneSample
        + 0.20f * membraneDifference
        + 0.18f * contact;

    float body = 0.0f;

    for (auto& mode : modes)
    {
        const float feedback =
            2.0f
            * mode.radius
            * mode.cosine
            * mode.y1
            - mode.radius
              * mode.radius
              * mode.y2;

        const float y0 =
            feedback
            + mode.drive * excitation;

        mode.y2 = mode.y1;
        mode.y1 = juce::jlimit(-1.5f, 1.5f, y0);

        body += mode.y1 * mode.outputGain;
    }

    // The shell colors the drum rather than becoming a second instrument.
    const float shellAmount =
        0.075f + 0.17f * shell01;

    const float coupled =
        membraneSample
        + body * shellAmount
        + contact * (0.012f + 0.030f * shell01);

    return std::tanh(coupled * 0.92f) * 0.82f;
}
