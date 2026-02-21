#include "PluginEditor.h"
#include "NodeFactory.h"

EuclideanArpEditor::EuclideanArpEditor(EuclideanArpProcessor &p)
    : AudioProcessorEditor(p), audioProcessor(p) {

  // Apply custom Neon styling entirely
  juce::LookAndFeel::setDefaultLookAndFeel(&customLookAndFeel);

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
  addAndMakeVisible(libraryPanel);
  libraryPanel.onNodeSelected = [this](const juce::String &nt) {
    addNodeFromLibrary(nt);
  };

  addAndMakeVisible(toggleSidebarButton);
  toggleSidebarButton.setButtonText("<");
  toggleSidebarButton.onClick = [this]() {
    isSidebarExpanded = !isSidebarExpanded;
    toggleSidebarButton.setButtonText(isSidebarExpanded ? "<" : ">");
    resized();
  };

  // Setup Graph Canvas
  graphCanvas = std::make_unique<GraphCanvas>(audioProcessor.graphEngine,
                                              audioProcessor.apvts,
                                              audioProcessor.graphLock);
  addAndMakeVisible(graphCanvas.get());
  graphCanvas->rebuild();

  setSize(900, 700);
  startTimerHz(30);
}

EuclideanArpEditor::~EuclideanArpEditor() {
  stopTimer();
  juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void EuclideanArpEditor::timerCallback() {
  // Periodically refresh macro display names to reflect any mapping changes
  audioProcessor.updateMacroNames();
  // Check for large sequences and update warning banner
  graphCanvas->checkForLargeSequences();
}

void EuclideanArpEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff111111));
  g.setColour(juce::Colour(0xff222222));
  if (isSidebarExpanded)
    g.fillRect(libraryPanel.getBounds());
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
  int sidebarWidth = isSidebarExpanded ? 180 : 0;

  auto libraryBounds = bounds.removeFromLeft(sidebarWidth);
  libraryPanel.setBounds(libraryBounds);

  auto toggleBounds = bounds.removeFromLeft(20);
  toggleSidebarButton.setBounds(toggleBounds);

  // Main Graph Canvas
  if (graphCanvas != nullptr) {
    graphCanvas->setBounds(bounds);
  }
}

void EuclideanArpEditor::addNodeFromLibrary(const juce::String &nodeType) {
  auto newNode = NodeFactory::createNode(
      nodeType.toStdString(), audioProcessor.midiHandler,
      audioProcessor.clockManager, audioProcessor.macros);
  if (newNode) {
    graphCanvas->addNodeAtDefaultPosition(newNode);
  }
}
