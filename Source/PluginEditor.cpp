#include "PluginEditor.h"

BlackDrumAudioProcessorEditor::BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(900, 570);
    title.setText("BLACKDRUM", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(25.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    title.setFont(juce::Font(juce::FontOptions(25.0f, juce::Font::bold)));
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


    auto setupKnob = [this](juce::Slider& slider, juce::Label& label, const juce::String& text, float initial, std::function<void(float)> setter)
    {
        label.setText(text, juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, juce::Colour(0xffdce5ea));
        label.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(0.0, 1.0, 0.01);
        slider.setValue(initial, juce::dontSendNotification);
        slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff66bbff));
        slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff39434a));
        slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffd9f1ff));
        slider.onValueChange = [&slider, setter] { setter((float)slider.getValue()); };
        addAndMakeVisible(slider);
    };
    setupKnob(compressorSlider, compressorLabel, "COMPRESSOR", processor.getCompressorMix(), [this](float v){ processor.setCompressorMix(v); });
    setupKnob(roomReverbSlider, roomReverbLabel, "ROOM REVERB", processor.getRoomReverbMix(), [this](float v){ processor.setRoomReverbMix(v); });
    setupKnob(physicalSynthSlider, physicalSynthLabel, "PHYSICAL SYNTH", processor.getPhysicalSynthMix(), [this](float v){ processor.setPhysicalSynthMix(v); });
    setupKnob(attackSlider, attackLabel, "ATTACK", processor.getTransient(), [this](float v){ processor.setTransient(v); });
    setupKnob(sustainSlider, sustainLabel, "SUSTAIN", processor.getSustain(), [this](float v){ processor.setSustain(v); });
    setupKnob(dynamicSlider, dynamicLabel, "DYNAMIC RESPONSE", processor.getDynamicResponse(), [this](float v){ processor.setDynamicResponse(v); });

    membraneTitle.setText("MEMBRANE MODELING", juce::dontSendNotification);
    membraneTitle.setFont(juce::Font(juce::FontOptions(19.0f, juce::Font::bold)));
    membraneTitle.setColour(juce::Label::textColourId, juce::Colour(0xffd8eaf5));
    membraneTitle.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    addAndMakeVisible(membraneTitle);
    membraneToggle.setToggleState(processor.getMembraneEnabled(), juce::dontSendNotification);
    membraneToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffdce5ea));
    membraneToggle.setColour(juce::ToggleButton::tickColourId, juce::Colour(0xff66bbff));
    membraneToggle.onClick = [this] { processor.setMembraneEnabled(membraneToggle.getToggleState()); };
    addAndMakeVisible(membraneToggle);
    setupKnob(tensionSlider, tensionLabel, "TENSION", processor.getMembraneTension(), [this](float v){ processor.setMembraneTension(v); });
    setupKnob(stiffnessSlider, stiffnessLabel, "STIFFNESS", processor.getMembraneStiffness(), [this](float v){ processor.setMembraneStiffness(v); });
    setupKnob(decaySlider, decayLabel, "DECAY", processor.getMembraneDecay(), [this](float v){ processor.setMembraneDecay(v); });
    setupKnob(velocitySlider, velocityLabel, "VELOCITY SENS", processor.getMembraneVelocity(), [this](float v){ processor.setMembraneVelocity(v); });

    for (auto* b : { &loadButton, &playButton, &removeButton, &savePresetButton, &loadPresetButton })
    {
        addAndMakeVisible(*b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff252b2f));
        b->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff33414a));
        b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffedf7fb));
        b->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
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
    wireSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff39434a));
    wireSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffd9f1ff));
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
    phaseVocoderSlider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff39434a));
    phaseVocoderSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffd9f1ff));
    phaseVocoderSlider.onValueChange = [this] { processor.setPhaseVocoderMix((float)phaseVocoderSlider.getValue()); };
    addAndMakeVisible(phaseVocoderSlider);

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
    wireSlider.setValue(processor.getWireNoiseMix(), juce::dontSendNotification);
    attackSlider.setValue(processor.getTransient(), juce::dontSendNotification);
    sustainSlider.setValue(processor.getSustain(), juce::dontSendNotification);
    dynamicSlider.setValue(processor.getDynamicResponse(), juce::dontSendNotification);
    compressorSlider.setValue(processor.getCompressorMix(), juce::dontSendNotification);
    roomReverbSlider.setValue(processor.getRoomReverbMix(), juce::dontSendNotification);
    physicalSynthSlider.setValue(processor.getPhysicalSynthMix(), juce::dontSendNotification);
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
    // Keep drawing intentionally simple: fewer custom graphics means fewer UI-specific build risks.
    g.fillAll(juce::Colour(0xff111416));

    // Purely visual panels. No DSP/audio state is touched here.
    auto panel = [&g](juce::Rectangle<float> r, juce::Colour fill, juce::Colour stroke)
    {
        g.setColour(fill);
        g.fillRoundedRectangle(r, 12.0f);
        g.setColour(stroke);
        g.drawRoundedRectangle(r, 12.0f, 1.0f);
    };
    panel({ 16.0f, 54.0f, 430.0f, 242.0f }, juce::Colour(0xff191d20), juce::Colour(0xff30373b));
    panel({ 458.0f, 54.0f, 426.0f, 190.0f }, juce::Colour(0xff191d20), juce::Colour(0xff30373b));
    panel({ 16.0f, 292.0f, 868.0f, 76.0f }, juce::Colour(0xff161a1d), juce::Colour(0xff2b3236));
    panel({ 16.0f, 376.0f, 868.0f, 174.0f }, juce::Colour(0xff191d20), juce::Colour(0xff30373b));

    g.setColour(juce::Colour(0xff30383d));
    g.drawLine(32.0f, 45.0f, 868.0f, 45.0f, 1.0f);
    g.setColour(juce::Colour(0xff66bbff));
    g.fillRoundedRectangle(32.0f, 43.0f, 90.0f, 3.0f, 1.5f);

    const float x = (float)getWidth() - 92.0f;
    const float y = 10.0f;
    g.setColour(juce::Colours::white);
    g.fillEllipse(x, y, 72.0f, 28.0f);
    g.setColour(juce::Colour(0xff202020));
    g.drawEllipse(x, y, 72.0f, 28.0f, 2.0f);
    g.drawLine(x, y + 14.0f, x, y + 45.0f, 2.0f);
    g.drawLine(x + 72.0f, y + 14.0f, x + 72.0f, y + 45.0f, 2.0f);
    g.drawEllipse(x, y + 32.0f, 72.0f, 20.0f, 2.0f);
    g.fillEllipse(x + 33.0f, y + 10.0f, 6.0f, 6.0f);
    for (int i = 0; i < 7; ++i)
        g.drawLine(x + 8.0f + i * 9.0f, y + 42.0f, x + 5.0f + i * 9.0f, y + 55.0f, 1.0f);
}
void BlackDrumAudioProcessorEditor::resized()
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
    place(wireLabel, wireSlider, 2, 0);
    place(phaseVocoderLabel, phaseVocoderSlider, 3, 0);
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
    // Living controls stay together in the lower-left area and have their own
    // row, so they cannot overlap the existing processing controls.
    compressorLabel.setBounds(24, 430, 112, 20);
    compressorSlider.setBounds(48, 450, 64, 64);
    roomReverbLabel.setBounds(148, 430, 112, 20);
    roomReverbSlider.setBounds(172, 450, 64, 64);
    physicalSynthLabel.setBounds(272, 430, 112, 20);
    physicalSynthSlider.setBounds(296, 450, 64, 64);
    savePresetButton.setBounds(405, 455, 170, 36);
    loadPresetButton.setBounds(600, 455, 170, 36);
}
