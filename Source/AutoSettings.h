#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>

struct SampleFeatures
{
    float durationMs = 0.0f;
    float attackMs = 0.0f;
    float decayT60Ms = 0.0f;
    float tailEnergyRatio = 0.0f;
    float centroidHz = 0.0f;
    float bodyEnergy = 0.0f;
    float snapEnergy = 0.0f;
    float airEnergy = 0.0f;
    float fundamentalHz = 0.0f;
    float toneToNoise = 0.0f;
    float ringAmount = 0.0f;
    float crestFactorDb = 0.0f;
};

struct AutoSettings
{
    float brightnessAmt = 0.5f;
    float snapAmt = 0.5f;
    float noiseLayerAmt = 0.5f;
    float attackSoftening = 0.5f;
    float tailShorten = 0.5f;
    float pitchDropAmt = 0.1f;
    float saturationAmt = 0.3f;
    float bodyBoostAmt = 0.5f;
    float velocityCurve = 0.4f;
};

struct AutoSettingCoefficients
{
    float brightnessBase = 0.8f, brightnessAir = 1.5f;
    float snapBase = 0.9f, snapEnergy = 2.0f;
    float noiseBase = 0.7f, noiseTone = 0.8f;
    float attackLow = 0.5f, attackHigh = 6.0f, attackOutHigh = 0.7f, attackOutLow = 0.2f;
    float tailBase = 0.3f, tailRatio = 0.7f;
    float pitchTone = 0.25f, pitchNoTone = 0.05f;
    float satLow = 6.0f, satHigh = 20.0f, satOutHigh = 0.6f, satOutLow = 0.15f;
    float bodyBase = 0.8f, bodyEnergy = 1.5f;
    float velocityBase = 0.4f, velocityHighCentroid = 0.3f;
};

inline constexpr AutoSettingCoefficients kAutoSettingCoefficients {};

inline float autoClamp(float v) noexcept
{
    return juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f);
}

inline AutoSettings makeAutoSettings(const SampleFeatures& f) noexcept
{
    AutoSettings s;
    s.brightnessAmt = autoClamp(kAutoSettingCoefficients.brightnessBase - kAutoSettingCoefficients.brightnessAir * f.airEnergy);
    s.snapAmt = autoClamp(kAutoSettingCoefficients.snapBase - kAutoSettingCoefficients.snapEnergy * f.snapEnergy);
    s.noiseLayerAmt = autoClamp(kAutoSettingCoefficients.noiseBase - kAutoSettingCoefficients.noiseTone * (1.0f - f.toneToNoise));
    const float attack = juce::jlimit(kAutoSettingCoefficients.attackLow, kAutoSettingCoefficients.attackHigh, f.attackMs);
    const float attackT = (attack - kAutoSettingCoefficients.attackLow) / (kAutoSettingCoefficients.attackHigh - kAutoSettingCoefficients.attackLow);
    s.attackSoftening = autoClamp(juce::jmap(attackT, 0.0f, 1.0f, kAutoSettingCoefficients.attackOutHigh, kAutoSettingCoefficients.attackOutLow));
    s.tailShorten = autoClamp(kAutoSettingCoefficients.tailBase + kAutoSettingCoefficients.tailRatio * f.tailEnergyRatio);
    s.pitchDropAmt = autoClamp(f.fundamentalHz > 0.0f ? kAutoSettingCoefficients.pitchTone * f.toneToNoise : kAutoSettingCoefficients.pitchNoTone);
    const float crest = juce::jlimit(kAutoSettingCoefficients.satLow, kAutoSettingCoefficients.satHigh, f.crestFactorDb);
    const float crestT = (crest - kAutoSettingCoefficients.satLow) / (kAutoSettingCoefficients.satHigh - kAutoSettingCoefficients.satLow);
    s.saturationAmt = autoClamp(juce::jmap(crestT, 0.0f, 1.0f, kAutoSettingCoefficients.satOutHigh, kAutoSettingCoefficients.satOutLow));
    s.bodyBoostAmt = autoClamp(kAutoSettingCoefficients.bodyBase - kAutoSettingCoefficients.bodyEnergy * f.bodyEnergy);
    s.velocityCurve = autoClamp(kAutoSettingCoefficients.velocityBase + kAutoSettingCoefficients.velocityHighCentroid * (f.centroidHz > 4000.0f ? 1.0f : 0.0f));
    return s;
}
