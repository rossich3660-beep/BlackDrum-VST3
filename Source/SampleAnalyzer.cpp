#include "SampleAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

namespace
{
constexpr int fftSize = 4096;
constexpr float eps = 1.0e-12f;

float finiteOrZero(float v) noexcept
{
    return std::isfinite(v) ? v : 0.0f;
}

float rmsWindow(const juce::AudioBuffer<float>& b, int start, int count)
{
    if (b.getNumSamples() <= 0 || count <= 0 || start >= b.getNumSamples())
        return 0.0f;
    const int end = juce::jmin(b.getNumSamples(), start + count);
    double sum = 0.0;
    for (int i = start; i < end; ++i)
    {
        const float x = b.getSample(0, i);
        sum += (double)x * (double)x;
    }
    return (float)std::sqrt(sum / (double)juce::jmax(1, end - start));
}

float bandEnergy(const std::vector<float>& power, double sampleRate, float lo, float hi)
{
    if (power.empty() || sampleRate <= 0.0)
        return 0.0f;
    const float binHz = (float)sampleRate / (float)fftSize;
    const int first = juce::jlimit(0, (int)power.size() - 1, (int)std::floor(lo / binHz));
    const int last = juce::jlimit(first, (int)power.size() - 1, (int)std::ceil(hi / binHz));
    double sum = 0.0;
    for (int k = first; k <= last; ++k)
        sum += power[(size_t)k];
    return (float)sum;
}
}

juce::AudioBuffer<float> SampleAnalyzer::makeAnalysisCopy(const juce::AudioBuffer<float>& source, double sourceRate)
{
    if (source.getNumSamples() <= 0 || source.getNumChannels() <= 0 || sourceRate <= 0.0)
        return {};

    juce::AudioBuffer<float> mono(1, source.getNumSamples());
    auto* dst = mono.getWritePointer(0);
    const int channels = source.getNumChannels();
    for (int i = 0; i < source.getNumSamples(); ++i)
    {
        double sum = 0.0;
        for (int ch = 0; ch < channels; ++ch)
            sum += source.getSample(ch, i);
        dst[i] = (float)(sum / (double)channels);
    }

    float peak = 0.0f;
    for (int i = 0; i < mono.getNumSamples(); ++i)
        peak = juce::jmax(peak, std::abs(dst[i]));
    if (peak > eps)
        mono.applyGain(1.0f / peak);

    const int targetSamples = juce::jmax(1, (int)std::llround((double)mono.getNumSamples() * 44100.0 / sourceRate));
    if (sourceRate < 44099.5 || sourceRate > 44100.5)
    {
        juce::AudioBuffer<float> resampled(1, targetSamples);
        juce::LagrangeInterpolator interp;
        interp.reset();
        interp.process(sourceRate / 44100.0, mono.getReadPointer(0), resampled.getWritePointer(0), targetSamples);
        return resampled;
    }
    return mono;
}

