#include "PluginProcessor.h"

#include <memory>

#include "AppSettings.h"
#include "MacroParameter.h"
#include "MidiOutNode/MidiOutNode.h"
#include "PluginEditor.h"

namespace FactoryPatches {
extern const char *getNamedResource(const char *name, int &size);
}

ArpsEuclidyaProcessor::ArpsEuclidyaProcessor()
    : AudioProcessor(BusesProperties()),  // No audio channels for MIDI effect
      apvts(*this, nullptr, "Parameters", createParameterLayout()) {
  // Fetch raw parameter pointers and MacroParameter* for RT thread
  for (int i = 0; i < 32; ++i) {
    macros[(size_t)i] =
        apvts.getRawParameterValue("macro_" + juce::String(i + 1));
    macroParams[(size_t)i] = dynamic_cast<MacroParameter *>(
        apvts.getParameter("macro_" + juce::String(i + 1)));
    apvts.addParameterListener("macro_" + juce::String(i + 1), this);
  }

  // Give the engine access to the macro parameter array for all nodes
  graphEngine.setMacros(&macros);

  // Create graph engine recalculation callback for internal UI sliders
  graphEngine.onGraphDirtied = [this]() { graphNeedsRecalculate = true; };

  // Wire topology changes to force the NoteExpressionManager to become dirty,
  // ensuring current held notes are pushed through new connections instantly
  graphEngine.onTopologyChanged = [this]() {
    noteExpressionManager.forceDirty();
  };

  // Load default Init patch from binary resources
  int size = 0;
  const char *data = FactoryPatches::getNamedResource("Init_euclidya", size);
  if (data != nullptr && size > 0) {
    juce::String xmlString(data, (size_t)size);
    std::unique_ptr<juce::XmlElement> xmlState =
        juce::XmlDocument::parse(xmlString);
    if (xmlState != nullptr && xmlState->hasTagName("ArpsEuclidyaState")) {
      loadFromXml(xmlState.get());
    }
  }

  noteExpressionManager.setIgnoreMpeMasterPressure(
      AppSettings::getInstance().getIgnoreMpeMasterPressure());
  noteExpressionManager.setLegacyMode(
      !AppSettings::getInstance().getMpeEnabled());
}

ArpsEuclidyaProcessor::~ArpsEuclidyaProcessor() {
  for (int i = 0; i < 32; ++i) {
    apvts.removeParameterListener("macro_" + juce::String(i + 1), this);
  }
}

void ArpsEuclidyaProcessor::logMidiEvent(int type, int channel, int d1,
                                         float d2) {
  if (midiLogFifo.getFreeSpace() > 0) {
    int start1;
    int size1;
    int start2;
    int size2;
    midiLogFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0) {
      midiLogBuffer[(size_t)start1] = {type, channel, d1, d2};
    }
    midiLogFifo.finishedWrite(size1 + size2);
  }
}

juce::AudioProcessorValueTreeState::ParameterLayout
ArpsEuclidyaProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  for (int i = 1; i <= 32; ++i) {
    layout.add(std::make_unique<MacroParameter>(i));
  }

  return layout;
}

const juce::String ArpsEuclidyaProcessor::getName() const {
  return "Arps Euclidya";
}

bool ArpsEuclidyaProcessor::acceptsMidi() const { return true; }

bool ArpsEuclidyaProcessor::producesMidi() const { return true; }

bool ArpsEuclidyaProcessor::isMidiEffect() const { return true; }

bool ArpsEuclidyaProcessor::supportsMPE() const { return true; }

double ArpsEuclidyaProcessor::getTailLengthSeconds() const { return 0.0; }

int ArpsEuclidyaProcessor::getNumPrograms() { return 1; }

int ArpsEuclidyaProcessor::getCurrentProgram() { return 0; }

void ArpsEuclidyaProcessor::setCurrentProgram(int index) {
  juce::ignoreUnused(index);
}

const juce::String ArpsEuclidyaProcessor::getProgramName(int index) {
  juce::ignoreUnused(index);
  return {};
}

void ArpsEuclidyaProcessor::changeProgramName(int index,
                                              const juce::String &newName) {
  juce::ignoreUnused(index, newName);
}

