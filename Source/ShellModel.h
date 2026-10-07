#pragma once
#include <JuceHeader.h>
#include <array>

class SnareShellModel
{
public:
    static constexpr int NumModes = 8;

    void prepare(double sampleRate);
    void reset();

    void setParameters(float topTuningHz,
                       float bottomTuningHz,
                       float damping01,
                       float shell01,
                       float depth01);

    void trigger(float impactEnergy, float velocity01);
    float processSample(float membraneSample);

private:
    struct Mode
    {
        float y1 = 0.0f;
        float y2 = 0.0f;
        float cosine = 1.0f;
        float radius = 0.99f;
        float drive = 0.0f;
        float outputGain = 0.0f;
    };

    void updateModes();

    std::array<Mode, NumModes> modes {};

    double sampleRate = 44100.0;

    float topTuningHz = 185.0f;
    float bottomTuningHz = 170.0f;
    float damping01 = 0.40f;
    float shell01 = 0.55f;
    float depth01 = 0.45f;

    float impactState = 0.0f;
    float impactDecayPerSample = 0.98f;
    float previousExcitation = 0.0f;

    static constexpr std::array<float, NumModes> modeRatios {{
        0.72f, 1.00f, 1.27f, 1.58f, 1.94f, 2.38f, 2.92f, 3.55f
    }};
};
