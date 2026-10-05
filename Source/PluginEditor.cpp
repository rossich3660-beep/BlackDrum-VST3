#include "PluginEditor.h"

BlackDrumAudioProcessorEditor::BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(1100, 700);
    title.setText("BlackSnare", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(42.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffe7f4fb));
    addAndMakeVisible(title);

    filename.setText("No sample loaded", juce::dontSendNotification);
    filename.setJustificationType(juce::Justification::centred);
    filename.setColour(juce::Label::textColourId, juce::Colour(0xffeeeeee));
    addAndMakeVisible(filename);

    hint.setText("Drop an audio file here - WAV / AIFF / FLAC / OGG", juce::dontSendNotification);
    hint.setJustificationType(juce::Justification::centred);
    hint.setColour(juce::Label::textColourId, juce::Colour(0xff929292));
    addAndMakeVisible(hint);

    voiceCountLabel.setText("VOICES", juce::dontSendNotification);
    voiceCountLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    voiceCountLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(voiceCountLabel);
    voiceCountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    voiceCountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 38, 20);
    voiceCountSlider.setRange(1, 16, 1);
    voiceCountSlider.setValue(processor.getVoiceCount(), juce::dontSendNotification);
    voiceCountSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff66bbff));
    voiceCountSlider.onValueChange = [this] { processor.setVoiceCount((int)voiceCountSlider.getValue()); };
    addAndMakeVisible(voiceCountSlider);

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

    for (auto* b : { &loadButton, &playButton, &removeButton, &savePresetButton, &loadPresetButton })
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

    phaseVocoderLabel.setText("PHASE MORPH", juce::dontSendNotification);
    phaseVocoderLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    phaseVocoderLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(phaseVocoderLabel);
    phaseVocoderSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    phaseVocoderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    phaseVocoderSlider.setRange(0.0, 1.0, 0.01);
    phaseVocoderSlider.setValue(processor.getPhaseVocoderMix(), juce::dontSendNotification);
    phaseVocoderSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff66bbff));
    phaseVocoderSlider.onValueChange = [this] { processor.setPhaseVocoderMix((float)phaseVocoderSlider.getValue()); };
    addAndMakeVisible(phaseVocoderSlider);

    shellLabel.setText("SHELL RESONANCE", juce::dontSendNotification);
    shellLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    shellLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(shellLabel);
    shellSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    shellSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    shellSlider.setRange(0.0, 1.0, 0.01);
    shellSlider.setValue(processor.getShellResonanceMix(), juce::dontSendNotification);
    snareWireLayerSlider.setValue(processor.getWireNoiseMix(), juce::dontSendNotification);
    phaseVocoderLayerSlider.setValue(processor.getPhaseVocoderMix(), juce::dontSendNotification);
    shellSlider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff66bbff));
    shellSlider.onValueChange = [this] { processor.setShellResonanceMix((float)shellSlider.getValue()); };
    addAndMakeVisible(shellSlider);

    setupKnob(snareWireLayerSlider, snareWireLayerLabel, "SNARE / WIRE", processor.getWireNoiseMix(), [this](float v){ processor.setWireNoiseMix(v); });
    setupKnob(phaseVocoderLayerSlider, phaseVocoderLayerLabel, "PHASE VOCODER", processor.getPhaseVocoderMix(), [this](float v){ processor.setPhaseVocoderMix(v); });

    savePresetButton.onClick = [this]
    {
        if (presetChooser != nullptr)
            return;
        presetChooser = std::make_unique<juce::FileChooser>(
            "Save BlackDrum preset", juce::File{}, "*.blackdrum");
        juce::Component::SafePointer<BlackDrumAudioProcessorEditor> safeThis(this);
        presetChooser->launchAsync(
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [safeThis](const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr) return;
                auto file = chooser.getResult();
                safeThis->presetChooser.reset();
                if (file != juce::File{})
                {
                    if (!file.hasFileExtension(".blackdrum"))
                        file = file.withFileExtension(".blackdrum");
                    safeThis->savePresetTo(file);
                }
            });
    };

    loadPresetButton.onClick = [this]
    {
        if (presetChooser != nullptr)
            return;
        presetChooser = std::make_unique<juce::FileChooser>(
            "Load BlackDrum preset", juce::File{}, "*.blackdrum");
        juce::Component::SafePointer<BlackDrumAudioProcessorEditor> safeThis(this);
        presetChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr) return;
                const auto file = chooser.getResult();
                safeThis->presetChooser.reset();
                if (file.existsAsFile())
                    safeThis->loadPresetFrom(file);
            });
    };

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

void BlackDrumAudioProcessorEditor::savePresetTo(const juce::File& file)
{
    const auto state = processor.createPresetState();
    if (auto xml = state.createXml())
    {
        xml->setAttribute("presetName", file.getFileNameWithoutExtension());
        if (file.replaceWithText(xml->toString()))
            hint.setText("Preset saved: " + file.getFileName(), juce::dontSendNotification);
        else
            hint.setText("Could not save preset", juce::dontSendNotification);
    }
}

