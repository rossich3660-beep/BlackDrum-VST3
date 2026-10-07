#include "PluginEditor.h"

PhysicalSnareAudioProcessorEditor::PhysicalSnareAudioProcessorEditor(
    PhysicalSnareAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(820, 450);
    setResizable(false, false);

    title.setText("PHYSICAL SNARE", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    stageLabel.setText("STAGE 3", juce::dontSendNotification);
    stageLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    stageLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff9d32));
    addAndMakeVisible(stageLabel);

    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setFont(juce::Font(juce::FontOptions(13.0f)));
    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffbdbdbd));
    addAndMakeVisible(statusLabel);

    styleSlider(tuneSlider);
    styleSlider(dampingSlider);
    styleSlider(hitPositionSlider);
    styleSlider(levelSlider);
    styleSlider(bottomTuneSlider);
    styleSlider(airSlider);

    tuneSlider.setRange(90.0, 360.0, 0.1);
    tuneSlider.setDoubleClickReturnValue(true, 185.0);

    dampingSlider.setRange(0.0, 1.0, 0.001);
    dampingSlider.setDoubleClickReturnValue(true, 0.40);

    hitPositionSlider.setRange(0.0, 1.0, 0.001);
    hitPositionSlider.setDoubleClickReturnValue(true, 0.35);

    levelSlider.setRange(0.0, 1.0, 0.001);
    levelSlider.setDoubleClickReturnValue(true, 0.75);

    bottomTuneSlider.setRange(90.0, 360.0, 0.1);
    bottomTuneSlider.setDoubleClickReturnValue(true, 170.0);

    airSlider.setRange(0.0, 1.0, 0.001);
    airSlider.setDoubleClickReturnValue(true, 0.35);

    for (auto* slider : {
        &tuneSlider, &dampingSlider, &hitPositionSlider,
        &levelSlider, &bottomTuneSlider, &airSlider })
    {
        addAndMakeVisible(*slider);
    }

    auto& state = processor.getParameters();

    tuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "TUNE", tuneSlider);
    dampingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "DAMP", dampingSlider);
    hitPositionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "HITPOS", hitPositionSlider);
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "LEVEL", levelSlider);
    bottomTuneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "BOTTOM", bottomTuneSlider);
    airAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        state, "AIR", airSlider);

    startTimerHz(30);
    timerCallback();
}

void PhysicalSnareAudioProcessorEditor::styleSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(
        juce::Slider::TextBoxBelow,
        false,
        58,
        18);

    slider.setNumDecimalPlacesToDisplay(2);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffff9d32));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffffc067));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff1b1b1b));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff444444));
}

void PhysicalSnareAudioProcessorEditor::timerCallback()
{
    const int note = processor.getLastMidiNote();
    const int velocity = processor.getLastMidiVelocity();

    const juce::String noteText =
        note >= 0
            ? juce::MidiMessage::getMidiNoteName(note, true, true, 4)
            : "-";

    statusLabel.setText(
        noteText + "  V" + juce::String(velocity),
        juce::dontSendNotification);

    repaint();
}

void PhysicalSnareAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101010));

    const auto panel = getLocalBounds().reduced(14).toFloat();

    g.setColour(juce::Colour(0xff171717));
    g.fillRoundedRectangle(panel, 14.0f);

    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(panel, 14.0f, 1.0f);

    const float cx = 410.0f;
    const float topY = 115.0f;
    const float bottomY = 255.0f;

    g.setColour(juce::Colour(0xff212121));
    g.fillEllipse(cx - 155.0f, topY - 32.0f, 310.0f, 64.0f);
    g.fillEllipse(cx - 155.0f, bottomY - 32.0f, 310.0f, 64.0f);

    g.setColour(juce::Colour(0xff6c6c6c));
    g.drawEllipse(cx - 155.0f, topY - 32.0f, 310.0f, 64.0f, 1.4f);
    g.drawEllipse(cx - 155.0f, bottomY - 32.0f, 310.0f, 64.0f, 1.4f);

    g.setColour(juce::Colour(0xff343434));
    g.drawLine(cx - 120.0f, topY + 20.0f, cx + 120.0f, topY + 20.0f, 1.0f);
    g.drawLine(cx - 120.0f, bottomY - 20.0f, cx + 120.0f, bottomY - 20.0f, 1.0f);

    g.setColour(juce::Colour(0xff2b2b2b));
    g.fillRoundedRectangle(
        cx - 120.0f,
        topY + 31.0f,
        240.0f,
        bottomY - topY - 62.0f,
        8.0f);

    const float hitX =
        cx - 118.0f + 236.0f * static_cast<float>(
            processor.getParameters().getRawParameterValue("HITPOS")->load());

    g.setColour(juce::Colour(0xffff9d32));
    g.fillEllipse(hitX - 6.0f, topY - 6.0f, 12.0f, 12.0f);

    const char* labels[] = { "TUNE", "DAMP", "HIT", "LEVEL", "BOT", "AIR" };
    juce::Slider* sliders[] = {
        &tuneSlider, &dampingSlider, &hitPositionSlider,
        &levelSlider, &bottomTuneSlider, &airSlider
    };

    for (int i = 0; i < 6; ++i)
    {
        const auto bounds = sliders[i]->getBounds();

        g.setColour(juce::Colour(0xffc0c0c0));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawFittedText(
            labels[i],
            bounds.getX(),
            bounds.getY() - 18,
            bounds.getWidth(),
            14,
            juce::Justification::centred,
            1);
    }
}

void PhysicalSnareAudioProcessorEditor::resized()
{
    title.setBounds(28, 22, 230, 30);
    stageLabel.setBounds(270, 24, 80, 24);
    statusLabel.setBounds(630, 22, 150, 26);

    const int y = 342;
    const int w = 92;
    const int h = 80;
    const int gap = 18;
    const int startX = 74;

    tuneSlider.setBounds(startX + 0 * (w + gap), y, w, h);
    dampingSlider.setBounds(startX + 1 * (w + gap), y, w, h);
    hitPositionSlider.setBounds(startX + 2 * (w + gap), y, w, h);
    levelSlider.setBounds(startX + 3 * (w + gap), y, w, h);
    bottomTuneSlider.setBounds(startX + 4 * (w + gap), y, w, h);
    airSlider.setBounds(startX + 5 * (w + gap), y, w, h);
}
