/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Ps1VerbAudioProcessor::Ps1VerbAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
            apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

Ps1VerbAudioProcessor::~Ps1VerbAudioProcessor()
{
}

//==============================================================================
const juce::String Ps1VerbAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool Ps1VerbAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool Ps1VerbAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool Ps1VerbAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double Ps1VerbAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int Ps1VerbAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int Ps1VerbAudioProcessor::getCurrentProgram()
{
    return 0;
}

void Ps1VerbAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String Ps1VerbAudioProcessor::getProgramName (int index)
{
    return {};
}

void Ps1VerbAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void Ps1VerbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    reverb.prepare(sampleRate, samplesPerBlock);
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void Ps1VerbAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Ps1VerbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

// Auto make parameters
juce::AudioProcessorValueTreeState::ParameterLayout
Ps1VerbAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("Preset", 1),
        "Preset",
        juce::StringArray{"Room", "Studio Small", "Studio Medium", "Studio Large", "Hall", "Half Echo", "Space Echo", "Chaos Echo", "Delay", "Sanity"},
        0  // Default to Room (index 1)
    ));
    
    layout.add(std::make_unique<juce::AudioParameterFloat>(
       juce::ParameterID("mix", 1),
       "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.1f),
        1.0));
    
    return layout;
}


void Ps1VerbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    
    const float mix = apvts.getRawParameterValue("mix")->load();
    auto* presetParam = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter("Preset"));
    const juce::String preset = presetParam->getCurrentChoiceName();
    
    if (preset != lastPreset)
    {
        reverb.reset();
        reverb.loadPreset(preset.toStdString());
        lastPreset = preset;
    }

    auto* leftChannel = buffer.getWritePointer(0);
    auto* rightChannel = buffer.getWritePointer(1);
    
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float leftOut, rightOut;
        
        reverb.processStereo(leftChannel[sample], rightChannel[sample], leftOut, rightOut);
        
        
        float left = (1.0f - mix) * leftChannel[sample] + mix * leftOut;
        float right = (1.0f - mix) * rightChannel[sample] + mix * rightOut;
        
        
        leftChannel[sample] = left;
        rightChannel[sample] = right;
        
        
    }
}

//==============================================================================
bool Ps1VerbAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* Ps1VerbAudioProcessor::createEditor()
{
//    return new Ps1VerbAudioProcessorEditor (*this);
    return new juce::GenericAudioProcessorEditor (*this);

}

//==============================================================================
void Ps1VerbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void Ps1VerbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Ps1VerbAudioProcessor();
}
