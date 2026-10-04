#include "PluginEditor.h"
BlackDrumAudioProcessorEditor::BlackDrumAudioProcessorEditor(BlackDrumAudioProcessor& p):AudioProcessorEditor(&p),processor(p){
 setSize(620,390); title.setText("BLACKDRUM",juce::dontSendNotification); title.setFont(juce::Font(25.0f,juce::Font::bold)); title.setColour(juce::Label::textColourId,juce::Colours::white); addAndMakeVisible(title);
 filename.setJustificationType(juce::Justification::centred); filename.setColour(juce::Label::textColourId,juce::Colour(0xffeeeeee)); addAndMakeVisible(filename);
 hint.setText("Drop an audio file here  •  WAV / AIFF / FLAC / OGG",juce::dontSendNotification); hint.setJustificationType(juce::Justification::centred); hint.setColour(juce::Label::textColourId,juce::Colour(0xff929292)); addAndMakeVisible(hint);
 for(auto* b:{&loadButton,&playButton,&removeButton}){addAndMakeVisible(b); b->setColour(juce::TextButton::buttonColourId,juce::Colour(0xff292929)); b->setColour(juce::TextButton::textColourOffId,juce::Colours::white);}
 loadButton.onClick=[this]{juce::FileChooser c("Choose a sample",juce::File{},"*.wav;*.aiff;*.aif;*.flac;*.ogg"); c.launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[this](const juce::FileChooser& fc){if(fc.getResult().existsAsFile()) loadFrom(fc.getResult());});};
 playButton.onClick=[this]{juce::MidiBuffer mb; mb.addEvent(juce::MidiMessage::noteOn(1,60,100),0); /* preview via MIDI host; sample preview UI is added later */};
 removeButton.onClick=[this]{filename.setText("No sample loaded",juce::dontSendNotification);};
}
void BlackDrumAudioProcessorEditor::loadFrom(const juce::File& f){if(processor.loadSample(f)) filename.setText(f.getFileName()+"  •  "+juce::String(f.getSize()/1024)+" KB",juce::dontSendNotification);}
bool BlackDrumAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& f){return f.size()>0;}
void BlackDrumAudioProcessorEditor::filesDropped(const juce::StringArray& f,int,int){if(f.size()) loadFrom(juce::File(f[0]));}
void BlackDrumAudioProcessorEditor::paint(juce::Graphics& g){
 g.fillAll(juce::Colour(0xff111111)); auto b=getLocalBounds().toFloat(); g.setColour(juce::Colour(0xff1b1b1b)); g.fillRoundedRectangle(b.reduced(14),14);
 g.setColour(juce::Colour(0xff303030)); g.drawRoundedRectangle(b.reduced(14),14,1.0f);
 auto area=juce::Rectangle<float>(35,112,getWidth()-70,190); g.setColour(juce::Colour(0xff151515)); g.fillRoundedRectangle(area,12);
 g.setColour(juce::Colour(0xff484848)); g.drawRoundedRectangle(area,12,1.5f);
 g.setColour(juce::Colour(0xffbdbdbd)); g.drawEllipse(getWidth()/2.0f-22,145,44,44,2.0f); g.drawLine(getWidth()/2.0f,155,getWidth()/2.0f,179,2.0f); g.drawLine(getWidth()/2.0f-8,170,getWidth()/2.0f,179,2.0f); g.drawLine(getWidth()/2.0f+8,170,getWidth()/2.0f,179,2.0f);
}
void BlackDrumAudioProcessorEditor::resized(){auto r=getLocalBounds(); title.setBounds(35,28,300,45); dropArea=r.withTrimmedTop(110).withTrimmedBottom(90); hint.setBounds(45,215,getWidth()-90,30); filename.setBounds(45,260,getWidth()-90,25); loadButton.setBounds(65,325,150,38); playButton.setBounds(235,325,150,38); removeButton.setBounds(405,325,150,38);}