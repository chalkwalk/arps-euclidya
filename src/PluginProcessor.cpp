#include "PluginProcessor.h"
#include "MacroParameter.h"
#include "MidiInNode.h"
#include "MidiOutNode.h"
#include "PluginEditor.h"
#include "ReverseNode.h"
#include "SortNode.h"

EuclideanArpProcessor::EuclideanArpProcessor()
    : AudioProcessor(BusesProperties()), // No audio channels for MIDI effect
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {

  // Fetch raw parameter pointers and MacroParameter* for RT thread
  for (int i = 0; i < 32; ++i) {
    macros[i] = apvts.getRawParameterValue("macro_" + juce::String(i + 1));
    macroParams[i] = dynamic_cast<MacroParameter *>(
        apvts.getParameter("macro_" + juce::String(i + 1)));
  }

  // Create default graph (Midi In -> Sort -> Midi Out)
  auto inNode = std::make_shared<MidiInNode>(midiHandler, macros);
  auto sortNode = std::make_shared<SortNode>();
  graphEngine.addNode(inNode);
  graphEngine.addNode(sortNode);
  graphEngine.addNode(
      std::make_shared<MidiOutNode>(midiHandler, clockManager, macros));
}

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

  for (int i = 1; i <= 32; ++i) {
    layout.add(std::make_unique<MacroParameter>(i));
  }

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
  // Ensure no audio passes through
  buffer.clear();

  // Step 2: Update Clock and process incoming MPE/Note states
  clockManager.update(getPlayHead(), buffer.getNumSamples(), getSampleRate());

  static bool hasLoggedParams = false;
  if (!hasLoggedParams) {
    logMidiEvent(6, 0, buffer.getNumSamples(), getSampleRate());
    hasLoggedParams = true;
  }

  if (clockManager.isTick()) {
    // Send a ping to our UI console
    logMidiEvent(3, 0, 0, 0.0f);
  }

  // Inject UI-generated synthetic notes and extract DAW MIDI to update the
  // on-screen keyboard
  keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(),
                                      true);

  midiHandler.processMidi(midiMessages);

  {
    const juce::ScopedLock sl(graphLock);

    // Instantaneous graph recalculation
    if (midiHandler.hasChanged()) {
      graphEngine.recalculate();
    }

    // Clear input, we are replacing the midi stream with our engine's output
    midiMessages.clear();

    // The output node evaluates the tick and sequence cache to build the new
    // buffer. Find all MidiOutNodes in the graph and let them emit.
    for (const auto &node : graphEngine.getNodes()) {
      if (auto *outNode = dynamic_cast<MidiOutNode *>(node.get())) {
        outNode->generateMidi(midiMessages, 0);
      }
    }
  }

  // Log the *output* MIDI to the UI FIFO using the user's out-of-band edit
  for (const auto metadata : midiMessages) {
    const auto message = metadata.getMessage();
    const int channel = message.getChannel();

    if (message.isNoteOn()) {
      logMidiEvent(4, channel, message.getNoteNumber(), message.getVelocity());
    } else if (message.isNoteOff()) {
      logMidiEvent(5, channel, message.getNoteNumber(), 0.0f);
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
  juce::XmlElement xmlRoot("EuclideanArpState");

  // Save APVTS Macros
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> apvtsXml(state.createXml());
  if (apvtsXml != nullptr) {
    auto *wrapper = xmlRoot.createNewChildElement("APVTS");
    wrapper->addChildElement(apvtsXml.release());
  }

  // Save Graph State
  auto *graphXml = xmlRoot.createNewChildElement("Graph");
  {
    const juce::ScopedLock sl(graphLock);
    graphEngine.saveState(graphXml);
  }

  copyXmlToBinary(xmlRoot, destData);
}

void EuclideanArpProcessor::setStateInformation(const void *data,
                                                int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));

  if (xmlState != nullptr && xmlState->hasTagName("EuclideanArpState")) {
    // Restore APVTS Macros
    auto *apvtsWrapper = xmlState->getChildByName("APVTS");
    if (apvtsWrapper != nullptr && apvtsWrapper->getNumChildElements() > 0) {
      auto *apvtsXml = apvtsWrapper->getChildElement(0);
      if (apvtsXml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*apvtsXml));
      }
    }

    // Restore Graph State
    auto *graphXml = xmlState->getChildByName("Graph");
    if (graphXml != nullptr) {
      const juce::ScopedLock sl(graphLock);
      graphEngine.loadState(graphXml, midiHandler, clockManager, macros);
      updateMacroNames();
    }
  }
}

EuclideanArpEditor *EuclideanArpProcessor::getEditor() {
  return dynamic_cast<EuclideanArpEditor *>(getActiveEditor());
}

void EuclideanArpProcessor::addNode(std::shared_ptr<GraphNode> node) {
  const juce::ScopedLock sl(graphLock);
  graphEngine.addNode(node);
  updateMacroNames();
}

void EuclideanArpProcessor::removeNode(GraphNode *node) {
  const juce::ScopedLock sl(graphLock);
  graphEngine.removeNode(node);
  updateMacroNames();
}

void EuclideanArpProcessor::updateMacroNames() {
  // Count how many parameters map to each macro index (0-31)
  std::array<int, 32> mappingCount = {};
  std::array<juce::String, 32> mappingNames;

  auto &nodes = graphEngine.getNodes();
  for (auto &node : nodes) {
    auto mappings = node->getMacroMappings();
    for (auto &[paramName, macroIndexPtr] : mappings) {
      int idx = *macroIndexPtr;
      if (idx >= 0 && idx < 32) {
        mappingCount[(size_t)idx]++;
        if (mappingCount[(size_t)idx] == 1) {
          mappingNames[(size_t)idx] = paramName;
        }
      }
    }
  }

  // Update each MacroParameter's display name
  for (int i = 0; i < 32; ++i) {
    if (macroParams[i] == nullptr)
      continue;

    if (mappingCount[(size_t)i] == 0) {
      macroParams[i]->clearMapping();
    } else if (mappingCount[(size_t)i] == 1) {
      macroParams[i]->setMappingName(mappingNames[(size_t)i]);
    } else {
      macroParams[i]->setMappingName("MULTIPLE");
    }
  }
}

// This creates new instances of the plugin
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new EuclideanArpProcessor();
}
