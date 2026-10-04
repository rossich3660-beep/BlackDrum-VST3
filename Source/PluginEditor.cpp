#include "PluginEditor.h"

BlackDrumAudioProcessorEditor::BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(900, 570);
    title.setText("BLACKDRUM", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(25.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    filename.setText("No sample loaded", juce::dontSendNotification);
    filename.setJustificationType(juce::Justification::centred);
    filename.setColour(juce::Label::textColourId, juce::Colour(0xffeeeeee));
    addAndMakeVisible(filename);

    hint.setText("Drop an audio file here - WAV / AIFF / FLAC / OGG", juce::dontSendNotification);
    hint.setJustificationType(juce::Justification::centred);
    hint.setColour(juce::Label::textColourId, juce::Colour(0xff929292));
    addAndMakeVisible(hint);

    bodyMixLabel.setText("BODY MIX", juce::dontSendNotification);
    bodyMixLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    bodyMixLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(bodyMixLabel);
    bodyMixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    bodyMixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    bodyMixSlider.setRange(0.0, 1.0, 0.01);
    bodyMixSlider.setValue(processor.getBodyMix(), juce::dontSendNotification);
    bodyMixSlider.setNumDecimalPlacesToDisplay(0);
    bodyMixSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff66bbff));
    bodyMixSlider.onValueChange = [this] { processor.setBodyMix((float)bodyMixSlider.getValue()); };
    addAndMakeVisible(bodyMixSlider);

    spectralLabel.setText("SPECTRAL MIX", juce::dontSendNotification);
    spectralLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    spectralLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(spectralLabel);
    spectralSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    spectralSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    spectralSlider.setRange(0.0, 1.0, 0.01);
    spectralSlider.setValue(processor.getSpectralMix(), juce::dontSendNotification);
    spectralSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff66bbff));
    spectralSlider.onValueChange = [this] { processor.setSpectralMix((float)spectralSlider.getValue()); };
    addAndMakeVisible(spectralSlider);

    auto setupKnob = [this](juce::Slider& slider, juce::Label& label, const juce::String& text, float initial, std::function<void(float)> setter)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colours::white);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(0.0, 1.0, 0.01);
        slider.setValue(initial, juce::dontSendNotification);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff66bbff));
        slider.onValueChange = [&slider, setter] { setter((float)slider.getValue()); };
        addAndMakeVisible(slider);
    };
    setupKnob(attackSlider, attackLabel, "ATTACK", processor.getTransient(), [this](float v){ processor.setTransient(v); });
    setupKnob(sustainSlider, sustainLabel, "SUSTAIN", processor.getSustain(), [this](float v){ processor.setSustain(v); });
    setupKnob(dynamicSlider, dynamicLabel, "DYNAMIC RESPONSE", processor.getDynamicResponse(), [this](float v){ processor.setDynamicResponse(v); });

    membraneTitle.setText("MEMBRANE MODELING", juce::dontSendNotification);
    membraneTitle.setFont(juce::Font(juce::FontOptions(19.0f, juce::Font::bold)));
    membraneTitle.setColour(juce::Label::textColourId, juce::Colour(0xffd8eaf5));
    addAndMakeVisible(membraneTitle);
    membraneToggle.setToggleState(processor.getMembraneEnabled(), juce::dontSendNotification);
    membraneToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    membraneToggle.onClick = [this] { processor.setMembraneEnabled(membraneToggle.getToggleState()); };
    addAndMakeVisible(membraneToggle);
    setupKnob(tensionSlider, tensionLabel, "TENSION", processor.getMembraneTension(), [this](float v){ processor.setMembraneTension(v); });
    setupKnob(stiffnessSlider, stiffnessLabel, "STIFFNESS", processor.getMembraneStiffness(), [this](float v){ processor.setMembraneStiffness(v); });
    setupKnob(decaySlider, decayLabel, "DECAY", processor.getMembraneDecay(), [this](float v){ processor.setMembraneDecay(v); });
    setupKnob(velocitySlider, velocityLabel, "VELOCITY SENS", processor.getMembraneVelocity(), [this](float v){ processor.setMembraneVelocity(v); });

    for (auto* b : { &loadButton, &playButton, &removeButton })
    {
        addAndMakeVisible(*b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff292929));
        b->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    }

    wireLabel.setText("WIRE NOISE", juce::dontSendNotification);
    wireLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    wireLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(wireLabel);
    wireSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    wireSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    wireSlider.setRange(0.0, 1.0, 0.01);
    wireSlider.setValue(processor.getWireNoiseMix(), juce::dontSendNotification);
    wireSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff66bbff));
    wireSlider.onValueChange = [this] { processor.setWireNoiseMix((float)wireSlider.getValue()); };
    addAndMakeVisible(wireSlider);

    loadButton.onClick = [this]
    {
        if (fileChooser != nullptr)
            return;

        fileChooser = std::make_unique<juce::FileChooser>(
            "Choose an audio sample", juce::File{},
            "*.wav;*.aiff;*.aif;*.flac;*.ogg");

        juce::Component::SafePointer<BlackDrumAudioProcessorEditor> safeThis(this);
        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safeThis]
            (const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr)
                    return;

                const auto selected = chooser.getResult();
                safeThis->fileChooser.reset();

                if (selected.existsAsFile())
                    safeThis->loadFrom(selected);
            });
    };

    playButton.onClick = [this]
    {
        // Preview playback is not implemented yet; MIDI notes from the host trigger the sample.
        hint.setText("Use a MIDI note to preview the loaded sample", juce::dontSendNotification);
    };

    removeButton.onClick = [this]
    {
        hint.setText("Sample remains loaded until another sample is selected", juce::dontSendNotification);
        filename.setText("No sample loaded", juce::dontSendNotification);
    };
}

