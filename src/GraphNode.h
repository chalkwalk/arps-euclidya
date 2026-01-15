#pragma once

#include "DataModel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

class GraphNode {
public:
  virtual ~GraphNode() = default;

  virtual std::string getName() const = 0;

  // Triggered when an upstream connection sets the dirty flag
  virtual void process() = 0;

  virtual std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
    juce::ignoreUnused(apvts);
    return nullptr;
  }

  void setInputSequence(int inputPort, const NoteSequence &sequence);
  const NoteSequence &getOutputSequence(int outputPort) const;

  struct Connection {
    GraphNode *targetNode;
    int targetInputPort;
  };

  void addConnection(int thisOutputPort, GraphNode *targetNode,
                     int targetInputPort);
  const std::map<int, std::vector<Connection>> &getConnections() const {
    return connections;
  }
  void clearConnections();

protected:
  std::map<int, NoteSequence> inputSequences;
  std::map<int, NoteSequence> outputSequences;

  std::map<int, std::vector<Connection>> connections;
};
