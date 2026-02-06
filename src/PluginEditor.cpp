#include "PluginEditor.h"
#include "NodeFactory.h"

EuclideanArpEditor::EuclideanArpEditor(EuclideanArpProcessor &p)
    : AudioProcessorEditor(p), audioProcessor(p) {

  // Setup Macros
  addAndMakeVisible(macroBar);
  for (int i = 0; i < 32; ++i) {
    auto slider = new juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag,
                                   juce::Slider::NoTextBox);
    macroBar.addAndMakeVisible(slider);
    macroSliders.add(slider);

    juce::String paramId = "macro_" + juce::String(i + 1);
    macroAttachments.add(
        new juce::AudioProcessorValueTreeState::SliderAttachment(
            audioProcessor.apvts, paramId, *slider));
  }

  // Setup Library
  addAndMakeVisible(libraryViewport);
  libraryViewport.setViewedComponent(&libraryContent, false);
  auto nodeTypes = NodeFactory::getAvailableNodeTypes();
  for (const auto &nt : nodeTypes) {
    auto btn = new juce::TextButton(nt);
    libraryContent.addAndMakeVisible(btn);
    libraryButtons.add(btn);
    btn->onClick = [this, nt]() { addNodeFromLibrary(nt); };
  }

  // Setup Graph
  addAndMakeVisible(graphViewport);
  graphViewport.setViewedComponent(&graphContent, false);

  rebuildGraphUI();

  setSize(900, 700);
  startTimerHz(30);
}

EuclideanArpEditor::~EuclideanArpEditor() { stopTimer(); }

void EuclideanArpEditor::timerCallback() {}

void EuclideanArpEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff111111)); // Premium dark mode background
  g.setColour(juce::Colour(0xff222222));
  g.fillRect(libraryViewport.getBounds());
  g.fillRect(macroBar.getBounds());
}

void EuclideanArpEditor::resized() {
  auto bounds = getLocalBounds();

  // Macro bar at the top
  auto macroBounds = bounds.removeFromTop(80);
  macroBar.setBounds(macroBounds);

  int sliderWidth = macroBounds.getWidth() / 16;
  int sliderHeight = 40;

  for (int i = 0; i < 32; ++i) {
    int row = i / 16;
    int col = i % 16;
    macroSliders[i]->setBounds(col * sliderWidth, row * sliderHeight,
                               sliderWidth, sliderHeight);
  }

  // Left Library
  auto libraryBounds = bounds.removeFromLeft(150);
  libraryViewport.setBounds(libraryBounds);

  int btnY = 5;
  for (auto btn : libraryButtons) {
    btn->setBounds(5, btnY, 140, 30);
    btnY += 35;
  }
  libraryContent.setBounds(0, 0, 150, btnY);

  // Main Graph
  graphViewport.setBounds(bounds);

  int panelY = 5;
  for (auto panel : nodePanels) {
    // Dynamic height: use the custom controls' preferred height + title bar
    int panelHeight = 220; // default
    auto *controls = panel->getChildComponent(
        panel->getNumChildComponents() - 1); // last child is customControls
    if (controls != nullptr) {
      panelHeight =
          controls->getHeight() + 34; // 24px title + 5px spacer + 5px padding
    }
    panel->setBounds(10, panelY, bounds.getWidth() - 40, panelHeight);
    panelY += panelHeight + 10;
  }
  graphContent.setBounds(0, 0, bounds.getWidth() - 20, panelY);
}

void EuclideanArpEditor::rebuildGraphUI() {
  nodePanels.clear();

  const juce::ScopedLock sl(audioProcessor.graphLock);
  auto nodes = audioProcessor.graphEngine.getNodes();

  for (size_t i = 0; i < nodes.size(); ++i) {
    auto node = nodes[i];
    auto panel = new NodeEditorPanel(node, audioProcessor.apvts);

    panel->onDelete = [this, node]() {
      audioProcessor.removeNode(node.get());
      rebuildGraphUI();
    };
    panel->onMoveUp = [this, node, i]() {
      audioProcessor.moveNode(node.get(), i - 1);
      rebuildGraphUI();
    };
    panel->onMoveDown = [this, node, i]() {
      audioProcessor.moveNode(node.get(), i + 1);
      rebuildGraphUI();
    };

    graphContent.addAndMakeVisible(panel);
    nodePanels.add(panel);
  }

  resized(); // trigger relayout of graphContent
}

void EuclideanArpEditor::addNodeFromLibrary(const juce::String &nodeType) {
  auto newNode = NodeFactory::createNode(
      nodeType.toStdString(), audioProcessor.midiHandler,
      audioProcessor.clockManager, audioProcessor.macros);
  if (newNode) {
    audioProcessor.addNode(newNode);
    rebuildGraphUI();
  }
}