bool BlackDrumAudioProcessorEditor::isSupportedAudioFile(const juce::File& file) const
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff"
        || ext == ".flac" || ext == ".ogg";
}

void BlackDrumAudioProcessorEditor::loadFrom(const juce::File& file)
{
    if (!isSupportedAudioFile(file))
    {
        hint.setText("Unsupported file. Choose WAV, AIFF, FLAC or OGG.", juce::dontSendNotification);
        return;
    }

    if (processor.loadSample(file))
    {
        filename.setText(file.getFileName() + " - "
            + juce::String(static_cast<juce::int64>(file.getSize() / 1024)) + " KB",
            juce::dontSendNotification);
        hint.setText("Sample loaded successfully", juce::dontSendNotification);
    }
    else
    {
        hint.setText("Could not load this audio file", juce::dontSendNotification);
    }
}

bool BlackDrumAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isSupportedAudioFile(juce::File(path)))
            return true;
    return false;
}

void BlackDrumAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    dragHover = true;
    repaint();
}

void BlackDrumAudioProcessorEditor::fileDragExit(const juce::StringArray&)
{
    dragHover = false;
    repaint();
}

void BlackDrumAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    dragHover = false;
    repaint();
    for (const auto& path : files)
    {
        const juce::File file(path);
        if (isSupportedAudioFile(file))
        {
            loadFrom(file);
            return;
        }
    }
}

void BlackDrumAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0b1115));
    auto bounds = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff17242c), 0.0f, 0.0f,
                                           juce::Colour(0xff090d10), (float)getWidth(), (float)getHeight(), false));
    g.fillRoundedRectangle(bounds.reduced(12.0f), 16.0f);
    g.setColour(juce::Colour(0xff34434b));
    g.drawRoundedRectangle(bounds.reduced(12.0f), 16.0f, 1.2f);

    auto panel = juce::Rectangle<float>(30.0f, 140.0f, (float)getWidth() - 60.0f, 220.0f);
    g.setColour(juce::Colour(0xaa081116));
    g.fillRoundedRectangle(panel, 14.0f);
    g.setColour(juce::Colour(0xff354c58));
    g.drawRoundedRectangle(panel, 14.0f, 1.0f);

    // Small hand-drawn snare illustration, inspired by the supplied vintage patent plate.
    const float cx = 150.0f, cy = 260.0f;
    g.setColour(juce::Colour(0xff8ab9d4));
    g.drawEllipse(cx - 70.0f, cy - 43.0f, 140.0f, 30.0f, 1.5f);
    g.drawLine(cx - 70.0f, cy - 28.0f, cx - 66.0f, cy + 25.0f, 1.5f);
    g.drawLine(cx + 70.0f, cy - 28.0f, cx + 66.0f, cy + 25.0f, 1.5f);
    g.drawArc(cx - 66.0f, cy + 8.0f, 132.0f, 30.0f, 0.0f, juce::MathConstants<float>::pi, 1.5f);
    g.drawEllipse(cx - 66.0f, cy + 9.0f, 132.0f, 30.0f, 1.0f);
    for (int k = -2; k <= 2; ++k)
    {
        const float x = cx + (float)k * 27.0f;
        g.drawLine(x, cy - 31.0f, x, cy + 29.0f, 1.0f);
        g.fillEllipse(x - 2.5f, cy - 8.0f, 5.0f, 5.0f);
    }
    g.setColour(juce::Colour(0xff5aaee0));
    g.drawLine(290.0f, 168.0f, 290.0f, 337.0f, 1.0f);

    auto drop = juce::Rectangle<float>(35.0f, 380.0f, (float)getWidth() - 70.0f, 125.0f);
    g.setColour(dragHover ? juce::Colour(0xff172936) : juce::Colour(0xff101619));
    g.fillRoundedRectangle(drop, 12.0f);
    g.setColour(dragHover ? juce::Colour(0xff66bbff) : juce::Colour(0xff3b4a51));
    g.drawRoundedRectangle(drop, 12.0f, dragHover ? 2.0f : 1.0f);
    g.setColour(juce::Colour(0xff9cb2bf));
    g.drawEllipse(getWidth() / 2.0f - 18.0f, 395.0f, 36.0f, 36.0f, 1.8f);
    g.drawLine(getWidth() / 2.0f, 402.0f, getWidth() / 2.0f, 424.0f, 1.8f);
    g.drawLine(getWidth() / 2.0f - 7.0f, 417.0f, getWidth() / 2.0f, 424.0f, 1.8f);
    g.drawLine(getWidth() / 2.0f + 7.0f, 417.0f, getWidth() / 2.0f, 424.0f, 1.8f);
}
void BlackDrumAudioProcessorEditor::resized()
{
    title.setBounds(35, 28, 330, 48);
    const int knobW = 58, gap = 7, labelY = 27, knobY = 48;
    const int total = 6 * knobW + 5 * gap;
    const int x0 = getWidth() - total - 28;
    auto place = [&](juce::Label& label, juce::Slider& slider, int index)
    {
        const int x = x0 + index * (knobW + gap);
        label.setBounds(x - 8, labelY, knobW + 16, 20);
        slider.setBounds(x, knobY, knobW, knobW);
    };
    place(spectralLabel, spectralSlider, 0);
    place(bodyMixLabel, bodyMixSlider, 1);
    place(wireLabel, wireSlider, 2);
    place(attackLabel, attackSlider, 3);
    place(sustainLabel, sustainSlider, 4);
    place(dynamicLabel, dynamicSlider, 5);

    membraneTitle.setBounds(325, 157, 300, 30);
    membraneToggle.setBounds(650, 158, 210, 28);
    auto placeMem = [&](juce::Label& label, juce::Slider& slider, int index)
    {
        const int x = 340 + index * 125;
        label.setBounds(x - 12, 205, 112, 22);
        slider.setBounds(x, 230, 88, 88);
    };
    placeMem(tensionLabel, tensionSlider, 0);
    placeMem(stiffnessLabel, stiffnessSlider, 1);
    placeMem(decayLabel, decaySlider, 2);
    placeMem(velocityLabel, velocitySlider, 3);

    hint.setBounds(45, 438, getWidth() - 90, 24);
    filename.setBounds(45, 465, getWidth() - 90, 24);
    loadButton.setBounds(80, 520, 165, 36);
    playButton.setBounds(267, 520, 165, 36);
    removeButton.setBounds(454, 520, 165, 36);
}