//==============================================================================
void ArpsEuclidyaProcessor::prepareToPlay(double sampleRate,
                                          int samplesPerBlock) {
  juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void ArpsEuclidyaProcessor::releaseResources() {}

bool ArpsEuclidyaProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const {
  // MIDI effect without audio in/out
  return !(
      layouts.getMainInputChannelSet() != juce::AudioChannelSet::disabled() ||
      layouts.getMainOutputChannelSet() != juce::AudioChannelSet::disabled());
}

void ArpsEuclidyaProcessor::handleNoteOnEvent(const clap_event_note_t *event) {
  keyboardState.noteOn(event->channel + 1, event->key, (float)event->velocity);
  noteExpressionManager.handleNoteOn(event->channel, event->key,
                                     (float)event->velocity, event->note_id);
}

void ArpsEuclidyaProcessor::handleNoteOffEvent(const clap_event_note_t *event) {
  keyboardState.noteOff(event->channel + 1, event->key, (float)event->velocity);
  noteExpressionManager.handleNoteOff(event->channel, event->key,
                                      (float)event->velocity, event->note_id);
}

void ArpsEuclidyaProcessor::handleNoteExpressionEvent(
    const clap_event_note_expression_t *event) {
  NoteExpressionType type;
  switch (event->expression_id) {
    case CLAP_NOTE_EXPRESSION_VOLUME:
      type = NoteExpressionType::Volume;
      break;
    case CLAP_NOTE_EXPRESSION_TUNING:
      type = NoteExpressionType::Tuning;
      break;
    case CLAP_NOTE_EXPRESSION_BRIGHTNESS:
      type = NoteExpressionType::Brightness;
      break;
    case CLAP_NOTE_EXPRESSION_PRESSURE:
      type = NoteExpressionType::Pressure;
      break;
    default:
      return;
  }
  noteExpressionManager.handleNoteExpression(
      event->channel, event->key, type, (float)event->value, event->note_id);
}

void ArpsEuclidyaProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                         juce::MidiBuffer &midiMessages) {
  // Ensure no audio passes through; this is a MIDI effect.
  // This also ensures we don't pick up noise or feedback from the host.
  buffer.clear();

  // If we are running in CLAP mode, we prefer direct events for NoteOn/Off.
  // We filter out any redundant MIDI NoteOn/Off messages here...
  if (isClapProtocol) {
    juce::MidiBuffer filteredMessages;
    for (const auto metadata : midiMessages) {
      const auto message = metadata.getMessage();
      if (!message.isNoteOn() && !message.isNoteOff()) {
        filteredMessages.addEvent(message, metadata.samplePosition);
      }
    }
    midiMessages.swapWith(filteredMessages);
  }

  const int numSamples = buffer.getNumSamples();

  // Step 2: Update Clock and process incoming MPE/Note states
  clockManager.update(getPlayHead(), buffer.getNumSamples(), getSampleRate());

  if (!hasLoggedParams) {
    logMidiEvent(6, 0, buffer.getNumSamples(), (float)getSampleRate());
    hasLoggedParams = true;
  }

  // Inject UI-generated synthetic notes and extract DAW MIDI to update the
  // on-screen keyboard.
  // CRITICAL: We set injectToBuffer to false in CLAP mode because we handle
  // the notes manually and don't want redundant MIDI events on Channel 1.
  keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples,
                                      !isClapProtocol);

  // Log incoming MIDI
  for (const auto metadata : midiMessages) {
    const auto message = metadata.getMessage();
    const int channel = message.getChannel();
    if (message.isNoteOn()) {
      logMidiEvent(10, channel, message.getNoteNumber(), message.getVelocity());
    } else if (message.isNoteOff()) {
      logMidiEvent(11, channel, message.getNoteNumber(), 0.0f);
    } else if (message.isController()) {
      logMidiEvent(12, channel, message.getControllerNumber(),
                   (float)message.getControllerValue());
    } else if (message.isChannelPressure()) {
      logMidiEvent(13, channel, message.getChannelPressureValue(), 0.0f);
    } else if (message.isPitchWheel()) {
      logMidiEvent(14, channel, message.getPitchWheelValue(), 0.0f);
    } else if (message.isAftertouch()) {
      logMidiEvent(15, channel, message.getNoteNumber(),
                   (float)message.getAfterTouchValue());
    }
  }

  if (!isClapProtocol) {
    noteExpressionManager.processMidi(midiMessages);
  }

  {
    const juce::ScopedLock sl(graphLock);

    // Instantaneous graph recalculation
    if (noteExpressionManager.hasChanged()) {
      graphNeedsRecalculate = true;
    }

    if (graphNeedsRecalculate.exchange(false)) {
      // Mark source nodes dirty — they have no upstream connections
      // and need to re-evaluate when external state changes (MIDI,
      // macro sliders, etc.).
      for (const auto &node : graphEngine.getNodes()) {
        if (node->getNumInputPorts() == 0) {
          node->isDirty = true;
        }
      }
      graphEngine.recalculate();
    }

    // Clear input, we are replacing the midi stream with our engine's output
    midiMessages.clear();

    // The output node evaluates the tick and sequence cache to build the new
    // buffer. Find all MidiOutNodes in the graph and let them emit.
    class MidiBufferCollector : public NoteEventCollector {
     public:
      MidiBufferCollector(juce::MidiBuffer &b) : buffer(b) {}
      void addNoteOn(int channel, int noteNumber, float velocity,
                     int sampleOffset, int32_t /*noteID*/) override {
        buffer.addEvent(
            juce::MidiMessage::noteOn(channel + 1, noteNumber, velocity),
            sampleOffset);
      }
      void addNoteOff(int channel, int noteNumber, float velocity,
                      int sampleOffset, int32_t /*noteID*/) override {
        buffer.addEvent(
            juce::MidiMessage::noteOff(channel + 1, noteNumber, velocity),
            sampleOffset);
      }
      void addNoteExpression(int channel, int noteNumber,
                             NoteExpressionType type, float value,
                             int sampleOffset, int32_t /*noteID*/) override {
        switch (type) {
          case NoteExpressionType::Pressure:
            buffer.addEvent(juce::MidiMessage::aftertouchChange(
                                channel + 1, noteNumber, (int)(value * 127.0f)),
                            sampleOffset);
            break;
          case NoteExpressionType::Brightness:
            buffer.addEvent(juce::MidiMessage::controllerEvent(
                                channel + 1, 74, (int)(value * 127.0f)),
                            sampleOffset);
            break;
          default:
            break;
        }
      }

     private:
      juce::MidiBuffer &buffer;
    };

    class ClapEventCollector : public NoteEventCollector {
     public:
      ClapEventCollector(std::vector<OutboundClapEvent> &v) : events(v) {}
      void addNoteOn(int channel, int noteNumber, float velocity,
                     int sampleOffset, int32_t noteID) override {
        events.push_back({OutboundClapEvent::Type::NoteOn, channel, noteNumber,
                          velocity, sampleOffset, noteID,
                          NoteExpressionType::Volume});
      }
      void addNoteOff(int channel, int noteNumber, float velocity,
                      int sampleOffset, int32_t noteID) override {
        events.push_back({OutboundClapEvent::Type::NoteOff, channel, noteNumber,
                          velocity, sampleOffset, noteID,
                          NoteExpressionType::Volume});
      }
      void addNoteExpression(int channel, int noteNumber,
                             NoteExpressionType type, float value,
                             int sampleOffset, int32_t noteID) override {
        events.push_back({OutboundClapEvent::Type::NoteExpression, channel,
                          noteNumber, value, sampleOffset, noteID, type});
      }

     private:
      std::vector<OutboundClapEvent> &events;
    };

    outboundClapEvents.clear();
    std::unique_ptr<NoteEventCollector> collector;
    if (isClapProtocol) {
      collector = std::make_unique<ClapEventCollector>(outboundClapEvents);
    } else {
      collector = std::make_unique<MidiBufferCollector>(midiMessages);
    }

    for (const auto &outNode : graphEngine.getMidiOutNodes()) {
      outNode->generateOutput(*collector, numSamples, globalNoteIDCounter);
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

bool ArpsEuclidyaProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor *ArpsEuclidyaProcessor::createEditor() {
  return new ArpsEuclidyaEditor(*this);
}

//==============================================================================
void ArpsEuclidyaProcessor::getStateInformation(juce::MemoryBlock &destData) {
  juce::XmlElement xmlRoot("ArpsEuclidyaState");

  if (juce::PluginHostType::getPluginLoadedAs() ==
      juce::AudioProcessor::wrapperType_Standalone) {
    xmlRoot.setAttribute("standaloneBPM", clockManager.getBPM());
  }

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

  // Save Metadata
  auto *metaXml = xmlRoot.createNewChildElement("Metadata");
  if (currentPatchMetadata.author.isEmpty()) {
    currentPatchMetadata.author = AppSettings::getInstance().getDefaultAuthor();
  }
  if (currentPatchMetadata.created.isEmpty()) {
    currentPatchMetadata.created = juce::Time::getCurrentTime().toISO8601(true);
  }
  currentPatchMetadata.modified = juce::Time::getCurrentTime().toISO8601(true);

  metaXml->setAttribute("name", currentPatchMetadata.name);
  metaXml->setAttribute("author", currentPatchMetadata.author);
  metaXml->setAttribute("description", currentPatchMetadata.description);
  metaXml->setAttribute("tags", currentPatchMetadata.tags);
  metaXml->setAttribute("created", currentPatchMetadata.created);
  metaXml->setAttribute("modified", currentPatchMetadata.modified);

  copyXmlToBinary(xmlRoot, destData);
}

void ArpsEuclidyaProcessor::setStateInformation(const void *data,
                                                int sizeInBytes) {
  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));

  if (xmlState != nullptr && xmlState->hasTagName("ArpsEuclidyaState")) {
    loadFromXml(xmlState.get());
  }
}

