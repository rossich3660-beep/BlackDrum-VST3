#include "PluginEditor.h"

BlackDrumAudioProcessorEditor::BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(760, 430);
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
    g.fillAll(juce::Colour(0xff111111));
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff1b1b1b));
    g.fillRoundedRectangle(bounds.reduced(14.0f), 14.0f);
    g.setColour(dragHover ? juce::Colour(0xff66bbff) : juce::Colour(0xff303030));
    g.drawRoundedRectangle(bounds.reduced(14.0f), 14.0f, dragHover ? 2.5f : 1.0f);

    auto area = juce::Rectangle<float>(35.0f, 112.0f, static_cast<float>(getWidth() - 70), 190.0f);
    g.setColour(dragHover ? juce::Colour(0xff202a33) : juce::Colour(0xff151515));
    g.fillRoundedRectangle(area, 12.0f);
    g.setColour(dragHover ? juce::Colour(0xff66bbff) : juce::Colour(0xff484848));
    g.drawRoundedRectangle(area, 12.0f, 1.5f);

    g.setColour(juce::Colour(0xffbdbdbd));
    g.drawEllipse(getWidth() / 2.0f - 22.0f, 145.0f, 44.0f, 44.0f, 2.0f);
    g.drawLine(getWidth() / 2.0f, 155.0f, getWidth() / 2.0f, 179.0f, 2.0f);
    g.drawLine(getWidth() / 2.0f - 8.0f, 170.0f, getWidth() / 2.0f, 179.0f, 2.0f);
    g.drawLine(getWidth() / 2.0f + 8.0f, 170.0f, getWidth() / 2.0f, 179.0f, 2.0f);
}

void BlackDrumAudioProcessorEditor::resized()
{
    title.setBounds(35, 28, 220, 45);
    hint.setBounds(45, 255, getWidth() - 90, 30);
    filename.setBounds(45, 292, getWidth() - 90, 25);
    const int knobY = 48, labelY = 28, knobW = 68, gap = 12;
    const int total = 5 * knobW + 4 * gap;
    const int x0 = getWidth() - total - 32;
    auto place = [&](juce::Label& label, juce::Slider& slider, int index)
    {
        const int x = x0 + index * (knobW + gap);
        label.setBounds(x - 5, labelY, knobW + 10, 20);
        slider.setBounds(x, knobY, knobW, knobW);
    };
    place(spectralLabel, spectralSlider, 0);
    place(bodyMixLabel, bodyMixSlider, 1);
    place(wireLabel, wireSlider, 2);
    place(attackLabel, attackSlider, 3);
    place(sustainLabel, sustainSlider, 4);
    loadButton.setBounds(95, 365, 165, 38);
    playButton.setBounds(297, 365, 165, 38);
    removeButton.setBounds(499, 365, 165, 38);
}
