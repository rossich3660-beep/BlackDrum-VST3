#include "PluginEditor.h"
#include <cmath>

namespace
{
static constexpr int kFeatureCount = 12;
static constexpr int kAutoCount = 9;

float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f); }

void styleLabel(juce::Label& label, const juce::String& text, float size = 11.0f)
{
    label.setText(text, juce::dontSendNotification);
    label.setColour(juce::Label::textColourId, juce::Colour(0xffd9e7ee));
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::Font(juce::FontOptions(size, juce::Font::plain)));
}

void styleKnob(juce::Slider& slider, float value)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 16);
    slider.setRange(0.0, 1.0, 0.01);
    slider.setValue(value, juce::dontSendNotification);
    slider.setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xff59c7ff));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff24485a));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffa9c4d0));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff18313d));
}

void drawSection(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& name)
{
    g.setColour(juce::Colour(0xff0b171f));
    g.fillRoundedRectangle(r, 14.0f);
    g.setColour(juce::Colour(0xff1e5268));
    g.drawRoundedRectangle(r, 14.0f, 1.0f);
    g.setColour(juce::Colour(0xff72d4ff));
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText(name, r.getX() + 16.0f, r.getY() + 9.0f, 250.0f, 20.0f, juce::Justification::left);
}
}

struct BlackDrumAudioProcessorEditor::AnalysisJob : public juce::ThreadPoolJob
{
    AnalysisJob(juce::WeakReference<BlackDrumAudioProcessorEditor> e,
                juce::AudioBuffer<float> b, double rate)
        : ThreadPoolJob("BlackSnare Sample Analysis"), editor(e), source(std::move(b)), sourceRate(rate) {}

    JobStatus runJob() override
    {
        auto e = editor;
        if (e == nullptr)
            return jobHasFinished;

        auto mono = SampleAnalyzer::makeAnalysisCopy(source, sourceRate);
        SampleFeatures features;
        AutoSettings settings;

        const bool validLength = mono.getNumSamples() >= (int)std::llround(44100.0 * 0.020);
        if (validLength)
        {
            features = SampleAnalyzer::analyze(mono, 44100.0);
            settings = makeAutoSettings(features);
        }

        const float featureValues[kFeatureCount] = {
            features.durationMs, features.attackMs, features.decayT60Ms, features.tailEnergyRatio,
            features.centroidHz, features.bodyEnergy, features.snapEnergy, features.airEnergy,
            features.fundamentalHz, features.toneToNoise, features.ringAmount, features.crestFactorDb
        };
        const float autoValues[kAutoCount] = {
            settings.brightnessAmt, settings.snapAmt, settings.noiseLayerAmt,
            settings.attackSoftening, settings.tailShorten, settings.pitchDropAmt,
            settings.saturationAmt, settings.bodyBoostAmt, settings.velocityCurve
        };

        for (int i = 0; i < kFeatureCount; ++i)
            e->pendingFeatures[(size_t)i].store(std::isfinite(featureValues[i]) ? featureValues[i] : 0.0f);
        for (int i = 0; i < kAutoCount; ++i)
            e->pendingAuto[(size_t)i].store(clamp01(autoValues[i]));

        e->analysisReady.store(true);
        e->analysisRunning.store(false);
        e->triggerAsyncUpdate();
        return jobHasFinished;
    }

    juce::WeakReference<BlackDrumAudioProcessorEditor> editor;
    juce::AudioBuffer<float> source;
    double sourceRate = 44100.0;
};