bool ArpsEuclidyaProcessor::savePatch(const juce::File &file) {
  juce::XmlElement xmlRoot("ArpsEuclidyaState");
  xmlRoot.setAttribute("version", CURRENT_PATCH_VERSION);

  if (juce::PluginHostType::getPluginLoadedAs() ==
      juce::AudioProcessor::wrapperType_Standalone) {
    xmlRoot.setAttribute("standaloneBPM", clockManager.getBPM());
  }

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

  // Save Metadata
  auto *metaXml = xmlRoot.createNewChildElement("Metadata");
  if (currentPatchMetadata.author.isEmpty()) {
    currentPatchMetadata.author = AppSettings::getInstance().getDefaultAuthor();
  }
  if (currentPatchMetadata.created.isEmpty()) {
    currentPatchMetadata.created = juce::Time::getCurrentTime().toISO8601(true);
  }
  if (currentPatchMetadata.name.isEmpty()) {
    currentPatchMetadata.name = file.getFileNameWithoutExtension();
  }
  currentPatchMetadata.modified = juce::Time::getCurrentTime().toISO8601(true);

  metaXml->setAttribute("name", currentPatchMetadata.name);
  metaXml->setAttribute("author", currentPatchMetadata.author);
  metaXml->setAttribute("description", currentPatchMetadata.description);
  metaXml->setAttribute("tags", currentPatchMetadata.tags);
  metaXml->setAttribute("created", currentPatchMetadata.created);
  metaXml->setAttribute("modified", currentPatchMetadata.modified);

  return xmlRoot.writeTo(file);
}