SampleFeatures SampleAnalyzer::analyze(const juce::AudioBuffer<float>& mono, double sampleRate)
{
    SampleFeatures f;
    if (mono.getNumChannels() <= 0 || mono.getNumSamples() <= 0 || sampleRate <= 0.0)
        return f;

    const int n = mono.getNumSamples();
    const int rmsWindowSamples = juce::jmax(1, (int)std::llround(sampleRate * 0.002));
    const int rmsStepSamples = juce::jmax(1, (int)std::llround(sampleRate * 0.001));
    const float peakDb = -0.0001f;
    const float threshold40 = std::pow(10.0f, -40.0f / 20.0f);
    const float threshold60 = std::pow(10.0f, -60.0f / 20.0f);

    int peakIndex = 0;
    float peak = 0.0f;
    double totalSq = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float x = mono.getSample(0, i);
        if (!std::isfinite(x))
            return SampleFeatures{};
        const float ax = std::abs(x);
        if (ax > peak) { peak = ax; peakIndex = i; }
        totalSq += (double)x * (double)x;
    }
    if (peak <= eps || totalSq <= eps)
        return f;

    int endIndex = n - 1;
    for (int i = peakIndex; i < n; i += rmsStepSamples)
    {
        if (rmsWindow(mono, i, rmsWindowSamples) < threshold60)
        {
            endIndex = i;
            break;
        }
    }
    f.durationMs = (float)(1000.0 * (double)endIndex / sampleRate);

    int attackStart = 0;
    for (int i = 0; i <= peakIndex; i += rmsStepSamples)
    {
        if (rmsWindow(mono, i, rmsWindowSamples) >= threshold40)
        {
            attackStart = i;
            break;
        }
    }
    f.attackMs = (float)(1000.0 * (double)juce::jmax(0, peakIndex - attackStart) / sampleRate);

    const int peakRmsCount = juce::jmax(1, (int)std::llround(sampleRate * 0.010));
    const float peakRms = juce::jmax(eps, rmsWindow(mono, juce::jmax(0, peakIndex - peakRmsCount / 2), peakRmsCount));
    const float crest = peak / peakRms;
    f.crestFactorDb = crest > eps ? (float)(20.0 * std::log10(crest)) : 0.0f;

    const int totalTailStart = juce::jmin(n, (int)std::llround(sampleRate * 0.150));
    double tailSq = 0.0;
    for (int i = totalTailStart; i < n; ++i)
    {
        const float x = mono.getSample(0, i);
        tailSq += (double)x * (double)x;
    }
    f.tailEnergyRatio = autoClamp((float)(tailSq / totalSq));

    float t60Start = rmsWindow(mono, peakIndex, rmsWindowSamples);
    t60Start = juce::jmax(t60Start, threshold40);
    int t60Index = endIndex;
    const float target = t60Start * 0.001f;
    for (int i = peakIndex; i < endIndex; i += rmsStepSamples)
    {
        if (rmsWindow(mono, i, rmsWindowSamples) <= target)
        {
            t60Index = i;
            break;
        }
    }
    f.decayT60Ms = (float)(1000.0 * (double)juce::jmax(0, t60Index - peakIndex) / sampleRate);

    juce::dsp::FFT fft(12);
    juce::HeapBlock<float> fftData((size_t)fftSize * 2, true);
    juce::dsp::WindowingFunction<float> hann(fftSize, juce::dsp::WindowingFunction<float>::hann);

    std::vector<float> avgPower(fftSize / 2 + 1, 0.0f);
    std::vector<float> first80Power(fftSize / 2 + 1, 0.0f);
    int framesFirst = 0, framesTail = 0;
    const int firstEnd = juce::jmin(n, (int)std::llround(sampleRate * 0.080));
    const int tailEnd = juce::jmin(n, (int)std::llround(sampleRate * 0.300));
    const int hop = fftSize / 2;

    auto accumulateFrame = [&](int start, std::vector<float>& target, int& frameCount)
    {
        std::fill(fftData.get(), fftData.get() + fftSize * 2, 0.0f);
        const int count = juce::jmin(fftSize, n - start);
        if (count <= 0) return;
        std::copy(mono.getReadPointer(0, start), mono.getReadPointer(0, start) + count, fftData.get());
        hann.multiplyWithWindowingTable(fftData.get(), fftSize);
        fft.performRealOnlyForwardTransform(fftData.get());
        for (int k = 0; k <= fftSize / 2; ++k)
        {
            float re = 0.0f, im = 0.0f;
            if (k == 0) re = fftData[0];
            else if (k == fftSize / 2) re = fftData[1];
            else { re = fftData[2 * k]; im = fftData[2 * k + 1]; }
            const float p = re * re + im * im;
            target[(size_t)k] += std::isfinite(p) ? p : 0.0f;
        }
        ++frameCount;
    };

    for (int start = 0; start < firstEnd; start += hop)
        accumulateFrame(start, first80Power, framesFirst);
    for (int start = (int)std::llround(sampleRate * 0.080); start < tailEnd; start += hop)
        accumulateFrame(start, avgPower, framesTail);

    if (framesTail == 0)
    {
        avgPower = first80Power;
        framesTail = framesFirst;
    }
    if (framesFirst > 0)
        for (auto& v : first80Power) v /= (float)framesFirst;
    if (framesTail > 0)
        for (auto& v : avgPower) v /= (float)framesTail;

    double weighted = 0.0, totalPower = 0.0;
    for (int k = 1; k <= fftSize / 2; ++k)
    {
        const double hz = (double)k * sampleRate / (double)fftSize;
        const double p = first80Power[(size_t)k];
        weighted += hz * p;
        totalPower += p;
    }
    f.centroidHz = totalPower > eps ? (float)(weighted / totalPower) : 0.0f;

    const float all = juce::jmax(eps, bandEnergy(first80Power, sampleRate, 20.0f, 20000.0f));
    f.bodyEnergy = autoClamp(bandEnergy(first80Power, sampleRate, 120.0f, 350.0f) / all);
    f.snapEnergy = autoClamp(bandEnergy(first80Power, sampleRate, 2000.0f, 5000.0f) / all);
    f.airEnergy = autoClamp(bandEnergy(first80Power, sampleRate, 6000.0f, 20000.0f) / all);

    int fundamentalBin = -1;
    float best = 0.0f;
    const int firstBin = juce::jmax(1, (int)std::floor(80.0 * fftSize / sampleRate));
    const int lastBin = juce::min(fftSize / 2 - 1, (int)std::ceil(400.0 * fftSize / sampleRate));
    for (int k = firstBin; k <= lastBin; ++k)
    {
        if (first80Power[(size_t)k] > best)
        {
            best = first80Power[(size_t)k];
            fundamentalBin = k;
        }
    }
    if (fundamentalBin > 0 && best > totalPower * 0.002)
    {
        const float y0 = first80Power[(size_t)juce::jmax(0, fundamentalBin - 1)];
        const float y1 = first80Power[(size_t)fundamentalBin];
        const float y2 = first80Power[(size_t)juce::jmin(fftSize / 2, fundamentalBin + 1)];
        const float denom = y0 - 2.0f * y1 + y2;
        const float offset = std::abs(denom) > eps ? 0.5f * (y0 - y2) / denom : 0.0f;
        f.fundamentalHz = autoClamp(juce::jlimit(80.0f, 400.0f, (float)((fundamentalBin + offset) * sampleRate / fftSize) / 400.0f)) * 400.0f;
    }

    double logSum = 0.0, arithmetic = 0.0;
    int flatCount = 0;
    for (int k = firstBin; k <= lastBin; ++k)
    {
        const float p = juce::jmax(eps, first80Power[(size_t)k]);
        logSum += std::log(p);
        arithmetic += p;
        ++flatCount;
    }
    const float flatness = (flatCount > 0 && arithmetic > eps)
        ? (float)std::exp(logSum / (double)flatCount) / ((float)arithmetic / (float)flatCount)
        : 1.0f;
    f.toneToNoise = autoClamp(1.0f - flatness);

    std::vector<float> band;
    for (int k = juce::jmax(1, (int)std::floor(200.0 * fftSize / sampleRate));
         k <= juce::min(fftSize / 2, (int)std::ceil(1500.0 * fftSize / sampleRate)); ++k)
        band.push_back(first80Power[(size_t)k]);
    if (!band.empty())
    {
        std::sort(band.begin(), band.end());
        const float median = juce::jmax(eps, band[band.size() / 2]);
        const float maxPeak = *std::max_element(band.begin(), band.end());
        f.ringAmount = autoClamp((maxPeak / median - 1.0f) / 20.0f);
    }

    f.durationMs = finiteOrZero(f.durationMs);
    f.attackMs = finiteOrZero(f.attackMs);
    f.decayT60Ms = finiteOrZero(f.decayT60Ms);
    f.centroidHz = finiteOrZero(f.centroidHz);
    f.fundamentalHz = finiteOrZero(f.fundamentalHz);
    f.crestFactorDb = finiteOrZero(f.crestFactorDb);
    return f;
}
