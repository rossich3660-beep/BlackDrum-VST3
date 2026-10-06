#pragma once
#include <JuceHeader.h>
#include "AutoSettings.h"

class SampleAnalyzer
{
public:
    static SampleFeatures analyze(const juce::AudioBuffer<float>& mono, double sampleRate);
    static juce::AudioBuffer<float> makeAnalysisCopy(const juce::AudioBuffer<float>& source, double sourceRate);
};
