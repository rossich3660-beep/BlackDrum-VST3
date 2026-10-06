#include "PluginEditor.h"

BlackDrumAudioProcessorEditor::BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(980, 610);
    setResizable(false, false);

    title.setText("blackSnare", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(26.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xfff2f4f5));
    addAndMakeVisible(title);

    sampleName.setText("No reference sample loaded", juce::dontSendNotification);
    sampleName.setColour(juce::Label::textColourId, juce::Colour(0xffaeb8bd));
    sampleName.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(sampleName);

    status.setText("Load one real snare hit. It becomes the reference for the physical model.", juce::dontSendNotification);
    status.setColour(juce::Label::textColourId, juce::Colour(0xff7f8a90));
    status.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(status);

    auto styleButton = [](juce::TextButton& b)
    {
        b.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff242a2e));
        b.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3b4b53));
        b.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffeaf0f3));
        b.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        };
    for (auto* b : { &loadButton, &previewButton, &savePresetButton, &loadPresetButton })
    {
        styleButton(*b);
        addAndMakeVisible(*b);
    }

    setupKnob(topTune, topTuneLabel, "TOP TUNING", processor.getTopTuning(), [this](float v){ processor.setTopTuning(v); });
    setupKnob(topDamp, topDampLabel, "TOP DAMPING", processor.getTopDamping(), [this](float v){ processor.setTopDamping(v); });
    setupKnob(bottomTune, bottomTuneLabel, "BOTTOM TUNING", processor.getBottomTuning(), [this](float v){ processor.setBottomTuning(v); });
    setupKnob(bottomDamp, bottomDampLabel, "BOTTOM DAMPING", processor.getBottomDamping(), [this](float v){ processor.setBottomDamping(v); });

    setupKnob(diameter, diameterLabel, "DIAMETER", processor.getShellDiameter(), [this](float v){ processor.setShellDiameter(v); });
    setupKnob(depth, depthLabel, "SHELL DEPTH", processor.getShellDepth(), [this](float v){ processor.setShellDepth(v); });
    setupKnob(material, materialLabel, "SHELL MATERIAL", processor.getShellMaterial(), [this](float v){ processor.setShellMaterial(v); });
    setupKnob(resonance, resonanceLabel, "RESONANCE", processor.getResonance(), [this](float v){ processor.setResonance(v); });

    setupKnob(wireTension, wireTensionLabel, "WIRE TENSION", processor.getWireTension(), [this](float v){ processor.setWireTension(v); });
    setupKnob(wireAmount, wireAmountLabel, "WIRE AMOUNT", processor.getWireAmount(), [this](float v){ processor.setWireAmount(v); });
    setupKnob(wireDamping, wireDampingLabel, "WIRE DAMPING", processor.getWireDamping(), [this](float v){ processor.setWireDamping(v); });
    setupKnob(stick, stickLabel, "STICK HARDNESS", processor.getStickHardness(), [this](float v){ processor.setStickHardness(v); });

    setupKnob(sampleRef, sampleRefLabel, "REFERENCE", processor.getSampleInfluence(), [this](float v){ processor.setSampleInfluence(v); });
    setupKnob(velocity, velocityLabel, "VELOCITY", processor.getVelocityResponse(), [this](float v){ processor.setVelocityResponse(v); });
    setupKnob(room, roomLabel, "ROOM", processor.getRoom(), [this](float v){ processor.setRoom(v); });
    setupKnob(master, masterLabel, "MASTER", processor.getMaster(), [this](float v){ processor.setMaster(v); });

    voicesLabel.setText("VOICES", juce::dontSendNotification);
    voicesLabel.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
    voicesLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9da9ae));
    voicesLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(voicesLabel);
    voices.setSliderStyle(juce::Slider::LinearHorizontal);
    voices.setTextBoxStyle(juce::Slider::TextBoxRight, false, 34, 18);
    voices.setRange(1, 16, 1);
    voices.setValue(processor.getVoiceCount(), juce::dontSendNotification);
    voices.setColour(juce::Slider::thumbColourId, juce::Colour(0xff8ed0ff));
    voices.onValueChange = [this] { processor.setVoiceCount((int) voices.getValue()); };
    addAndMakeVisible(voices);

    loadButton.onClick = [this]
    {
        if (fileChooser != nullptr) return;
        fileChooser = std::make_unique<juce::FileChooser>("Load snare reference", juce::File{}, "*.wav;*.aif;*.aiff;*.flac;*.ogg");
        juce::Component::SafePointer<BlackDrumAudioProcessorEditor> safeThis(this);
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& c)
            {
                if (safeThis == nullptr) return;
                const auto file = c.getResult();
                safeThis->fileChooser.reset();
                if (file.existsAsFile()) safeThis->loadFrom(file);
            });
    };

    previewButton.onClick = [this]
    {
        status.setText("Reference is synthesized, not played back. Send a MIDI note to hear blackSnare.", juce::dontSendNotification);
    };

    savePresetButton.onClick = [this]
    {
        if (presetChooser != nullptr) return;
        presetChooser = std::make_unique<juce::FileChooser>("Save blackSnare preset", juce::File{}, "*.blacksnare");
        juce::Component::SafePointer<BlackDrumAudioProcessorEditor> safeThis(this);
        presetChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [safeThis](const juce::FileChooser& c)
            {
                if (safeThis == nullptr) return;
                auto file = c.getResult();
                safeThis->presetChooser.reset();
                if (file != juce::File{})
                {
                    if (!file.hasFileExtension(".blacksnare")) file = file.withFileExtension(".blacksnare");
                    safeThis->savePresetTo(file);
                }
            });
    };

    loadPresetButton.onClick = [this]
    {
        if (presetChooser != nullptr) return;
        presetChooser = std::make_unique<juce::FileChooser>("Load blackSnare preset", juce::File{}, "*.blacksnare");
        juce::Component::SafePointer<BlackDrumAudioProcessorEditor> safeThis(this);
        presetChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& c)
            {
                if (safeThis == nullptr) return;
                const auto file = c.getResult();
                safeThis->presetChooser.reset();
                if (file.existsAsFile()) safeThis->loadPresetFrom(file);
            });
    };
}

