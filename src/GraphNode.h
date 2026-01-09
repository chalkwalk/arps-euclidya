#pragma once

#include "DataModel.h"
#include <juce_core/juce_core.h>
#include <map>
#include <vector>

class GraphNode {
public:
  virtual ~GraphNode() = default;

  virtual std::string getName() const = 0;

  // Triggered when an upstream connection sets the dirty flag
  virtual void process() = 0;

  void setInputSequence(int inputPort, const NoteSequence &sequence);
  const NoteSequence &getOutputSequence(int outputPort) const;

  void addConnection(int thisOutputPort, GraphNode *targetNode,
                     int targetInputPort);

protected:
  std::map<int, NoteSequence> inputSequences;
  std::map<int, NoteSequence> outputSequences;

  struct Connection {
    GraphNode *targetNode;
    int targetInputPort;
  };
  std::map<int, std::vector<Connection>> connections;
};