BlackDrumAudioProcessorEditor::BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p), analysisPool(std::make_unique<juce::ThreadPool>(1))
{
    setSize(1180, 820);
    setResizable(true, true);

    title.setText("BlackSnare", juce::dontSendNotification);
    title.setFont(juce::Font(juce::FontOptions(34.0f, juce::Font::bold)));
    title.setColour(juce::Label::textColourId, juce::Colour(0xffedf8ff));
    addAndMakeVisible(title);

    styleLabel(subtitle, "ONE-SHOT SNARE RESYNTHESIS  /  AUTO VELOCITY MODEL", 11.0f);
    subtitle.setJustificationType(juce::Justification::left);
    addAndMakeVisible(subtitle);

    styleLabel(sampleLabel, "NO SAMPLE", 12.0f);
    addAndMakeVisible(sampleLabel);
    styleLabel(analysisStatus, "READY", 11.0f);
    analysisStatus.setColour(juce::Label::textColourId, juce::Colour(0xff63d6ff));
    addAndMakeVisible(analysisStatus);
    styleLabel(featureSummary, "No analysis yet", 10.0f);
    addAndMakeVisible(featureSummary);
    styleLabel(hint, "DROP WAV / AIFF / FLAC / OGG HERE", 10.0f);
    hint.setColour(juce::Label::textColourId, juce::Colour(0xff7e9ba8));
    addAndMakeVisible(hint);

    auto setupEngineKnob = [this](juce::Slider& s, juce::Label& l, const juce::String& name, float v, std::function<void(float)> setter)
    {
        styleLabel(l, name);
        styleKnob(s, v);
        s.onValueChange = [this, &s, setter] { setter((float)s.getValue()); processor.setManualAutoEdits(true); };
        addAndMakeVisible(l);
        addAndMakeVisible(s);
    };

    setupEngineKnob(bodyMixSlider, bodyMixLabel, "BODY MIX", processor.getBodyMix(), [this](float v){ processor.setBodyMix(v); });
    setupEngineKnob(spectralSlider, spectralLabel, "SPECTRAL", processor.getSpectralMix(), [this](float v){ processor.setSpectralMix(v); });
    setupEngineKnob(wireSlider, wireLabel, "WIRE NOISE", processor.getWireNoiseMix(), [this](float v){ processor.setWireNoiseMix(v); });
    setupEngineKnob(phaseVocoderSlider, phaseVocoderLabel, "PHASE MORPH", processor.getPhaseVocoderMix(), [this](float v){ processor.setPhaseVocoderMix(v); });
    setupEngineKnob(shellSlider, shellLabel, "SHELL", processor.getShellResonanceMix(), [this](float v){ processor.setShellResonanceMix(v); });
    setupEngineKnob(attackSlider, attackLabel, "ATTACK", processor.getTransient(), [this](float v){ processor.setTransient(v); });
    setupEngineKnob(sustainSlider, sustainLabel, "SUSTAIN", processor.getSustain(), [this](float v){ processor.setSustain(v); });
    setupEngineKnob(dynamicSlider, dynamicLabel, "DYNAMIC", processor.getDynamicResponse(), [this](float v){ processor.setDynamicResponse(v); });
    setupEngineKnob(tensionSlider, tensionLabel, "TENSION", processor.getMembraneTension(), [this](float v){ processor.setMembraneTension(v); });
    setupEngineKnob(stiffnessSlider, stiffnessLabel, "STIFFNESS", processor.getMembraneStiffness(), [this](float v){ processor.setMembraneStiffness(v); });
    setupEngineKnob(decaySlider, decayLabel, "DECAY", processor.getMembraneDecay(), [this](float v){ processor.setMembraneDecay(v); });
    setupEngineKnob(velocitySlider, velocityLabel, "VELOCITY", processor.getMembraneVelocity(), [this](float v){ processor.setMembraneVelocity(v); });

    styleLabel(membraneTitle, "MEMBRANE MODEL", 13.0f);
    membraneTitle.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(membraneTitle);
    membraneToggle.setToggleState(processor.getMembraneEnabled(), juce::dontSendNotification);
    membraneToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffc8dbe4));
    membraneToggle.onClick = [this] { processor.setMembraneEnabled(membraneToggle.getToggleState()); processor.setManualAutoEdits(true); };
    addAndMakeVisible(membraneToggle);

    styleLabel(autoTitle, "AUTO RESYNTHESIS", 13.0f);
    autoTitle.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    addAndMakeVisible(autoTitle);
    styleLabel(autoHint, "Analysis is background-only. AUTO re-runs it safely.", 10.0f);
    autoHint.setJustificationType(juce::Justification::left);
    addAndMakeVisible(autoHint);

    AutoSettings as = processor.getAutoSettings();
    const char* names[kAutoCount] = { "BRIGHTNESS", "SNAP", "NOISE LAYER", "ATTACK SOFTEN", "TAIL SHORTEN", "PITCH DROP", "SATURATION", "BODY BOOST", "VELOCITY CURVE" };
    juce::Label* labels[kAutoCount] = { &brightnessLabel, &snapLabel, &noiseLayerLabel, &attackSoftLabel, &tailShortenLabel, &pitchDropLabel, &saturationLabel, &bodyBoostLabel, &velocityCurveLabel };
    juce::Slider* sliders[kAutoCount] = { &brightnessSlider, &snapSlider, &noiseLayerSlider, &attackSoftSlider, &tailShortenSlider, &pitchDropSlider, &saturationSlider, &bodyBoostSlider, &velocityCurveSlider };
    float values[kAutoCount] = { as.brightnessAmt, as.snapAmt, as.noiseLayerAmt, as.attackSoftening, as.tailShorten, as.pitchDropAmt, as.saturationAmt, as.bodyBoostAmt, as.velocityCurve };
    for (int i = 0; i < kAutoCount; ++i)
    {
        styleLabel(*labels[i], names[i], 10.0f);
        styleKnob(*sliders[i], values[i]);
        addAndMakeVisible(*labels[i]);
        addAndMakeVisible(*sliders[i]);
    }

    auto setAutoSlider = [this](juce::Slider& slider, int index)
    {
        slider.onValueChange = [this, &slider, index]
        {
            auto s = processor.getAutoSettings();
            const float v = (float)slider.getValue();
            switch (index)
            {
                case 0: s.brightnessAmt = v; break; case 1: s.snapAmt = v; break; case 2: s.noiseLayerAmt = v; break;
                case 3: s.attackSoftening = v; break; case 4: s.tailShorten = v; break; case 5: s.pitchDropAmt = v; break;
                case 6: s.saturationAmt = v; break; case 7: s.bodyBoostAmt = v; break; default: s.velocityCurve = v; break;
            }
            processor.applyAutoSettings(s, true);
        };
    };
    for (int i = 0; i < kAutoCount; ++i)
        setAutoSlider(*sliders[i], i);

    autoButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff174e68));
    autoButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    autoButton.onClick = [this] { startAnalysis(true); };
    addAndMakeVisible(autoButton);

    voiceCountLabel.setText("VOICES", juce::dontSendNotification);
    styleLabel(voiceCountLabel, "VOICES", 10.0f);
    voiceCountSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    voiceCountSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 18);
    voiceCountSlider.setRange(1, 16, 1);
    voiceCountSlider.setValue(processor.getVoiceCount(), juce::dontSendNotification);
    voiceCountSlider.setColour(juce::Slider::thumbColourId, juce::Colour(0xff59c7ff));
    voiceCountSlider.onValueChange = [this] { processor.setVoiceCount((int)voiceCountSlider.getValue()); processor.setManualAutoEdits(true); };
    addAndMakeVisible(voiceCountSlider);
    addAndMakeVisible(voiceCountLabel);

    for (auto* b : { &loadButton, &playButton, &removeButton, &savePresetButton, &loadPresetButton })
    {
        addAndMakeVisible(*b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff111f27));
        b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffd8eaf2));
        b->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff1a5269));
    }

    loadButton.onClick = [this]
    {
        if (fileChooser != nullptr) return;
        fileChooser = std::make_unique<juce::FileChooser>("Choose snare sample", juce::File{}, "*.wav;*.aiff;*.aif;*.flac;*.ogg");
        juce::Component::SafePointer<BlackDrumAudioProcessorEditor> safeThis(this);
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr) return;
                const auto file = chooser.getResult();
                safeThis->fileChooser.reset();
                if (file.existsAsFile()) safeThis->loadFrom(file);
            });
    };

    playButton.onClick = [this] { hint.setText("Use MIDI to audition the current sample.", juce::dontSendNotification); };
    removeButton.onClick = [this] { hint.setText("Sample stays loaded until another sample is selected.", juce::dontSendNotification); };

    savePresetButton.onClick = [this]
    {
        if (presetChooser != nullptr) return;
        presetChooser = std::make_unique<juce::FileChooser>("Save BlackSnare preset", juce::File{}, "*.blackdrum");
        juce::Component::SafePointer<BlackDrumAudioProcessorEditor> safeThis(this);
        presetChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [safeThis](const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr) return;
                auto file = chooser.getResult();
                safeThis->presetChooser.reset();
                if (file != juce::File{})
                {
                    if (!file.hasFileExtension(".blackdrum")) file = file.withFileExtension(".blackdrum");
                    safeThis->savePresetTo(file);
                }
            });
    };

    loadPresetButton.onClick = [this]
    {
        if (presetChooser != nullptr) return;
        presetChooser = std::make_unique<juce::FileChooser>("Load BlackSnare preset", juce::File{}, "*.blackdrum");
        juce::Component::SafePointer<BlackDrumAudioProcessorEditor> safeThis(this);
        presetChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safeThis](const juce::FileChooser& chooser)
            {
                if (safeThis == nullptr) return;
                const auto file = chooser.getResult();
                safeThis->presetChooser.reset();
                if (file.existsAsFile()) safeThis->loadPresetFrom(file);
            });
    };

    if (processor.sampleName().isNotEmpty())
    {
        sampleLabel.setText(processor.sampleName(), juce::dontSendNotification);
        startAnalysis(false);
    }
}