bool ArpsEuclidyaProcessor::loadPatch(const juce::File &file) {
  auto xmlState = juce::XmlDocument::parse(file);

  if (xmlState != nullptr && xmlState->hasTagName("ArpsEuclidyaState")) {
    loadFromXml(xmlState.get());
    return true;
  }
  return false;
}

void ArpsEuclidyaProcessor::loadFromXml(juce::XmlElement *xmlState) {
  int version = xmlState->getIntAttribute("version", 0);

  if (version > CURRENT_PATCH_VERSION) {
    return;
  }

  if (version < CURRENT_PATCH_VERSION) {
    upgradePatch(xmlState, version);
  }

  if (xmlState->hasAttribute("standaloneBPM") &&
      juce::PluginHostType::getPluginLoadedAs() ==
          juce::AudioProcessor::wrapperType_Standalone) {
    clockManager.setBPM(xmlState->getDoubleAttribute("standaloneBPM"));
  }

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
    graphEngine.loadState(graphXml, noteExpressionManager, clockManager,
                          macros);
    for (const auto &node : graphEngine.getNodes()) {
      node->onMappingChanged = [this]() { updateMacroNames(); };
    }
    updateMacroNames();

    if (auto *editor = getEditor()) {
      editor->rebuildCanvas();
    }
  }

  // Restore Metadata
  auto *metaXml = xmlState->getChildByName("Metadata");
  if (metaXml != nullptr) {
    currentPatchMetadata.name = metaXml->getStringAttribute("name");
    currentPatchMetadata.author = metaXml->getStringAttribute("author");
    currentPatchMetadata.description =
        metaXml->getStringAttribute("description");
    currentPatchMetadata.tags = metaXml->getStringAttribute("tags");
    currentPatchMetadata.created = metaXml->getStringAttribute("created");
    currentPatchMetadata.modified = metaXml->getStringAttribute("modified");
  } else {
    currentPatchMetadata = PatchMetadata();
  }
}

