#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "GraphNode.h"

class NodeEditorPanel : public juce::Component {
 public:
  NodeEditorPanel(std::shared_ptr<GraphNode> node,
                  juce::AudioProcessorValueTreeState &apvts);
  ~NodeEditorPanel() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

  std::shared_ptr<GraphNode> getNode() const { return targetNode; }

  std::function<void()> onDelete;

 private:
  std::shared_ptr<GraphNode> targetNode;

  juce::Label titleLabel;
  juce::TextButton deleteButton{"X"};

  std::unique_ptr<juce::Component> customControls;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeEditorPanel)
};
