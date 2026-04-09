#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>

class MacroParameter;

struct MidiLogEvent {
  // Output logging: 0=NoteOn, 1=NoteOff, 2=CC, 3=TICK, 4=ArpNoteOn,
  // 5=ArpNoteOff Input logging: 10=InNoteOn, 11=InNoteOff, 12=InCC,
  // 13=InChanPress, 14=InPitchBend, 15=InPolyAftertouch
  // CLAP logging: 20=ClapNoteOn, 21=ClapNoteOff, 22=ClapExpr
  int logType;
  int channel;
  int data1;
  float data2;
};

// Forward declaration of the editor class
class ArpsEuclidyaEditor;

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#endif
#include <clap-juce-extensions/clap-juce-extensions.h>
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include "ClockManager.h"
#include "GraphEngine.h"
#include "NoteExpressionManager.h"

struct OutboundClapEvent {
  enum class Type { NoteOn, NoteOff, NoteExpression };
  Type type;
  int channel;
  int noteNumber;
  float value;  // velocity or expression value
  int sampleOffset;
  int32_t noteID;
  NoteExpressionType expressionType;
};

/**
 */
class ArpsEuclidyaProcessor
    : public juce::AudioProcessor,
      public juce::AudioProcessorValueTreeState::Listener,
      public clap_juce_extensions::clap_juce_audio_processor_capabilities {
 public:
  //==============================================================================
  ArpsEuclidyaProcessor();
  ~ArpsEuclidyaProcessor() override;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;

  bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor *createEditor() override;
  bool hasEditor() const override;

  const juce::String getName() const override;
  bool acceptsMidi() const override;
  bool producesMidi() const override;
  bool isMidiEffect() const override;
  bool supportsMPE() const override;
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  void parameterChanged(const juce::String &parameterID,
                        float newValue) override;

  class ArpsEuclidyaEditor *getEditor();

  NoteExpressionManager &getNoteExpressionManager() {
    return noteExpressionManager;
  }

  // CLAP Extensions
  bool supportsNoteExpressions() override;
  bool supportsDirectEvent(uint16_t space_id, uint16_t type) override;
  bool supportsNoteDialectClap(bool isInput) override;
  bool prefersNoteDialectClap(bool isInput) override;
  void handleDirectEvent(const clap_event_header_t *event,
                         int sampleOffset) override;
  bool supportsOutboundEvents() override { return true; }
  void addOutboundEventsToQueue(const clap_output_events *out_events,
                                const juce::MidiBuffer &midiBuffer,
                                int sampleOffset) override;

  void updateMacroNames();
  void toggleMacroBipolar(int index);
  bool getMacroBipolar(int index) const;

  juce::AbstractFifo midiLogFifo{512};
  std::array<MidiLogEvent, 512> midiLogBuffer;
  void logMidiEvent(int type, int channel, int d1, float d2);

  // Subsystems and state accessed by UI
  NoteExpressionManager noteExpressionManager;
  ClockManager clockManager;
  GraphEngine graphEngine;
  juce::CriticalSection graphLock;

  std::atomic<bool> macrosDirty{true};

  juce::AudioProcessorValueTreeState apvts;
  juce::MidiKeyboardState keyboardState;
  std::array<std::atomic<float> *, 32> macros = {nullptr};
  std::array<MacroParameter *, 32> macroParams = {nullptr};

  struct PatchMetadata {
    juce::String name;
    juce::String author;
    juce::String description;
    juce::String tags;
    juce::String created;
    juce::String modified;
  };

  PatchMetadata currentPatchMetadata;
  bool hasLoggedParams = false;
  std::atomic<bool> graphNeedsRecalculate{false};

  // Graph Editor methods
  static constexpr int CURRENT_PATCH_VERSION = 2;

  bool savePatch(const juce::File &file);
  bool loadPatch(const juce::File &file);

  void addNode(const std::shared_ptr<GraphNode> &node);
  void removeNode(GraphNode *node);

  void performGraphMutation(std::function<void()> mutation);
  std::unique_ptr<juce::XmlElement> captureState();

  juce::UndoManager undoManager;

  std::atomic<bool> isClapProtocol{false};
  std::atomic<int32_t> globalNoteIDCounter{0};
  std::vector<OutboundClapEvent> outboundClapEvents;

  void loadFromXml(juce::XmlElement *xmlState);

 private:
  void handleNoteOnEvent(const clap_event_note_t *event);
  void handleNoteOffEvent(const clap_event_note_t *event);
  void handleNoteExpressionEvent(const clap_event_note_expression_t *event);

  static void upgradePatch(juce::XmlElement *xml, int fromVersion);
  static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArpsEuclidyaProcessor)
};