void ArpsEuclidyaProcessor::upgradePatch(juce::XmlElement *xml,
                                         int fromVersion) {
  // Incremental upgrades
  for (int v = fromVersion; v < CURRENT_PATCH_VERSION; ++v) {
    if (v == 1) {
      // Example migration for v1 to v2
      // v1 patches have no metadata. We just let loadFromXml fill an empty
      // PatchMetadata. But we update the version attribute.
    }
  }
  xml->setAttribute("version", CURRENT_PATCH_VERSION);
}

ArpsEuclidyaEditor *ArpsEuclidyaProcessor::getEditor() {
  return dynamic_cast<ArpsEuclidyaEditor *>(getActiveEditor());
}

class GraphStateUndoAction : public juce::UndoableAction {
 public:
  GraphStateUndoAction(ArpsEuclidyaProcessor &p,
                       std::unique_ptr<juce::XmlElement> before,
                       std::unique_ptr<juce::XmlElement> after)
      : processor(p),
        stateBefore(std::move(before)),
        stateAfter(std::move(after)) {}

  bool perform() override {
    if (isFirstPerform) {
      isFirstPerform = false;
      return true;
    }
    if (stateAfter != nullptr) {
      processor.loadFromXml(stateAfter.get());
      return true;
    }
    return false;
  }

  bool undo() override {
    if (stateBefore != nullptr) {
      processor.loadFromXml(stateBefore.get());
      return true;
    }
    return false;
  }

  int getSizeInUnits() override { return 10; }

 private:
  ArpsEuclidyaProcessor &processor;
  std::unique_ptr<juce::XmlElement> stateBefore;
  std::unique_ptr<juce::XmlElement> stateAfter;
  bool isFirstPerform = true;
};

void ArpsEuclidyaProcessor::performGraphMutation(
    std::function<void()> mutation) {
  auto before = captureState();
  mutation();
  auto after = captureState();
  undoManager.perform(
      new GraphStateUndoAction(*this, std::move(before), std::move(after)));
}

std::unique_ptr<juce::XmlElement> ArpsEuclidyaProcessor::captureState() {
  auto xmlRoot = std::make_unique<juce::XmlElement>("ArpsEuclidyaState");
  xmlRoot->setAttribute("version", CURRENT_PATCH_VERSION);

  if (juce::PluginHostType::getPluginLoadedAs() ==
      juce::AudioProcessor::wrapperType_Standalone) {
    xmlRoot->setAttribute("standaloneBPM", clockManager.getBPM());
  }

  // Save APVTS Macros
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> apvtsXml(state.createXml());
  if (apvtsXml != nullptr) {
    auto *wrapper = xmlRoot->createNewChildElement("APVTS");
    wrapper->addChildElement(apvtsXml.release());
  }

  // Save Graph State
  auto *graphXml = xmlRoot->createNewChildElement("Graph");
  {
    const juce::ScopedLock sl(graphLock);
    graphEngine.saveState(graphXml);
  }

  // Save Metadata
  auto *metaXml = xmlRoot->createNewChildElement("Metadata");
  metaXml->setAttribute("name", currentPatchMetadata.name);
  metaXml->setAttribute("author", currentPatchMetadata.author);
  metaXml->setAttribute("description", currentPatchMetadata.description);
  metaXml->setAttribute("tags", currentPatchMetadata.tags);
  metaXml->setAttribute("created", currentPatchMetadata.created);
  metaXml->setAttribute("modified", currentPatchMetadata.modified);

  return xmlRoot;
}

void ArpsEuclidyaProcessor::addNode(const std::shared_ptr<GraphNode> &node) {
  performGraphMutation([this, node]() {
    const juce::ScopedLock sl(graphLock);
    node->onMappingChanged = [this]() { updateMacroNames(); };
    graphEngine.addNode(node);
    updateMacroNames();
  });
}

