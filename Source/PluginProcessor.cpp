#include "PluginProcessor.h"
#include "PluginEditor.h"
BlackDrumAudioProcessor::BlackDrumAudioProcessor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)){formats.registerBasicFormats();}
void BlackDrumAudioProcessor::prepareToPlay(double,int){}
bool BlackDrumAudioProcessor::loadSample(const juce::File& f){
 std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(f)); if(!r || r->lengthInSamples<=0 || r->lengthInSamples> (juce::int64)r->sampleRate*120.0) return false;
 juce::AudioBuffer<float> temp((int)r->numChannels,(int)r->lengthInSamples); r->read(&temp,0,(int)r->lengthInSamples,0,true,true);
 const juce::ScopedLock lock(sampleLock); sample=std::move(temp); sourceRate=r->sampleRate; loadedFile=f; playhead=-1; return true;
}
juce::AudioBuffer<float> BlackDrumAudioProcessor::sampleCopy() const {const juce::ScopedLock lock(sampleLock); juce::AudioBuffer<float> c(sample.getNumChannels(),sample.getNumSamples()); c.makeCopyOf(sample); return c;}
void BlackDrumAudioProcessor::processBlock(juce::AudioBuffer<float>& out,juce::MidiBuffer& midi){
 juce::ScopedNoDenormals no; out.clear();
 for(const auto m:midi) if(m.getMessage().isNoteOn()) playhead=0;
 const juce::ScopedLock lock(sampleLock);
 if(sample.getNumSamples()==0) return;
 for(int i=0;i<out.getNumSamples();++i){if(playhead<0 || playhead>=sample.getNumSamples()) break;
  for(int ch=0;ch<out.getNumChannels();++ch){int sc=juce::jmin(ch,sample.getNumChannels()-1); out.setSample(ch,i,sample.getSample(sc,playhead));}
  ++playhead;
 }
}
juce::AudioProcessorEditor* BlackDrumAudioProcessor::createEditor(){return new BlackDrumAudioProcessorEditor(*this);}
void BlackDrumAudioProcessor::getStateInformation(juce::MemoryBlock& d){juce::MemoryOutputStream s(d,true); s.writeString(loadedFile.getFullPathName());}
void BlackDrumAudioProcessor::setStateInformation(const void* data,int size){juce::MemoryInputStream s(data,(size_t)size,false); auto p=s.readString(); if(p.isNotEmpty()) loadSample(juce::File(p));}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new BlackDrumAudioProcessor();}