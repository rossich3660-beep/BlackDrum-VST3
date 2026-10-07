#pragma once
#include <JuceHeader.h>
#include <array>

class SnareWireModel
{
public:
    static constexpr int NumWires = 10;
    static constexpr int NumStringModes = 4;

    void prepare(double sampleRate);
    void reset();

    void setParameters(float snareTension01,
                       float contact01,
                       float damping01);

    void trigger(float impactEnergy, float velocity01);

    float processSample(float bottomDisplacement,
                        float bottomVelocity);

private:
    struct Mode
    {
        float displacement = 0.0f;
        float velocity = 0.0f;
        float omega = 0.0f;
        float damping = 0.0f;
        float weight = 0.0f;
    };

    struct Wire
    {
        std::array<Mode, NumStringModes> modes {};
        float gap = 0.0f;
        float spring = 0.0f;
        float contactDamping = 0.0f;
        float roughness = 0.0f;
        float contactState = 0.0f;
        float previousRelativeVelocity = 0.0f;
        float previousForce = 0.0f;
        float outputGain = 0.0f;
    };

    float randomBipolar() noexcept;
    void updateWires();

    std::array<Wire, NumWires> wires {};

    double sampleRate = 44100.0;

    float snareTension01 = 0.65f;
    float contact01 = 0.75f;
    float damping01 = 0.40f;

    float triggerEnergy = 0.0f;
    float triggerDecayPerSample = 0.97f;

    float previousHeadVelocity = 0.0f;
    unsigned int randomState = 0xA341316Cu;

    static constexpr std::array<float, NumWires> tensionScale {{
        0.92f, 1.01f, 0.97f, 1.04f, 0.95f,
        1.02f, 0.99f, 1.06f, 0.94f, 1.03f
    }};

    static constexpr std::array<float, NumWires> gapScale {{
        0.92f, 1.04f, 0.97f, 1.08f, 0.95f,
        1.02f, 1.06f, 0.93f, 1.01f, 1.07f
    }};
};