void BlackDrumAudioProcessorEditor::setupKnob(juce::Slider& slider, juce::Label& label,
                                              const juce::String& text, float initial,
                                              std::function<void(float)> setter)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffb8c3c8));
    label.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange(0.0, 1.0, 0.01);
    slider.setValue(initial, juce::dontSendNotification);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff8ed0ff));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff3b454a));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour(0xffe6f7ff));
    slider.onValueChange = [&slider, setter] { setter((float) slider.getValue()); };
    addAndMakeVisible(slider);
}

void BlackDrumAudioProcessorEditor::syncFromProcessor()
{
    topTune.setValue(processor.getTopTuning(), juce::dontSendNotification);
    topDamp.setValue(processor.getTopDamping(), juce::dontSendNotification);
    bottomTune.setValue(processor.getBottomTuning(), juce::dontSendNotification);
    bottomDamp.setValue(processor.getBottomDamping(), juce::dontSendNotification);
    diameter.setValue(processor.getShellDiameter(), juce::dontSendNotification);
    depth.setValue(processor.getShellDepth(), juce::dontSendNotification);
    material.setValue(processor.getShellMaterial(), juce::dontSendNotification);
    resonance.setValue(processor.getResonance(), juce::dontSendNotification);
    wireTension.setValue(processor.getWireTension(), juce::dontSendNotification);
    wireAmount.setValue(processor.getWireAmount(), juce::dontSendNotification);
    wireDamping.setValue(processor.getWireDamping(), juce::dontSendNotification);
    stick.setValue(processor.getStickHardness(), juce::dontSendNotification);
    sampleRef.setValue(processor.getSampleInfluence(), juce::dontSendNotification);
    velocity.setValue(processor.getVelocityResponse(), juce::dontSendNotification);
    room.setValue(processor.getRoom(), juce::dontSendNotification);
    master.setValue(processor.getMaster(), juce::dontSendNotification);
    voices.setValue(processor.getVoiceCount(), juce::dontSendNotification);
}

