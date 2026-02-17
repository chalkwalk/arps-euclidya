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

  // Setup Graph Canvas
  addAndMakeVisible(graphViewport);
  graphCanvas = std::make_unique<GraphCanvas>(audioProcessor.graphEngine,
                                              audioProcessor.apvts,
                                              audioProcessor.graphLock);
  graphViewport.setViewedComponent(graphCanvas.get(), false);
  graphCanvas->rebuild();

  setSize(900, 700);
  startTimerHz(30);
}

EuclideanArpEditor::~EuclideanArpEditor() { stopTimer(); }

void EuclideanArpEditor::timerCallback() {
  // Periodically refresh macro display names to reflect any mapping changes
  audioProcessor.updateMacroNames();
  // Check for large sequences and update warning banner
  graphCanvas->checkForLargeSequences();
}

void EuclideanArpEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff111111));
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

  // Main Graph Canvas
  graphViewport.setBounds(bounds);
}

void EuclideanArpEditor::addNodeFromLibrary(const juce::String &nodeType) {
  auto newNode = NodeFactory::createNode(
      nodeType.toStdString(), audioProcessor.midiHandler,
      audioProcessor.clockManager, audioProcessor.macros);
  if (newNode) {
    graphCanvas->addNodeAtDefaultPosition(newNode);
  }
}
