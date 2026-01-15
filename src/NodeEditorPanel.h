#pragma once

#include "GraphNode.h"
#include <juce_gui_basics/juce_gui_basics.h>

class NodeEditorPanel : public juce::Component,
                        public juce::ComboBox::Listener {
public:
  NodeEditorPanel(std::shared_ptr<GraphNode> node,
                  juce::AudioProcessorValueTreeState &apvts);
  ~NodeEditorPanel() override;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) override;

  std::shared_ptr<GraphNode> getNode() const { return targetNode; }

  std::function<void()> onDelete;
  std::function<void()> onMoveUp;
  std::function<void()> onMoveDown;

private:
  std::shared_ptr<GraphNode> targetNode;

  juce::Label titleLabel;
  juce::TextButton deleteButton{"X"};
  juce::TextButton moveUpButton{"^"};
  juce::TextButton moveDownButton{"v"};

  std::unique_ptr<juce::Component> customControls;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeEditorPanel)
};
