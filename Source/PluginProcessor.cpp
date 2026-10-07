#include "PluginProcessor.h"
#include "PluginEditor.h"

PhysicalSnareAudioProcessor::PhysicalSnareAudioProcessor()
    : AudioProcessor(
        BusesProperties()
            .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

void PhysicalSnareAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    currentBlockSize = juce::jmax(0, samplesPerBlock);

    lastMidiNote.store(-1, std::memory_order_relaxed);
    lastMidiVelocity.store(0, std::memory_order_relaxed);
    midiEventCount.store(0, std::memory_order_relaxed);
}

void PhysicalSnareAudioProcessor::releaseResources()
{
    currentBlockSize = 0;
}

void PhysicalSnareAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Stage 1 intentionally produces silence.
    // This guarantees that MIDI reception and plugin lifecycle can be
    // validated before any physical-model DSP is introduced.
    buffer.clear();

    for (const auto metadata : midiMessages)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
        {
            const int note = message.getNoteNumber();
            const int velocity = juce::jlimit(
                0, 127,
                (int) juce::roundToInt(message.getFloatVelocity() * 127.0f));

            lastMidiNote.store(note, std::memory_order_relaxed);
            lastMidiVelocity.store(velocity, std::memory_order_relaxed);
            midiEventCount.fetch_add(1, std::memory_order_relaxed);
        }
        else if (message.isNoteOff())
        {
            lastMidiNote.store(-1, std::memory_order_relaxed);
        }
    }
}

juce::AudioProcessorEditor* PhysicalSnareAudioProcessor::createEditor()
{
    return new PhysicalSnareAudioProcessorEditor(*this);
}

void PhysicalSnareAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);
    stream.writeDouble(currentSampleRate);
}

void PhysicalSnareAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    juce::MemoryInputStream stream(data, static_cast<size_t>(sizeInBytes), false);
    if (stream.getTotalLength() >= sizeof(double))
        currentSampleRate = stream.readDouble();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhysicalSnareAudioProcessor();
}
