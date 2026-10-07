#include "PluginEditor.h"

PhysicalSnareAudioProcessorEditor::PhysicalSnareAudioProcessorEditor(
    PhysicalSnareAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(900, 520);
    setResizable(false, false);

    title.setText("PHYSICAL SNARE", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(30.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    stageLabel.setText("STAGE 2  -  BATTER MEMBRANE", juce::dontSendNotification);
    stageLabel.setFont(juce::Font(juce::FontOptions(15.0f)));
    stageLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff9d32));
    addAndMakeVisible(stageLabel);

    midiLabel.setJustificationType(juce::Justification::centred);
    midiLabel.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    midiLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(midiLabel);

    velocityLabel.setJustificationType(juce::Justification::centred);
    velocityLabel.setFont(juce::Font(juce::FontOptions(16.0f)));
    velocityLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(velocityLabel);

    eventsLabel.setJustificationType(juce::Justification::centred);
    eventsLabel.setFont(juce::Font(juce::FontOptions(14.0f)));
    eventsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffbdbdbd));
    addAndMakeVisible(eventsLabel);

    infoLabel.setText(
        "Linear modal membrane: 12 modes, velocity excitation, spatial hit position",
        juce::dontSendNotification);
    infoLabel.setJustificationType(juce::Justification::centred);
    infoLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    infoLabel.setColour(juce::Label::textColourId, juce::Colour(0xff929292));
    addAndMakeVisible(infoLabel);

    styleSlider(tuneSlider, " Hz");
    tuneSlider.setRange(90.0, 360.0, 0.1);
    tuneSlider.setDoubleClickReturnValue(true, 185.0);
    addAndMakeVisible(tuneSlider);

    styleSlider(dampingSlider, "");
    dampingSlider.setRange(0.0, 1.0, 0.001);
    dampingSlider.setNumDecimalPlacesToDisplay(2);
    dampingSlider.setDoubleClickReturnValue(true, 0.40);
    addAndMakeVisible(dampingSlider);

    styleSlider(hitPositionSlider, "");
    hitPositionSlider.setRange(0.0, 1.0, 0.001);
    hitPositionSlider.setNumDecimalPlacesToDisplay(2);
    hitPositionSlider.setDoubleClickReturnValue(true, 0.35);
    addAndMakeVisible(hitPositionSlider);

    styleSlider(levelSlider, "");
    levelSlider.setRange(0.0, 1.0, 0.001);
    levelSlider.setNumDecimalPlacesToDisplay(2);
    levelSlider.setDoubleClickReturnValue(true, 0.75);
    addAndMakeVisible(levelSlider);

    auto& state = processor.getParameters();

    tuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "TUNE", tuneSlider);
    dampingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "DAMP", dampingSlider);
    hitPositionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "HITPOS", hitPositionSlider);
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "LEVEL", levelSlider);

    startTimerHz(30);
    timerCallback();
}

void PhysicalSnareAudioProcessorEditor::styleSlider(juce::Slider& slider, const juce::String& suffix)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        90,
        22);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff9d32));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffc067));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff202020));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff505050));

    if (suffix.isNotEmpty())
        slider.setTextValueSuffix(suffix);
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
        midiLabel.setText("MIDI NOTE  -", juce::dontSendNotification);
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

    const auto panel = getLocalBounds().reduced(18).toFloat();

    g.setColour(juce::Colour(0xff171717));
    g.fillRoundedRectangle(panel, 16.0f);

    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(panel, 16.0f, 1.0f);

    const auto head = juce::Rectangle<float>(250.0f, 95.0f, 370.0f, 260.0f);

    g.setColour(juce::Colour(0xff0f0f0f));
    g.fillRoundedRectangle(head, 18.0f);

    g.setColour(juce::Colour(0xff5d5d5d));
    g.drawRoundedRectangle(head, 18.0f, 1.5f);

    const auto centreX = head.getCentreX();
    const auto centreY = head.getCentreY();

    for (int i = 0; i < 5; ++i)
    {
        const float inset = 18.0f + (float) i * 22.0f;
        g.setColour(i == 0 ? juce::Colour(0xff7a7a7a) : juce::Colour(0xff353535));
        g.drawEllipse(
            centreX - 92.0f + inset,
            centreY - 62.0f + inset * 0.42f,
            184.0f - inset * 0.8f,
            124.0f - inset * 0.38f,
            i == 0 ? 1.5f : 1.0f);
    }

    const float x = centreX - 92.0f + 184.0f * hitPositionSlider.getValue();
    const float y = centreY;

    g.setColour(juce::Colour(0xffff9d32));
    g.fillEllipse(x - 6.0f, y - 6.0f, 12.0f, 12.0f);

    g.setColour(juce::Colour(0xffff9d32).withAlpha(0.35f));
    g.drawEllipse(x - 15.0f, y - 15.0f, 30.0f, 30.0f, 1.0f);

    g.setColour(juce::Colour(0xff777777));
    g.drawLine(250.0f, 375.0f, 620.0f, 375.0f, 1.0f);
}

void PhysicalSnareAudioProcessorEditor::resized()
{
    title.setBounds(42, 28, 340, 40);
    stageLabel.setBounds(42, 70, 330, 24);

    infoLabel.setBounds(200, 50, 500, 24);

    const int y = 375;
    tuneSlider.setBounds(90, y, 160, 115);
    dampingSlider.setBounds(265, y, 160, 115);
    hitPositionSlider.setBounds(440, y, 160, 115);
    levelSlider.setBounds(615, y, 160, 115);

    midiLabel.setBounds(100, 325, 700, 28);
    velocityLabel.setBounds(100, 350, 700, 26);
    eventsLabel.setBounds(100, 405, 700, 24);
}