void BlackDrumAudioProcessorEditor::savePresetTo(const juce::File& file)
{
    if (auto xml = processor.createPresetState().createXml())
    {
        xml->setAttribute("presetName", file.getFileNameWithoutExtension());
        status.setText(file.replaceWithText(xml->toString()) ? "Preset saved" : "Could not save preset", juce::dontSendNotification);
    }
}

void BlackDrumAudioProcessorEditor::loadPresetFrom(const juce::File& file)
{
    const auto xml = juce::parseXML(file);
    if (xml == nullptr || !processor.applyPresetState(juce::ValueTree::fromXml(*xml)))
    {
        status.setText("Invalid blackSnare preset", juce::dontSendNotification);
        return;
    }
    syncFromProcessor();
    sampleName.setText(processor.sampleName().isNotEmpty() ? processor.sampleName() : "No reference sample loaded", juce::dontSendNotification);
    status.setText("Preset loaded", juce::dontSendNotification);
}

bool BlackDrumAudioProcessorEditor::isSupportedAudioFile(const juce::File& file) const
{
    const auto e = file.getFileExtension().toLowerCase();
    return e == ".wav" || e == ".aif" || e == ".aiff" || e == ".flac" || e == ".ogg";
}

void BlackDrumAudioProcessorEditor::loadFrom(const juce::File& file)
{
    if (!isSupportedAudioFile(file)) { status.setText("Unsupported audio file", juce::dontSendNotification); return; }
    if (processor.loadSample(file))
    {
        sampleName.setText(file.getFileName(), juce::dontSendNotification);
        status.setText("Reference analyzed — MIDI triggers the synthesized snare", juce::dontSendNotification);
    }
    else status.setText("Could not read reference sample", juce::dontSendNotification);
}

bool BlackDrumAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& f : files) if (isSupportedAudioFile(juce::File(f))) return true;
    return false;
}

void BlackDrumAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int) { dragHover = true; repaint(); }
void BlackDrumAudioProcessorEditor::fileDragExit(const juce::StringArray&) { dragHover = false; repaint(); }

void BlackDrumAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    dragHover = false; repaint();
    for (const auto& f : files)
    {
        const juce::File file(f);
        if (isSupportedAudioFile(file)) { loadFrom(file); return; }
    }
}

void BlackDrumAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1214));
    auto panel = [&g](juce::Rectangle<float> r)
    {
        g.setColour(juce::Colour(0xff171c1f)); g.fillRoundedRectangle(r, 10.0f);
        g.setColour(juce::Colour(0xff30383d)); g.drawRoundedRectangle(r, 10.0f, 1.0f);
    };
    panel({18.0f,72.0f,286.0f,270.0f});
    panel({676.0f,72.0f,286.0f,270.0f});
    panel({318.0f,72.0f,344.0f,270.0f});
    panel({18.0f,356.0f,944.0f,172.0f});
    panel({18.0f,542.0f,944.0f,50.0f});

    g.setColour(juce::Colour(0xff8ed0ff));
    g.fillRoundedRectangle(20.0f,58.0f,92.0f,3.0f,1.5f);

    const float cx=490.0f, cy=206.0f;
    g.setColour(juce::Colour(0xffd5dadd));
    g.fillEllipse(cx-118.0f,cy-78.0f,236.0f,46.0f);
    g.setColour(juce::Colour(0xff343d42)); g.drawEllipse(cx-118.0f,cy-78.0f,236.0f,46.0f,2.0f);
    g.setColour(juce::Colour(0xff7c4f38)); g.fillRoundedRectangle(cx-104.0f,cy-55.0f,208.0f,110.0f,12.0f);
    g.setColour(juce::Colour(0xff2a3033)); g.drawRoundedRectangle(cx-104.0f,cy-55.0f,208.0f,110.0f,12.0f,2.0f);
    g.setColour(juce::Colour(0xffbfc7ca)); g.fillEllipse(cx-118.0f,cy+32.0f,236.0f,46.0f);
    g.setColour(juce::Colour(0xff30383c)); g.drawEllipse(cx-118.0f,cy+32.0f,236.0f,46.0f,2.0f);
    g.setColour(juce::Colour(0xffd6dde0));
    for (int i=-4;i<=4;++i) g.drawLine(cx-70.0f+i*17.0f,cy+44.0f,cx-70.0f+i*17.0f,cy+66.0f,1.5f);

    g.setColour(juce::Colour(0xffeef4f6));
    g.setFont(juce::Font(juce::FontOptions(19.0f, juce::Font::bold)));
    g.drawText("PHYSICAL SNARE",350,238,280,28,juce::Justification::centred);
    g.setColour(juce::Colour(0xff8ed0ff));
    g.drawLine(cx-82.0f,cy-58.0f,cx-82.0f,cy+58.0f,1.0f);
    g.drawLine(cx+82.0f,cy-58.0f,cx+82.0f,cy+58.0f,1.0f);

    g.setColour(juce::Colour(0xff9da9ae));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("BATTER HEAD",350,92,280,20,juce::Justification::centred);
    g.drawText("SHELL",350,116,280,20,juce::Justification::centred);
    g.drawText("SNARE WIRES",350,300,280,20,juce::Justification::centred);

    if (dragHover)
    {
        g.setColour(juce::Colour(0x668ed0ff)); g.fillRoundedRectangle(18.0f,72.0f,944.0f,456.0f,10.0f);
        g.setColour(juce::Colours::white); g.drawRoundedRectangle(24.0f,78.0f,932.0f,444.0f,8.0f,2.0f);
        g.setFont(juce::Font(juce::FontOptions(22.0f, juce::Font::bold)));
        g.drawText("DROP SNARE REFERENCE",0,270,getWidth(),34,juce::Justification::centred);
    }
}

void BlackDrumAudioProcessorEditor::resized()
{
    title.setBounds(20,14,190,34);
    sampleName.setBounds(215,18,430,24);
    loadButton.setBounds(664,14,112,32);
    previewButton.setBounds(780,14,82,32);
    savePresetButton.setBounds(866,14,48,32);
    loadPresetButton.setBounds(918,14,44,32);

    const int knob=58;
    auto place=[knob](juce::Label& l,juce::Slider& s,int x,int y)
    { l.setBounds(x-10,y,78,18); s.setBounds(x,y+18,knob,knob); };

    place(topTuneLabel,topTune,42,96); place(topDampLabel,topDamp,142,96);
    place(bottomTuneLabel,bottomTune,42,192); place(bottomDampLabel,bottomDamp,142,192);

    place(diameterLabel,diameter,700,96); place(depthLabel,depth,800,96);
    place(materialLabel,material,900,96); place(resonanceLabel,resonance,750,192);
    place(wireTensionLabel,wireTension,850,192); place(wireAmountLabel,wireAmount,700,288);
    place(wireDampingLabel,wireDamping,800,288); place(stickLabel,stick,900,288);

    place(sampleRefLabel,sampleRef,58,380); place(velocityLabel,velocity,170,380);
    place(roomLabel,room,282,380); place(masterLabel,master,394,380);

    voicesLabel.setBounds(510,384,60,18); voices.setBounds(570,384,250,18);
    status.setBounds(42,470,896,26);

    topTune.setTooltip("Batter-head tuning");
    topDamp.setTooltip("Batter-head damping");
    bottomTune.setTooltip("Bottom-head tuning");
    bottomDamp.setTooltip("Bottom-head damping");
    diameter.setTooltip("Virtual shell diameter");
    depth.setTooltip("Virtual shell depth");
    material.setTooltip("Shell material and stiffness");
    resonance.setTooltip("Head-to-shell coupling");
    wireTension.setTooltip("Snare wire tension");
    wireAmount.setTooltip("Snare-wire contribution");
    wireDamping.setTooltip("Wire damping and buzz length");
    stick.setTooltip("Exciter hardness");
    sampleRef.setTooltip("How strongly the reference defines the synthetic exciter");
    velocity.setTooltip("How strongly MIDI velocity changes the physical model");
}
