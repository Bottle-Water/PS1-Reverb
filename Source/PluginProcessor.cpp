/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"

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
    // Long enough to cover the decay of the largest presets (Space Echo / Hall).
    return 3.0;
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
        juce::StringArray{"Room", "Studio Small", "Studio Medium", "Studio Large", "Hall", "Half Echo", "Space Echo", "Chaos Echo", "Delay"},
        4  // Default to Hall (index 4)
    ));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
       juce::ParameterID("mix", 1),
       "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f),
        1.0f));

    // Safe extensions (scale existing values / sit outside the SPU model)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("inGain", 1), "Input",
        juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("outGain", 1), "Output",
        juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("width", 1), "Width",
        juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("preDelay", 1), "Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 200.0f), 0.0f));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("hfDamp", 1), "HF Damp",
        juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

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

    Reverb::UserParams up;
    up.inGain     = apvts.getRawParameterValue("inGain")->load();
    up.outGain    = apvts.getRawParameterValue("outGain")->load();
    up.width      = apvts.getRawParameterValue("width")->load();
    up.preDelayMs = apvts.getRawParameterValue("preDelay")->load();
    up.hfDamp     = apvts.getRawParameterValue("hfDamp")->load();
    reverb.setUserParams(up);

    auto* leftChannel  = buffer.getWritePointer(0);
    // Support a mono main bus: fall back to the left channel for the right.
    auto* rightChannel = totalNumInputChannels > 1 ? buffer.getWritePointer(1) : leftChannel;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        float leftOut, rightOut;

        reverb.processStereo(leftChannel[sample], rightChannel[sample], leftOut, rightOut);

        const float left  = (1.0f - mix) * leftChannel[sample]  + mix * leftOut;
        const float right = (1.0f - mix) * rightChannel[sample] + mix * rightOut;

        leftChannel[sample]  = left;
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
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void Ps1VerbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Persist the full APVTS (preset choice + mix) with the host session.
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void Ps1VerbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Ps1VerbAudioProcessor();
}
