#include "PluginProcessor.h"
#include "PluginEditor.h"

PhysicalSnareAudioProcessor::PhysicalSnareAudioProcessor()
    : AudioProcessor(
        BusesProperties()
            .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(
          *this,
          nullptr,
          "PARAMETERS",
          createParameterLayout())
{
    tuningParameter = parameters.getRawParameterValue("TUNE");
    dampingParameter = parameters.getRawParameterValue("DAMP");
    hitPositionParameter = parameters.getRawParameterValue("HITPOS");
    levelParameter = parameters.getRawParameterValue("LEVEL");
    bottomTuneParameter = parameters.getRawParameterValue("BOTTOM");
    airCouplingParameter = parameters.getRawParameterValue("AIR");
}

juce::AudioProcessorValueTreeState::ParameterLayout
PhysicalSnareAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "TUNE", "Tune",
        juce::NormalisableRange<float>(90.0f, 360.0f, 0.1f), 185.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "DAMP", "Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.40f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "HITPOS", "Hit",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "LEVEL", "Level",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.75f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "BOTTOM", "Bottom",
        juce::NormalisableRange<float>(90.0f, 360.0f, 0.1f), 170.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "AIR", "Air",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.35f));

    return { params.begin(), params.end() };
}

void PhysicalSnareAudioProcessor::prepareToPlay(double sampleRate, int)
{
    membrane.prepare(sampleRate > 1000.0 ? sampleRate : 44100.0);

    membrane.setParameters(
        tuningParameter != nullptr ? tuningParameter->load() : 185.0f,
        dampingParameter != nullptr ? dampingParameter->load() : 0.40f,
        hitPositionParameter != nullptr ? hitPositionParameter->load() : 0.35f,
        bottomTuneParameter != nullptr ? bottomTuneParameter->load() : 170.0f,
        airCouplingParameter != nullptr ? airCouplingParameter->load() : 0.35f);

    lastMidiNote.store(-1, std::memory_order_relaxed);
    lastMidiVelocity.store(0, std::memory_order_relaxed);
}

void PhysicalSnareAudioProcessor::releaseResources()
{
    membrane.reset();
}

void PhysicalSnareAudioProcessor::processBlock(
    juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    membrane.setParameters(
        tuningParameter != nullptr ? tuningParameter->load() : 185.0f,
        dampingParameter != nullptr ? dampingParameter->load() : 0.40f,
        hitPositionParameter != nullptr ? hitPositionParameter->load() : 0.35f,
        bottomTuneParameter != nullptr ? bottomTuneParameter->load() : 170.0f,
        airCouplingParameter != nullptr ? airCouplingParameter->load() : 0.35f);

    auto event = midiMessages.cbegin();
    const auto end = midiMessages.cend();

    const float outputLevel =
        levelParameter != nullptr
            ? juce::jlimit(0.0f, 1.0f, levelParameter->load())
            : 0.75f;

    for (int sampleIndex = 0; sampleIndex < buffer.getNumSamples(); ++sampleIndex)
    {
        while (event != end && (*event).samplePosition <= sampleIndex)
        {
            const auto message = (*event).getMessage();

            if (message.isNoteOn())
            {
                lastMidiNote.store(message.getNoteNumber(), std::memory_order_relaxed);

                const int velocity = juce::jlimit(
                    0,
                    127,
                    juce::roundToInt(message.getFloatVelocity() * 127.0f));

                lastMidiVelocity.store(velocity, std::memory_order_relaxed);
                membrane.trigger(message.getFloatVelocity());
            }
            else if (message.isNoteOff())
            {
                lastMidiNote.store(-1, std::memory_order_relaxed);
            }

            ++event;
        }

        const float sample = membrane.processSample() * outputLevel;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sampleIndex, sample);
    }
}

juce::AudioProcessorEditor* PhysicalSnareAudioProcessor::createEditor()
{
    return new PhysicalSnareAudioProcessorEditor(*this);
}

void PhysicalSnareAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());

    if (xml != nullptr)
        copyXmlToBinary(*xml, destData);
}

void PhysicalSnareAudioProcessor::setStateInformation(
    const void* data,
    int sizeInBytes)
{
    if (data == nullptr || sizeInBytes <= 0)
        return;

    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PhysicalSnareAudioProcessor();
}