BlackDrumAudioProcessorEditor::~BlackDrumAudioProcessorEditor()
{
    cancelPendingUpdate();
    if (analysisPool != nullptr)
        analysisPool->removeAllJobs(true, 2000);
}

void BlackDrumAudioProcessorEditor::startAnalysis(bool forceApply)
{
    if (analysisRunning.exchange(true))
        return;

    auto source = processor.sampleCopy();
    if (source.getNumSamples() <= 0)
    {
        analysisRunning.store(false);
        analysisStatus.setText("NO SAMPLE", juce::dontSendNotification);
        return;
    }

    analysisForceApply = forceApply;
    analysisReady.store(false);
    analysisStatus.setText("ANALYZING…", juce::dontSendNotification);
    autoButton.setEnabled(false);
    analysisPool->addJob(new AnalysisJob(juce::WeakReference<BlackDrumAudioProcessorEditor>(this),
                                         std::move(source), processor.sampleRateOfFile()), true);
}

void BlackDrumAudioProcessorEditor::handleAsyncUpdate()
{
    if (!analysisReady.exchange(false))
        return;

    SampleFeatures f;
    f.durationMs = pendingFeatures[0].load();
    f.attackMs = pendingFeatures[1].load();
    f.decayT60Ms = pendingFeatures[2].load();
    f.tailEnergyRatio = clamp01(pendingFeatures[3].load());
    f.centroidHz = pendingFeatures[4].load();
    f.bodyEnergy = clamp01(pendingFeatures[5].load());
    f.snapEnergy = clamp01(pendingFeatures[6].load());
    f.airEnergy = clamp01(pendingFeatures[7].load());
    f.fundamentalHz = pendingFeatures[8].load();
    f.toneToNoise = clamp01(pendingFeatures[9].load());
    f.ringAmount = clamp01(pendingFeatures[10].load());
    f.crestFactorDb = pendingFeatures[11].load();

    AutoSettings s;
    s.brightnessAmt = clamp01(pendingAuto[0].load());
    s.snapAmt = clamp01(pendingAuto[1].load());
    s.noiseLayerAmt = clamp01(pendingAuto[2].load());
    s.attackSoftening = clamp01(pendingAuto[3].load());
    s.tailShorten = clamp01(pendingAuto[4].load());
    s.pitchDropAmt = clamp01(pendingAuto[5].load());
    s.saturationAmt = clamp01(pendingAuto[6].load());
    s.bodyBoostAmt = clamp01(pendingAuto[7].load());
    s.velocityCurve = clamp01(pendingAuto[8].load());

    processor.setSampleFeatures(f);
    const bool shouldApply = analysisForceApply || !processor.hasManualAutoEdits();
    if (shouldApply)
    {
        processor.applyAutoSettings(s, false);
        syncAutoControls(s);
        processor.setManualAutoEdits(false);
    }

    analysisStatus.setText(shouldApply ? "AUTO APPLIED" : "ANALYSIS READY / MANUAL", juce::dontSendNotification);
    featureSummary.setText(
        "ATT " + juce::String(f.attackMs, 1) + " ms   •   DECAY " + juce::String(f.decayT60Ms, 0) +
        " ms   •   CENT " + juce::String(f.centroidHz, 0) + " Hz   •   BODY " + juce::String(f.bodyEnergy, 2) +
        "   •   SNAP " + juce::String(f.snapEnergy, 2) + "   •   AIR " + juce::String(f.airEnergy, 2),
        juce::dontSendNotification);

    autoButton.setEnabled(true);
    repaint();
}