void ArpsEuclidyaProcessor::removeNode(GraphNode *node) {
  performGraphMutation([this, node]() {
    const juce::ScopedLock sl(graphLock);
    graphEngine.removeNode(node);
    updateMacroNames();
  });
}

void ArpsEuclidyaProcessor::updateMacroNames() {
  // Count how many parameters map to each macro index (0-31)
  std::array<int, 32> mappingCount = {};
  std::array<juce::String, 32> mappingNames;

  const auto &nodes = graphEngine.getNodes();
  for (const auto &node : nodes) {
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

  bool namesChanged = false;

  // Update each MacroParameter's display name
  for (size_t i = 0; i < 32; ++i) {
    if (macroParams[i] == nullptr) {
      continue;
    }

    juce::String oldName = macroParams[i]->getName(1024);

    if (mappingCount[i] == 0) {
      macroParams[i]->clearMapping();
    } else if (mappingCount[i] == 1) {
      macroParams[i]->setMappingName(mappingNames[i]);
    } else {
      macroParams[i]->setMappingName("MULTIPLE");
    }

    // Check if the resulting name is different
    if (macroParams[i]->getName(1024) != oldName) {
      namesChanged = true;
    }
  }
  macrosDirty = true;

  if (namesChanged) {
    // Notify the DAW host to rescan parameter metadata (CLAP/VST3 support)
    auto details =
        juce::AudioProcessorListener::ChangeDetails().withParameterInfoChanged(
            true);
    updateHostDisplay(details);
  }
}

void ArpsEuclidyaProcessor::parameterChanged(const juce::String &parameterID,
                                             float newValue) {
  juce::ignoreUnused(parameterID, newValue);
  graphNeedsRecalculate = true;
}

bool ArpsEuclidyaProcessor::supportsNoteExpressions() { return true; }

bool ArpsEuclidyaProcessor::supportsNoteDialectClap(bool isInput) {
  juce::ignoreUnused(isInput);
  return true;
}

bool ArpsEuclidyaProcessor::prefersNoteDialectClap(bool isInput) {
  juce::ignoreUnused(isInput);
  return true;
}

bool ArpsEuclidyaProcessor::supportsDirectEvent(uint16_t space_id,
                                                uint16_t type) {
  if (space_id == CLAP_CORE_EVENT_SPACE_ID) {
    switch (type) {
      case CLAP_EVENT_NOTE_ON:
      case CLAP_EVENT_NOTE_OFF:
      case CLAP_EVENT_NOTE_EXPRESSION:
      case CLAP_EVENT_MIDI:
      case CLAP_EVENT_MIDI_SYSEX:
        return true;
      default:
        break;
    }
  }
  return false;
}

void ArpsEuclidyaProcessor::handleDirectEvent(const clap_event_header_t *event,
                                              int sampleOffset) {
  isClapProtocol = true;
  juce::ignoreUnused(sampleOffset);
  if (event->space_id == CLAP_CORE_EVENT_SPACE_ID) {
    switch (event->type) {
      case CLAP_EVENT_NOTE_ON: {
        handleNoteOnEvent(reinterpret_cast<const clap_event_note_t *>(event));
        break;
      }
      case CLAP_EVENT_NOTE_OFF: {
        handleNoteOffEvent(reinterpret_cast<const clap_event_note_t *>(event));
        break;
      }
      case CLAP_EVENT_NOTE_EXPRESSION: {
        handleNoteExpressionEvent(
            reinterpret_cast<const clap_event_note_expression_t *>(event));
        break;
      }
      case CLAP_EVENT_PARAM_VALUE: {
        const auto *v =
            reinterpret_cast<const clap_event_param_value_t *>(event);
        const auto &params = getParameters();
        const int index = static_cast<int>(v->param_id);
        if (index >= 0 && index < params.size()) {
          params[index]->setValueNotifyingHost(static_cast<float>(v->value));
        }
        break;
      }
      case CLAP_EVENT_MIDI: {
        const auto *midiEvent =
            reinterpret_cast<const clap_event_midi_t *>(event);
        juce::MidiMessage msg(midiEvent->data[0], midiEvent->data[1],
                              midiEvent->data[2]);
        noteExpressionManager.handleMidiMessage(msg);
        break;
      }
      case CLAP_EVENT_MIDI_SYSEX: {
        break;
      }
      default:
        break;
    }
  }
}

void ArpsEuclidyaProcessor::addOutboundEventsToQueue(
    const clap_output_events *out_events, const juce::MidiBuffer &midiBuffer,
    int sampleOffset) {
  if (!out_events || !out_events->try_push)
    return;

  // 1. Push native CLAP events collected during processBlock
  for (const auto &evt : outboundClapEvents) {
    if (evt.type == OutboundClapEvent::Type::NoteOn) {
      clap_event_note clapEvt{};
      clapEvt.header.size = sizeof(clap_event_note);
      clapEvt.header.type = CLAP_EVENT_NOTE_ON;
      clapEvt.header.time =
          static_cast<uint32_t>(evt.sampleOffset + sampleOffset);
      clapEvt.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      clapEvt.header.flags = 0;
      clapEvt.port_index = 0;
      clapEvt.channel = (int16_t)evt.channel;
      clapEvt.key = (int16_t)evt.noteNumber;
      clapEvt.velocity = (double)evt.value;
      clapEvt.note_id = evt.noteID;
      out_events->try_push(out_events, &clapEvt.header);
    } else if (evt.type == OutboundClapEvent::Type::NoteOff) {
      clap_event_note clapEvt{};
      clapEvt.header.size = sizeof(clap_event_note);
      clapEvt.header.type = CLAP_EVENT_NOTE_OFF;
      clapEvt.header.time =
          static_cast<uint32_t>(evt.sampleOffset + sampleOffset);
      clapEvt.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      clapEvt.header.flags = 0;
      clapEvt.port_index = 0;
      clapEvt.channel = (int16_t)evt.channel;
      clapEvt.key = (int16_t)evt.noteNumber;
      clapEvt.velocity = (double)evt.value;
      clapEvt.note_id = evt.noteID;
      out_events->try_push(out_events, &clapEvt.header);
    } else if (evt.type == OutboundClapEvent::Type::NoteExpression) {
      clap_event_note_expression clapEvt{};
      clapEvt.header.size = sizeof(clap_event_note_expression);
      clapEvt.header.type = CLAP_EVENT_NOTE_EXPRESSION;
      clapEvt.header.time =
          static_cast<uint32_t>(evt.sampleOffset + sampleOffset);
      clapEvt.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      clapEvt.header.flags = 0;
      clapEvt.port_index = 0;
      clapEvt.channel = (int16_t)evt.channel;
      clapEvt.key = (int16_t)evt.noteNumber;
      clapEvt.note_id = evt.noteID;
      clapEvt.value = (double)evt.value;

      switch (evt.expressionType) {
        case NoteExpressionType::Volume:
          clapEvt.expression_id = CLAP_NOTE_EXPRESSION_VOLUME;
          break;
        case NoteExpressionType::Pressure:
          clapEvt.expression_id = CLAP_NOTE_EXPRESSION_PRESSURE;
          break;
        case NoteExpressionType::Tuning:
          clapEvt.expression_id = CLAP_NOTE_EXPRESSION_TUNING;
          break;
        case NoteExpressionType::Brightness:
          clapEvt.expression_id = CLAP_NOTE_EXPRESSION_BRIGHTNESS;
          break;
      }
      out_events->try_push(out_events, &clapEvt.header);
    }
  }

  // 2. Interleave any remaining MIDI events from the buffer into the CLAP
  // output queue
  for (auto meta : midiBuffer) {
    auto msg = meta.getMessage();
    if (msg.getRawDataSize() <= 3) {
      clap_event_midi evt{};
      evt.header.size = sizeof(clap_event_midi);
      evt.header.type = CLAP_EVENT_MIDI;
      evt.header.time =
          static_cast<uint32_t>(meta.samplePosition + sampleOffset);
      evt.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      evt.header.flags = 0;
      evt.port_index = 0;
      std::memcpy(&evt.data, msg.getRawData(), (size_t)msg.getRawDataSize());
      out_events->try_push(out_events,
                           reinterpret_cast<const clap_event_header *>(&evt));
    }
  }
}

// This creates new instances of the plugin
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter() {
  return new ArpsEuclidyaProcessor();
}
