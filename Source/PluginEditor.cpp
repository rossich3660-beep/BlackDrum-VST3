#include "PluginEditor.h"

PhysicalSnareAudioProcessorEditor::PhysicalSnareAudioProcessorEditor(
    PhysicalSnareAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(760, 430);
    setResizable(false, false);

    title.setText("PHYSICAL SNARE", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    stageLabel.setText("STAGE 1  •  MIDI ENGINE", juce::dontSendNotification);
    stageLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::plain)));
    stageLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff9d32));
    addAndMakeVisible(stageLabel);

    midiLabel.setJustificationType(juce::Justification::centred);
    midiLabel.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    midiLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(midiLabel);

    velocityLabel.setJustificationType(juce::Justification::centred);
    velocityLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::plain)));
    velocityLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(velocityLabel);

    eventsLabel.setJustificationType(juce::Justification::centred);
    eventsLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::plain)));
    eventsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffbdbdbd));
    addAndMakeVisible(eventsLabel);

    infoLabel.setText(
        "No audio synthesis yet. Stage 2 will add the first physical membrane model.",
        juce::dontSendNotification);
    infoLabel.setJustificationType(juce::Justification::centred);
    infoLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    infoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff929292));
    addAndMakeVisible(infoLabel);

    startTimerHz(30);
    timerCallback();
}

void PhysicalSnareAudioProcessorEditor::timerCallback()
{
    const int note = processor.getLastMidiNote();
    const int velocity = processor.getLastMidiVelocity();

    if (note >= 0)
    {
        midiLabel.setText(
            "MIDI NOTE  " + juce::MidiMessage::getMidiNoteName(note, true, true, 4),
            juce::dontSendNotification);
    }
    else
    {
        midiLabel.setText("MIDI NOTE  —", juce::dontSendNotification);
    }

    velocityLabel.setText(
        "VELOCITY  " + juce::String(velocity),
        juce::dontSendNotification);

    eventsLabel.setText(
        "RECEIVED EVENTS  " + juce::String(processor.getMidiEventCount()),
        juce::dontSendNotification);
}

void PhysicalSnareAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101010));

    auto panel = getLocalBounds().reduced(18).toFloat();
    g.setColour(juce::Colour(0xff181818));
    g.fillRoundedRectangle(panel, 16.0f);

    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(panel, 16.0f, 1.0f);

    auto centre = juce::Rectangle<float>(
        55.0f, 125.0f,
        static_cast<float>(getWidth() - 110),
        205.0f);

    g.setColour(juce::Colour(0xff131313));
    g.fillRoundedRectangle(centre, 14.0f);

    g.setColour(juce::Colour(0xff484848));
    g.drawRoundedRectangle(centre, 14.0f, 1.0f);

    g.setColour(juce::Colour(0xff262626));
    g.fillEllipse(
        getWidth() * 0.5f - 78.0f,
        160.0f,
        156.0f,
        156.0f);

    g.setColour(juce::Colour(0xff8a8a8a));
    g.drawEllipse(
        getWidth() * 0.5f - 78.0f,
        160.0f,
        156.0f,
        156.0f,
        2.0f);

    g.setColour(juce::Colour(0xffff9d32));
    g.fillEllipse(
        getWidth() * 0.5f - 8.0f,
        230.0f,
        16.0f,
        16.0f);
}

void PhysicalSnareAudioProcessorEditor::resized()
{
    title.setBounds(42, 30, 310, 42);
    stageLabel.setBounds(42, 73, 280, 26);

    midiLabel.setBounds(80, 342, 600, 32);
    velocityLabel.setBounds(80, 374, 600, 28);
    eventsLabel.setBounds(80, 399, 600, 24);

    infoLabel.setBounds(70, 118, 620, 28);
}
