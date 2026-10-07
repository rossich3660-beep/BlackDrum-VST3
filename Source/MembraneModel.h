#pragma once
#include <JuceHeader.h>
#include <array>

class SnareMembraneModel
{
public:
    static constexpr int NumModes = 12;
    static constexpr int MaxVoices = 8;

    void prepare(double sampleRate);
    void reset();

    void setParameters(float topTuningHz,
                       float damping01,
                       float hitPosition01,
                       float bottomTuningHz,
                       float airCoupling01);

    void trigger(float velocity01);
    float processSample();

private:
    struct ModeSpec
    {
        int angularOrder;
        int radialOrder;
        float zero;
    };

    struct ModeState
    {
        float phase = 0.0f;
        float amplitude = 0.0f;
        float phaseStep = 0.0f;
        float decayPerSample = 1.0f;
    };

    struct Voice
    {
        std::array<ModeState, NumModes> modes {};
        bool active = false;
        unsigned int age = 0;
    };

    struct Head
    {
        std::array<Voice, MaxVoices> voices {};
    };

    static float besselJn(int order, float x);
    static float besselJ(int order, float x);

    int chooseVoice(Head&) noexcept;
    int findActiveVoice(const Head&) const noexcept;

    float processHead(Head&);
    void configureVoice(Voice&, float tuningHz, float baseDecaySeconds);
    void driveBottomFromAir(float pressure, float coupling);
    void applyTopAirFeedback(float pressure, float coupling);

    Head topHead;
    Head bottomHead;

    double sampleRate = 44100.0;

    float topTuningHz = 185.0f;
    float bottomTuningHz = 170.0f;
    float damping01 = 0.40f;
    float hitPosition01 = 0.35f;
    float airCoupling01 = 0.35f;

    float airDisplacement = 0.0f;
    float airVelocity = 0.0f;

    unsigned int ageCounter = 0;

    static constexpr std::array<ModeSpec, NumModes> modeSpecs {{
        { 0, 1, 2.4048256f },
        { 1, 1, 3.8317060f },
        { 2, 1, 5.1356225f },
        { 0, 2, 5.5200782f },
        { 3, 1, 6.3801618f },
        { 1, 2, 7.0155864f },
        { 4, 1, 7.5883427f },
        { 2, 2, 8.4172449f },
        { 0, 3, 8.6537280f },
        { 5, 1, 8.7714844f },
        { 3, 2, 9.7610235f },
        { 1, 3, 10.173468f }
    }};
};