void BlackDrumAudioProcessorEditor::syncAutoControls(const AutoSettings& s)
{
    brightnessSlider.setValue(s.brightnessAmt, juce::dontSendNotification);
    snapSlider.setValue(s.snapAmt, juce::dontSendNotification);
    noiseLayerSlider.setValue(s.noiseLayerAmt, juce::dontSendNotification);
    attackSoftSlider.setValue(s.attackSoftening, juce::dontSendNotification);
    tailShortenSlider.setValue(s.tailShorten, juce::dontSendNotification);
    pitchDropSlider.setValue(s.pitchDropAmt, juce::dontSendNotification);
    saturationSlider.setValue(s.saturationAmt, juce::dontSendNotification);
    bodyBoostSlider.setValue(s.bodyBoostAmt, juce::dontSendNotification);
    velocityCurveSlider.setValue(s.velocityCurve, juce::dontSendNotification);
}

void BlackDrumAudioProcessorEditor::syncEngineControls()
{
    voiceCountSlider.setValue(processor.getVoiceCount(), juce::dontSendNotification);
    bodyMixSlider.setValue(processor.getBodyMix(), juce::dontSendNotification);
    spectralSlider.setValue(processor.getSpectralMix(), juce::dontSendNotification);
    wireSlider.setValue(processor.getWireNoiseMix(), juce::dontSendNotification);
    phaseVocoderSlider.setValue(processor.getPhaseVocoderMix(), juce::dontSendNotification);
    shellSlider.setValue(processor.getShellResonanceMix(), juce::dontSendNotification);
    attackSlider.setValue(processor.getTransient(), juce::dontSendNotification);
    sustainSlider.setValue(processor.getSustain(), juce::dontSendNotification);
    dynamicSlider.setValue(processor.getDynamicResponse(), juce::dontSendNotification);
    tensionSlider.setValue(processor.getMembraneTension(), juce::dontSendNotification);
    stiffnessSlider.setValue(processor.getMembraneStiffness(), juce::dontSendNotification);
    decaySlider.setValue(processor.getMembraneDecay(), juce::dontSendNotification);
    velocitySlider.setValue(processor.getMembraneVelocity(), juce::dontSendNotification);
    membraneToggle.setToggleState(processor.getMembraneEnabled(), juce::dontSendNotification);
}

