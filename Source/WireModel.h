#pragma once
#include <JuceHeader.h>
#include <array>

class SnareWireModel
{
public:
    static constexpr int NumWires = 10;

    void prepare(double sampleRate);
    void reset();

    void setParameters(float snareTension01,
                       float contact01,
                       float damping01);

    void trigger(float impactEnergy, float velocity01);

    float processSample(float bottomDisplacement);

private:
    struct Wire
    {
        float y1 = 0.0f;
        float y2 = 0.0f;
        float cosine = 1.0f;
        float radius = 0.98f;
        float contactState = 0.0f;
        float previousContact = 0.0f;
        float gain = 0.0f;
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

    unsigned int randomState = 0xA341316Cu;

    static constexpr std::array<float, NumWires> wireRatios {{
        0.74f, 0.81f, 0.89f, 0.96f, 1.00f,
        1.06f, 1.13f, 1.21f, 1.29f, 1.38f
    }};

    static constexpr std::array<float, NumWires> wireIrregularity {{
        0.96f, 1.025f, 0.985f, 1.045f, 0.972f,
        1.018f, 1.038f, 0.978f, 1.030f, 0.992f
    }};
};
