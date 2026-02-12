#pragma once

#include <array>
#include <juce_audio_processors/juce_audio_processors.h>

class MacroParameter;

struct MidiLogEvent {
  int logType; // 0=NoteOn, 1=NoteOff, 2=CC, 3=TICK, 4=ArpNoteOn, 5=ArpNoteOff
  int channel;
  int data1;
  float data2;
};

// Forward declaration of the editor class
class EuclideanArpEditor;

#include "ClockManager.h"
#include "GraphEngine.h"
#include "MidiHandler.h"

class EuclideanArpProcessor : public juce::AudioProcessor {
public:
  EuclideanArpProcessor();
  ~EuclideanArpProcessor() override;

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
  double getTailLengthSeconds() const override;

  int getNumPrograms() override;
  int getCurrentProgram() override;
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int index, const juce::String &newName) override;

  void getStateInformation(juce::MemoryBlock &destData) override;
  void setStateInformation(const void *data, int sizeInBytes) override;

  class EuclideanArpEditor *getEditor();

  juce::AudioProcessorValueTreeState apvts;
  std::array<std::atomic<float> *, 32> macros = {nullptr};
  std::array<MacroParameter *, 32> macroParams = {nullptr};

  // Called after graph changes to update macro display names
  void updateMacroNames();

  juce::AbstractFifo midiLogFifo{512};
  std::array<MidiLogEvent, 512> midiLogBuffer;
  void logMidiEvent(int type, int channel, int d1, float d2);

  // Subsystems
  MidiHandler midiHandler;
  ClockManager clockManager;
  GraphEngine graphEngine;
  juce::CriticalSection graphLock;

  // Graph Editor methods
  void addNode(std::shared_ptr<GraphNode> node);
  void removeNode(GraphNode *node);

  // Hardcoded Step 2 Nodes
  // Hardcoded Step 2 Nodes

private:
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EuclideanArpProcessor)
};
