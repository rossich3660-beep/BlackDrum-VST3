#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
BlackDrumAudioProcessor::BlackDrumAudioProcessor():AudioProcessor(BusesProperties().withOutput("Output",juce::AudioChannelSet::stereo(),true)){formats.registerBasicFormats();}
void BlackDrumAudioProcessor::prepareToPlay(double rate,int){outputRate=rate>0.0?rate:44100.0; playbackPosition=-1.0; filterState[0]=filterState[1]=0.0f;}
bool BlackDrumAudioProcessor::loadSample(const juce::File& f){
 std::unique_ptr<juce::AudioFormatReader> r(formats.createReaderFor(f)); if(!r || r->lengthInSamples<=0 || r->lengthInSamples>(juce::int64)r->sampleRate*120.0) return false;
 juce::AudioBuffer<float> temp((int)r->numChannels,(int)r->lengthInSamples); r->read(&temp,0,(int)r->lengthInSamples,0,true,true);
 const juce::ScopedLock lock(sampleLock); sample=std::move(temp); sourceRate=r->sampleRate; loadedFile=f; playbackPosition=-1.0; filterState[0]=filterState[1]=0.0f; return true;
}
juce::AudioBuffer<float> BlackDrumAudioProcessor::sampleCopy() const {const juce::ScopedLock lock(sampleLock); juce::AudioBuffer<float> c(sample.getNumChannels(),sample.getNumSamples()); c.makeCopyOf(sample); return c;}
void BlackDrumAudioProcessor::processBlock(juce::AudioBuffer<float>& out,juce::MidiBuffer& midi){
 juce::ScopedNoDenormals no; out.clear();
 const juce::ScopedLock lock(sampleLock);
 if(sample.getNumSamples()==0) return;
 auto event=midi.cbegin(); const auto end=midi.cend();
 const float sr=(float)juce::jmax(1.0,outputRate);
 for(int i=0;i<out.getNumSamples();++i){
  while(event!=end && (*event).samplePosition<=i){
   const auto msg=(*event).getMessage();
   if(msg.isNoteOn()){
    const float v=juce::jlimit(0.0f,1.0f,msg.getFloatVelocity());
    voiceGain=0.12f+0.88f*v*v;
    // Stronger hits are slightly brighter and have a subtly higher attack pitch.
    playbackRate=(float)(sourceRate/outputRate)*(0.985f+0.045f*v);
    const float cutoff=900.0f+v*15500.0f;
    filterCoefficient=1.0f-std::exp(-2.0f*juce::MathConstants<float>::pi*cutoff/sr);
    playbackPosition=0.0; filterState[0]=filterState[1]=0.0f;
   }
   ++event;
  }
  if(playbackPosition<0.0 || playbackPosition>=(double)sample.getNumSamples()) continue;
  const int idx=(int)playbackPosition, next=juce::jmin(idx+1,sample.getNumSamples()-1);
  const float frac=(float)(playbackPosition-idx);
  const float progress=(float)(playbackPosition/juce::jmax(1.0,sourceRate*0.035));
  const float transient=1.0f+(0.10f+0.22f*(voiceGain-0.12f)/0.88f)*std::exp(-progress*3.0f);
  for(int ch=0;ch<out.getNumChannels();++ch){
   const int sc=juce::jmin(ch,sample.getNumChannels()-1);
   const float a=sample.getSample(sc,idx), b=sample.getSample(sc,next);
   const float raw=a+(b-a)*frac;
   const int fc=juce::jmin(ch,1);
   filterState[fc]+=filterCoefficient*(raw-filterState[fc]);
   const float shaped=std::tanh(filterState[fc]*voiceGain*transient*1.15f);
   out.setSample(ch,i,shaped);
  }
  playbackPosition+=playbackRate;
 }
}
juce::AudioProcessorEditor* BlackDrumAudioProcessor::createEditor(){return new BlackDrumAudioProcessorEditor(*this);}
void BlackDrumAudioProcessor::getStateInformation(juce::MemoryBlock& d){juce::MemoryOutputStream s(d,true); s.writeString(loadedFile.getFullPathName());}
void BlackDrumAudioProcessor::setStateInformation(const void* data,int size){juce::MemoryInputStream s(data,(size_t)size,false); auto p=s.readString(); if(p.isNotEmpty()) loadSample(juce::File(p));}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new BlackDrumAudioProcessor();}