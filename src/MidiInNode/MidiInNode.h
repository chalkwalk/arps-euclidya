#pragma once

#include "../GraphNode.h"
#include "../NoteExpressionManager.h"

class MidiInNode : public GraphNode {
 public:
  explicit MidiInNode(NoteExpressionManager &handler);
  ~MidiInNode() override = default;

  std::string getName() const override { return "Midi In"; }
  int getNumInputPorts() const override { return 0; }

  void process() override;
  NodeLayout getLayout() const override;

  int channelFilter = 0;  // 0 means all channels
  MacroParam macroChannelFilter{"Channel Filter", {}};

  NoteExpressionManager &getNoteExpressionManager() {
    return noteExpressionManager;
  }

  void saveNodeState(juce::XmlElement *xml) override;
  void loadNodeState(juce::XmlElement *xml) override;

  std::vector<MacroParam *> getMacroParams() override {
    return {&macroChannelFilter};
  }

 private:
  NoteExpressionManager &noteExpressionManager;
};