void BlackDrumAudioProcessorEditor::savePresetTo(const juce::File& file)
{
    if (auto xml = processor.createPresetState().createXml())
    {
        xml->setAttribute("presetName", file.getFileNameWithoutExtension());
        hint.setText(file.replaceWithText(xml->toString()) ? "PRESET SAVED" : "PRESET SAVE FAILED", juce::dontSendNotification);
    }
}

void BlackDrumAudioProcessorEditor::loadPresetFrom(const juce::File& file)
{
    if (auto xml = juce::parseXML(file))
    {
        if (processor.applyPresetState(juce::ValueTree::fromXml(*xml)))
        {
            sampleLabel.setText(processor.sampleName().isNotEmpty() ? processor.sampleName() : "NO SAMPLE", juce::dontSendNotification);
            syncEngineControls();
            syncAutoControls(processor.getAutoSettings());
            featureSummary.setText("Preset features restored.", juce::dontSendNotification);
            hint.setText("PRESET LOADED", juce::dontSendNotification);
            return;
        }
    }
    hint.setText("INVALID PRESET", juce::dontSendNotification);
}

bool BlackDrumAudioProcessorEditor::isSupportedAudioFile(const juce::File& file) const
{
    const auto ext = file.getFileExtension().toLowerCase();
    return ext == ".wav" || ext == ".aif" || ext == ".aiff" || ext == ".flac" || ext == ".ogg";
}

