#include "NodeEditorPanel.h"

NodeEditorPanel::NodeEditorPanel(std::shared_ptr<GraphNode> node,
                                 juce::AudioProcessorValueTreeState &apvts)
    : targetNode(node) {

  titleLabel.setText(node->getName(), juce::dontSendNotification);
  titleLabel.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(titleLabel);

  addAndMakeVisible(deleteButton);
  addAndMakeVisible(moveUpButton);
  addAndMakeVisible(moveDownButton);

  customControls = node->createEditorComponent(apvts);
  if (customControls != nullptr) {
    addAndMakeVisible(customControls.get());
  }

  deleteButton.onClick = [this] {
    if (onDelete)
      onDelete();
  };
  moveUpButton.onClick = [this] {
    if (onMoveUp)
      onMoveUp();
  };
  moveDownButton.onClick = [this] {
    if (onMoveDown)
      onMoveDown();
  };

  // Connections drop-down could be added here in the future
}

NodeEditorPanel::~NodeEditorPanel() {}

void NodeEditorPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
  g.setColour(juce::Colours::grey);
  g.drawRect(getLocalBounds(), 1);
}

void NodeEditorPanel::resized() {
  auto bounds = getLocalBounds().reduced(5);

  auto topRow = bounds.removeFromTop(24);
  titleLabel.setBounds(topRow.removeFromLeft(topRow.getWidth() - 80));

  moveUpButton.setBounds(topRow.removeFromLeft(24));
  moveDownButton.setBounds(topRow.removeFromLeft(24));
  topRow.removeFromLeft(4); // spacer
  deleteButton.setBounds(topRow.removeFromLeft(24));

  if (customControls != nullptr) {
    bounds.removeFromTop(5); // spacer
    customControls->setBounds(bounds);
  }
}

void NodeEditorPanel::comboBoxChanged(juce::ComboBox *comboBoxThatHasChanged) {
  juce::ignoreUnused(comboBoxThatHasChanged);
}
