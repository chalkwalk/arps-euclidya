#include "PluginProcessor.h"
#include "PluginEditor.h"

EuclideanArpProcessor::EuclideanArpProcessor()
     : AudioProcessor(BusesProperties()), // No audio channels for MIDI effect
       apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

EuclideanArpProcessor::~EuclideanArpProcessor()
{
}

juce::AudioProcessorValueTreeState::ParameterLayout EuclideanArpProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Create 32 Macro parameters
    for (int i = 1; i <= 32; ++i) {
        juce::String macroID = "macro_" + juce::String(i);
        juce::String macroName = "Macro " + juce::String(i);
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(macroID, 1),
            macroName,
            0.0f, 1.0f, 0.0f
        ));
    }

    return layout;
}

const juce::String EuclideanArpProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EuclideanArpProcessor::acceptsMidi() const
{
    return true;
}

bool EuclideanArpProcessor::producesMidi() const
{
    return true;
}

bool EuclideanArpProcessor::isMidiEffect() const
{
    return true;
}

double EuclideanArpProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EuclideanArpProcessor::getNumPrograms()
{
    return 1;
}

int EuclideanArpProcessor::getCurrentProgram()
{
    return 0;
}

void EuclideanArpProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String EuclideanArpProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void EuclideanArpProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index);
    juce::ignoreUnused(newName);
}

void EuclideanArpProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void EuclideanArpProcessor::releaseResources()
{
}

bool EuclideanArpProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // MIDI effect without audio in/out
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled()
     || layouts.getMainOutputChannelSet() != juce::AudioChannelSet::disabled())
        return false;

    return true;
}

void EuclideanArpProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(buffer);

    // Step 1: No-Op Passthrough!
    // Simply forwarding the complete incoming midiMessages buffer to the output.
    // We also optionally print to the debug UI pane

    if (auto* pEditor = dynamic_cast<EuclideanArpEditor*>(getActiveEditor()))
    {
        for (const auto metadata : midiMessages)
        {
            const auto message = metadata.getMessage();
            const int channel = message.getChannel();

            if (message.isNoteOn())
            {
                pEditor->printTextToChannel(channel,
                    "Note On: " + juce::String(message.getNoteNumber()) +
                    " V: " + juce::String(message.getVelocity()));
            }
            else if (message.isNoteOff())
            {
                pEditor->printTextToChannel(channel,
                    "Note Off: " + juce::String(message.getNoteNumber()));
            }
            else if (message.isController())
            {
                pEditor->printTextToChannel(channel,
                    "CC " + juce::String(message.getControllerNumber()) +
                    ": " + juce::String(message.getControllerValue()));
            }
        }
    }
}

bool EuclideanArpProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* EuclideanArpProcessor::createEditor()
{
    return new EuclideanArpEditor(*this);
}

void EuclideanArpProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EuclideanArpProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

EuclideanArpEditor* EuclideanArpProcessor::getEditor()
{
    return static_cast<EuclideanArpEditor*>(getActiveEditor());
}

// This creates new instances of the plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EuclideanArpProcessor();
}