void BlackDrumAudioProcessorEditor::loadFrom(const juce::File& file)
{
    if (!isSupportedAudioFile(file))
    {
        hint.setText("UNSUPPORTED AUDIO FILE", juce::dontSendNotification);
        return;
    }

    if (processor.loadSample(file))
    {
        sampleLabel.setText(file.getFileName(), juce::dontSendNotification);
        processor.setManualAutoEdits(false);
        hint.setText("SAMPLE LOADED — STARTING ANALYSIS", juce::dontSendNotification);
        startAnalysis(true);
    }
    else
    {
        hint.setText("COULD NOT LOAD SAMPLE", juce::dontSendNotification);
    }
}

bool BlackDrumAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isSupportedAudioFile(juce::File(path))) return true;
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
    g.fillAll(juce::Colour(0xff050b10));

    g.setColour(juce::Colour(0xff0b1820));
    for (int x = 0; x < getWidth(); x += 24) g.drawVerticalLine(x, 0.0f, (float)getHeight());
    for (int y = 0; y < getHeight(); y += 24) g.drawHorizontalLine(y, 0.0f, (float)getWidth());

    g.setColour(juce::Colour(0xff14384a));
    g.drawLine(24.0f, 104.0f, (float)getWidth() - 24.0f, 104.0f, 1.0f);

    g.setColour(juce::Colour(0xff0b131a));
    g.fillRoundedRectangle(22.0f, 118.0f, (float)getWidth() - 44.0f, 270.0f, 16.0f);
    g.setColour(juce::Colour(0xff1b4b60));
    g.drawRoundedRectangle(22.0f, 118.0f, (float)getWidth() - 44.0f, 270.0f, 16.0f, 1.0f);
    g.setColour(juce::Colour(0xff0a1319));
    g.fillRoundedRectangle(22.0f, 402.0f, (float)getWidth() - 44.0f, 286.0f, 16.0f);
    g.setColour(juce::Colour(0xff173d4f));
    g.drawRoundedRectangle(22.0f, 402.0f, (float)getWidth() - 44.0f, 286.0f, 16.0f, 1.0f);

    drawSection(g, { 36.0f, 126.0f, 700.0f, 254.0f }, "AUTO RESYNTHESIS");
    drawSection(g, { 750.0f, 126.0f, 420.0f, 254.0f }, "SAMPLE ANALYSIS");

    g.setColour(juce::Colour(0xff0c1820));
    g.fillRoundedRectangle(dropArea.toFloat(), 10.0f);
    g.setColour(dragHover ? juce::Colour(0xff5fd5ff) : juce::Colour(0xff24566c));
    g.drawRoundedRectangle(dropArea.toFloat(), 10.0f, dragHover ? 2.0f : 1.0f);
    g.setColour(juce::Colour(0xff80b8c9));
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("DROP SAMPLE", dropArea.reduced(0, 8).withHeight(18), juce::Justification::centred);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.setColour(juce::Colour(0xff668592));
    g.drawText("MONO ANALYSIS COPY • 44.1 kHz • ORIGINAL UNCHANGED", dropArea.reduced(0, 28).withHeight(18), juce::Justification::centred);

    g.setColour(juce::Colour(0xff75d8ff));
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText("PHYSICAL ENGINE", 38, 410, 220, 20, juce::Justification::left);
    g.setColour(juce::Colour(0xff203e4b));
    g.drawLine(38.0f, 438.0f, (float)getWidth() - 38.0f, 438.0f, 1.0f);

    g.setColour(juce::Colour(0xff526e79));
    g.setFont(juce::Font(juce::FontOptions(9.0f)));
    g.drawText("ONE SAMPLE  /  MIDI VELOCITY  /  PHYSICAL MODEL + SPECTRAL LAYERS", 38, 700, 700, 16, juce::Justification::left);
}

