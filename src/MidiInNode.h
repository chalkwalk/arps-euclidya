#pragma once

#include "GraphNode.h"
#include "MidiHandler.h"

class MidiInNode : public GraphNode {
public:
  MidiInNode(MidiHandler &handler);
  ~MidiInNode() override = default;

  std::string getName() const override { return "Midi In"; }

  // Reads from MidiHandler and generates sequence cache
  void process() override;

private:
  MidiHandler &midiHandler;
};