void BlackDrumAudioProcessorEditor::loadPresetFrom(const juce::File& file)
{
    const auto xml = juce::parseXML(file);
    if (xml == nullptr)
    {
        hint.setText("Invalid BlackDrum preset", juce::dontSendNotification);
        return;
    }

    const auto state = juce::ValueTree::fromXml(*xml);
    if (!processor.applyPresetState(state))
    {
        hint.setText("Invalid BlackDrum preset", juce::dontSendNotification);
        return;
    }

    filename.setText(processor.sampleName().isNotEmpty() ? processor.sampleName() : "No sample loaded",
                     juce::dontSendNotification);
    voiceCountSlider.setValue(processor.getVoiceCount(), juce::dontSendNotification);
    bodyMixSlider.setValue(processor.getBodyMix(), juce::dontSendNotification);
    spectralSlider.setValue(processor.getSpectralMix(), juce::dontSendNotification);
    wireSlider.setValue(processor.getWireNoiseMix(), juce::dontSendNotification);
    attackSlider.setValue(processor.getTransient(), juce::dontSendNotification);
    sustainSlider.setValue(processor.getSustain(), juce::dontSendNotification);
    dynamicSlider.setValue(processor.getDynamicResponse(), juce::dontSendNotification);
    shellSlider.setValue(processor.getShellResonanceMix(), juce::dontSendNotification);
    membraneToggle.setToggleState(processor.getMembraneEnabled(), juce::dontSendNotification);
    tensionSlider.setValue(processor.getMembraneTension(), juce::dontSendNotification);
    stiffnessSlider.setValue(processor.getMembraneStiffness(), juce::dontSendNotification);
    decaySlider.setValue(processor.getMembraneDecay(), juce::dontSendNotification);
    velocitySlider.setValue(processor.getMembraneVelocity(), juce::dontSendNotification);
    hint.setText("Preset loaded safely", juce::dontSendNotification);
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
    const auto bounds = getLocalBounds();
    g.fillAll(juce::Colour(0xff071118));
    g.setColour(juce::Colour(0xff0d2430));
    for (int y = 8; y < getHeight(); y += 32) g.drawHorizontalLine(y, 8.0f, (float)getWidth() - 8.0f);
    for (int x = 8; x < getWidth(); x += 32) g.drawVerticalLine(x, 8.0f, (float)getHeight() - 8.0f);
    g.setColour(juce::Colour(0xff1d5b78));
    g.drawRoundedRectangle(bounds.toFloat().reduced(7.0f), 10.0f, 1.2f);

    g.setColour(juce::Colour(0xff8bd7ff));
    g.drawLine(42.0f, 102.0f, 1058.0f, 102.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::plain)));
    g.drawText("PHYSICAL MODELING SNARE DRUM", 555, 42, 430, 24, juce::Justification::left);
    g.drawText("ONE SHOT  •  RESYNTHESIS  •  MORPHING", 555, 68, 430, 20, juce::Justification::left);

    // Simple blueprint drum sketches; vector-only to keep the editor stable.
    g.setColour(juce::Colour(0xff6fa6ba).withAlpha(0.58f));
    g.drawEllipse(25.0f, 28.0f, 150.0f, 52.0f, 1.2f);
    g.drawEllipse(25.0f, 82.0f, 150.0f, 52.0f, 1.2f);
    for (int i = 0; i < 8; ++i)
    {
        const float x = 34.0f + i * 18.0f;
        g.drawLine(x, 40.0f, x, 122.0f, 0.8f);
    }
    g.drawEllipse(925.0f, 28.0f, 150.0f, 52.0f, 1.2f);
    g.drawEllipse(925.0f, 82.0f, 150.0f, 52.0f, 1.2f);
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
    g.drawText("SNARE DRUM", 925, 14, 150, 16, juce::Justification::centred);

    auto panel = juce::Rectangle<float>(28.0f, 116.0f, 1044.0f, 430.0f);
    g.setColour(juce::Colour(0xff081820).withAlpha(0.96f));
    g.fillRoundedRectangle(panel, 18.0f);
    g.setColour(juce::Colour(0xff2b8bb4));
    g.drawRoundedRectangle(panel, 18.0f, 1.2f);
    g.setColour(juce::Colour(0xff6fc8eb));
    g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    g.drawText("PHYSICAL LAYERS", 48, 130, 250, 24, juce::Justification::left);
    g.drawText("MEMBRANE MODELING", 570, 130, 300, 24, juce::Justification::left);
    g.drawLine(48.0f, 158.0f, 548.0f, 158.0f, 1.0f);
    g.drawLine(570.0f, 158.0f, 1048.0f, 158.0f, 1.0f);
    g.setColour(juce::Colour(0xff25424e));
    g.drawLine(555.0f, 130.0f, 555.0f, 535.0f, 1.0f);
    g.drawLine(48.0f, 370.0f, 1048.0f, 370.0f, 1.0f);
    g.drawLine(48.0f, 468.0f, 1048.0f, 468.0f, 1.0f);
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::italic)));
    g.drawText("Real Drums", 35, 570, 150, 22, juce::Justification::left);
    g.drawText("Real Feel", 48, 592, 150, 22, juce::Justification::left);
}
void BlackDrumAudioProcessorEditor::resized()
{
    const int knob = 70;
    auto knobPlace = [&](juce::Label& label, juce::Slider& slider, int x, int y, int w)
    {
        label.setBounds(x, y, w, 22);
        slider.setBounds(x + (w - knob) / 2, y + 24, knob, knob);
    };

    title.setBounds(195, 24, 430, 58);
    hint.setBounds(170, 550, 760, 22);
    filename.setBounds(170, 574, 760, 22);

    knobPlace(bodyMixLabel, bodyMixSlider, 48, 176, 108);
    knobPlace(spectralLabel, spectralSlider, 165, 176, 108);
    knobPlace(wireLabel, wireSlider, 282, 176, 108);
    knobPlace(phaseVocoderLabel, phaseVocoderSlider, 399, 176, 108);

    knobPlace(attackLabel, attackSlider, 48, 276, 108);
    knobPlace(sustainLabel, sustainSlider, 165, 276, 108);
    knobPlace(dynamicLabel, dynamicSlider, 282, 276, 108);

    membraneTitle.setBounds(570, 130, 300, 24);
    membraneToggle.setBounds(570, 168, 220, 24);
    knobPlace(tensionLabel, tensionSlider, 570, 212, 108);
    knobPlace(stiffnessLabel, stiffnessSlider, 687, 212, 108);
    knobPlace(decayLabel, decaySlider, 804, 212, 108);
    knobPlace(velocityLabel, velocitySlider, 921, 212, 108);

    voiceCountLabel.setBounds(570, 320, 90, 24);
    voiceCountSlider.setBounds(665, 320, 250, 24);

    // Lower physical layer controls.
    knobPlace(shellLabel, shellSlider, 210, 384, 150);
    knobPlace(snareWireLayerLabel, snareWireLayerSlider, 480, 384, 150);
    knobPlace(phaseVocoderLayerLabel, phaseVocoderLayerSlider, 750, 384, 150);

    loadButton.setBounds(210, 610, 210, 38);
    playButton.setBounds(445, 610, 210, 38);
    removeButton.setBounds(680, 610, 210, 38);
    savePresetButton.setBounds(320, 655, 210, 36);
    loadPresetButton.setBounds(570, 655, 210, 36);
}
{
    const int margin = 20;
    title.setBounds(margin, 12, 240, 36);

    // Main processing controls: two compact rows, with labels kept separate from knobs.
    const int knobW = 64;
    const int colW = 112;
    const int startX = 24;
    const int row1LabelY = 62, row1KnobY = 82;
    const int row2LabelY = 170, row2KnobY = 190;
    auto place = [&](juce::Label& label, juce::Slider& slider, int index, int row)
    {
        const int x = startX + index * colW;
        const int ly = row == 0 ? row1LabelY : row2LabelY;
        const int ky = row == 0 ? row1KnobY : row2KnobY;
        label.setBounds(x, ly, colW - 8, 20);
        slider.setBounds(x + (colW - knobW) / 2 - 4, ky, knobW, knobW);
    };
    place(bodyMixLabel, bodyMixSlider, 0, 0);
    place(spectralLabel, spectralSlider, 1, 0);
    place(wireLabel, wireSlider, 2, 0);
    place(phaseVocoderLabel, phaseVocoderSlider, 3, 0);
    place(shellLabel, shellSlider, 4, 0);
    place(attackLabel, attackSlider, 0, 1);
    place(sustainLabel, sustainSlider, 1, 1);
    place(dynamicLabel, dynamicSlider, 2, 1);

    membraneTitle.setBounds(470, 62, 250, 24);
    membraneToggle.setBounds(470, 88, 220, 26);
    auto placeMem = [&](juce::Label& label, juce::Slider& slider, int index)
    {
        const int x = 470 + index * 100;
        label.setBounds(x, 142, 96, 20);
        slider.setBounds(x + 14, 164, 64, 64);
    };
    placeMem(tensionLabel, tensionSlider, 0);
    placeMem(stiffnessLabel, stiffnessSlider, 1);
    placeMem(decayLabel, decaySlider, 2);
    placeMem(velocityLabel, velocitySlider, 3);

    voiceCountLabel.setBounds(470, 250, 100, 24);
    voiceCountSlider.setBounds(570, 250, 180, 24);
    hint.setBounds(margin, 300, getWidth() - 2 * margin, 24);
    filename.setBounds(margin, 326, getWidth() - 2 * margin, 24);
    loadButton.setBounds(120, 390, 170, 36);
    playButton.setBounds(365, 390, 170, 36);
    removeButton.setBounds(610, 390, 170, 36);
    savePresetButton.setBounds(190, 455, 220, 36);
    loadPresetButton.setBounds(490, 455, 220, 36);
}
