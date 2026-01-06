#include "PluginProcessor.h"
#include "PluginEditor.h"

EuclideanArpProcessor::EuclideanArpProcessor()
    : AudioProcessor(BusesProperties()), // No audio channels for MIDI effect
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {}

EuclideanArpProcessor::~EuclideanArpProcessor() {}

void EuclideanArpProcessor::logMidiEvent(int type, int channel, int d1,
                                         float d2) {
  if (midiLogFifo.getFreeSpace() > 0) {
    int start1, size1, start2, size2;
    midiLogFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0) {
      midiLogBuffer[(size_t)start1] = {type, channel, d1, d2};
    }
    midiLogFifo.finishedWrite(size1 + size2);
  }
}

juce::AudioProcessorValueTreeState::ParameterLayout
EuclideanArpProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  return layout;
}

const juce::String EuclideanArpProcessor::getName() const {
  return JucePlugin_Name;
}

bool EuclideanArpProcessor::acceptsMidi() const { return true; }

bool EuclideanArpProcessor::producesMidi() const { return true; }

bool EuclideanArpProcessor::isMidiEffect() const { return true; }

double EuclideanArpProcessor::getTailLengthSeconds() const { return 0.0; }

int EuclideanArpProcessor::getNumPrograms() { return 1; }

int EuclideanArpProcessor::getCurrentProgram() { return 0; }

void EuclideanArpProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String EuclideanArpProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void EuclideanArpProcessor::changeProgramName(int index,
                                              const juce::String &newName) {
  juce::ignoreUnused(index);
  juce::ignoreUnused(newName);
}

void EuclideanArpProcessor::prepareToPlay(double sampleRate,
                                          int samplesPerBlock) {
  juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void EuclideanArpProcessor::releaseResources() {}

bool EuclideanArpProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  // MIDI effect without audio in/out
  if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled() ||
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::disabled())
    return false;

  return true;
}

void EuclideanArpProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                         juce::MidiBuffer &midiMessages) {
  juce::ignoreUnused(buffer);

  for (const auto metadata : midiMessages) {
    const auto message = metadata.getMessage();
    const int channel = message.getChannel();

    if (message.isNoteOn()) {
      logMidiEvent(0, channel, message.getNoteNumber(), message.getVelocity());
    } else if (message.isNoteOff()) {
      logMidiEvent(1, channel, message.getNoteNumber(), 0.0f);
    } else if (message.isController()) {
      logMidiEvent(2, channel, message.getControllerNumber(),
                   (float)message.getControllerValue());
    }
  }
}

bool EuclideanArpProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *EuclideanArpProcessor::createEditor() {
  return new EuclideanArpEditor(*this);
}

void EuclideanArpProcessor::getStateInformation(juce::MemoryBlock &destData) {
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void EuclideanArpProcessor::setStateInformation(const void *data,
                                                int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));
  if (xmlState.get() != nullptr)
    if (xmlState->hasTagName(apvts.state.getType()))
      apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

EuclideanArpEditor *EuclideanArpProcessor::getEditor() {
  return dynamic_cast<EuclideanArpEditor *>(getActiveEditor());
}

// This creates new instances of the plugin
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new EuclideanArpProcessor();
}