void BlackDrumAudioProcessorEditor::resized()
{
    const int w = getWidth();
    const int knob = 54;

    title.setBounds(34, 20, 320, 42);
    subtitle.setBounds(38, 62, 460, 22);
    analysisStatus.setBounds(w - 210, 26, 170, 22);
    sampleLabel.setBounds(w - 500, 58, 300, 22);

    auto place = [&](juce::Label& l, juce::Slider& s, int x, int y, int cellW)
    {
        l.setBounds(x, y, cellW, 20);
        s.setBounds(x + (cellW - knob) / 2, y + 20, knob, 76);
    };

    const int ax = 50, ay = 166, aw = 645;
    const int col = aw / 3;
    juce::Label* al[9] = { &brightnessLabel,&snapLabel,&noiseLayerLabel,&attackSoftLabel,&tailShortenLabel,&pitchDropLabel,&saturationLabel,&bodyBoostLabel,&velocityCurveLabel };
    juce::Slider* as[9] = { &brightnessSlider,&snapSlider,&noiseLayerSlider,&attackSoftSlider,&tailShortenSlider,&pitchDropSlider,&saturationSlider,&bodyBoostSlider,&velocityCurveSlider };
    for (int i = 0; i < 9; ++i)
        place(*al[i], *as[i], ax + (i % 3) * col, ay + (i / 3) * 72, col);

    autoButton.setBounds(596, 132, 105, 28);

    featureSummary.setBounds(770, 160, 380, 70);
    featureSummary.setJustificationType(juce::Justification::centred);
    dropArea = { 780, 250, 360, 76 };
    hint.setBounds(770, 334, 380, 20);

    const int ex = 50, ey = 456, ew = 108;
    juce::Label* el[12] = { &bodyMixLabel,&spectralLabel,&wireLabel,&phaseVocoderLabel,&shellLabel,&attackLabel,&sustainLabel,&dynamicLabel,&tensionLabel,&stiffnessLabel,&decayLabel,&velocityLabel };
    juce::Slider* es[12] = { &bodyMixSlider,&spectralSlider,&wireSlider,&phaseVocoderSlider,&shellSlider,&attackSlider,&sustainSlider,&dynamicSlider,&tensionSlider,&stiffnessSlider,&decaySlider,&velocitySlider };
    for (int i = 0; i < 12; ++i)
        place(*el[i], *es[i], ex + (i % 6) * ew, ey + (i / 2) * 108, ew);

    membraneTitle.setBounds(700, 458, 180, 20);
    membraneToggle.setBounds(860, 452, 180, 26);
    voiceCountLabel.setBounds(920, 468, 65, 20);
    voiceCountSlider.setBounds(985, 468, 150, 22);

    loadButton.setBounds(40, 732, 170, 34);
    playButton.setBounds(220, 732, 150, 34);
    removeButton.setBounds(380, 732, 150, 34);
    savePresetButton.setBounds(w - 360, 732, 155, 34);
    loadPresetButton.setBounds(w - 195, 732, 155, 34);
}
