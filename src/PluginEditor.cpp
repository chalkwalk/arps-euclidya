#include "PluginEditor.h"
#include "MacroParameter.h"
#include "NodeFactory.h"

EuclideanArpEditor::EuclideanArpEditor(EuclideanArpProcessor &p)
    : AudioProcessorEditor(p), audioProcessor(p),
      midiKeyboard(p.keyboardState,
                   juce::MidiKeyboardComponent::horizontalKeyboard) {

  // Apply custom Neon styling entirely
  juce::LookAndFeel::setDefaultLookAndFeel(&customLookAndFeel);

  // Setup Macros
  addAndMakeVisible(macroBar);
  for (int i = 0; i < 32; ++i) {
    auto wrapper = new MacroControl();
    macroBar.addAndMakeVisible(wrapper);
    macroControls.add(wrapper);

    juce::String paramId = "macro_" + juce::String(i + 1);
    macroAttachments.add(
        new juce::AudioProcessorValueTreeState::SliderAttachment(
            audioProcessor.apvts, paramId, wrapper->slider));
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

  graphCanvas->onNodeDropped = [this](const juce::String &nodeType,
                                      juce::Point<int> screenPos) {
    auto newNode = NodeFactory::createNode(
        nodeType.toStdString(), audioProcessor.midiHandler,
        audioProcessor.clockManager, audioProcessor.macros);
    if (newNode) {
      graphCanvas->addNodeAtPosition(newNode, screenPos);
    }
  };

  // Setup MIDI Keyboard and Clear Button
  addAndMakeVisible(midiKeyboard);
  midiKeyboard.setMidiChannel(1);
  midiKeyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                         customLookAndFeel.getNeonColor());
  midiKeyboard.setKeyPressBaseOctave(-1); // Disable computer keyboard input
  midiKeyboard.setWantsKeyboardFocus(false);

  addAndMakeVisible(clearNotesButton);
  clearNotesButton.setButtonText("Clear Notes");
  clearNotesButton.onClick = [this]() {
    audioProcessor.keyboardState.allNotesOff(0);
    audioProcessor.midiHandler.clearAllNotes();
    audioProcessor.graphEngine.recalculate();
  };

  graphCanvas->rebuild();

  setSize(900, 700);
  setResizable(true, true);
  setResizeLimits(600, 400, 1920, 1200);
  startTimerHz(30);
}

EuclideanArpEditor::~EuclideanArpEditor() {
  stopTimer();
  juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void EuclideanArpEditor::timerCallback() {
  // Periodically refresh macro display names to reflect any mapping changes
  audioProcessor.updateMacroNames();

  for (int i = 0; i < 32; ++i) {
    if (audioProcessor.macroParams[(size_t)i] != nullptr) {
      juce::String newName =
          audioProcessor.macroParams[(size_t)i]->getName(100);
      macroControls[i]->label.setText(newName, juce::dontSendNotification);

      bool mapped = !newName.startsWith("Macro ");
      if (macroControls[i]->isMapped != mapped) {
        macroControls[i]->isMapped = mapped;
        macroControls[i]->repaint();
      }
    }
  }

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
    macroControls[i]->setBounds(col * sliderWidth, row * sliderHeight,
                                sliderWidth, sliderHeight);
  }

  // Bottom Keyboard bar
  auto bottomBounds = bounds.removeFromBottom(80);

  auto clearBtnBounds = bottomBounds.removeFromLeft(120).reduced(10, 20);
  clearNotesButton.setBounds(clearBtnBounds);

  midiKeyboard.setBounds(bottomBounds.reduced(4));

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
