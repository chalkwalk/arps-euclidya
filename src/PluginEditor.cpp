#include "PluginEditor.h"
#include "MacroParameter.h"
#include "NodeFactory.h"

ArpsEuclidyaEditor::ArpsEuclidyaEditor(ArpsEuclidyaProcessor &p)
    : AudioProcessorEditor(p), audioProcessor(p),
      midiKeyboard(p.keyboardState,
                   juce::MidiKeyboardComponent::horizontalKeyboard) {

  // Apply custom Neon styling entirely
  juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

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

  graphCanvas->onNodeCloneRequest = [this](GraphNode *original, int gridX,
                                           int gridY) {
    if (original == nullptr)
      return;

    auto newNode = NodeFactory::createNode(
        original->getName(), audioProcessor.midiHandler,
        audioProcessor.clockManager, audioProcessor.macros);

    if (newNode) {
      // Copy state
      juce::XmlElement xml("Temp");
      original->saveNodeState(&xml);
      newNode->loadNodeState(&xml);

      newNode->gridX = gridX;
      newNode->gridY = gridY;
      newNode->nodeX =
          (float)(gridX * Layout::GridPitch) + Layout::TramlineOffset;
      newNode->nodeY =
          (float)(gridY * Layout::GridPitch) + Layout::TramlineOffset;

      audioProcessor.addNode(newNode);
      graphCanvas->rebuild();
      if (graphCanvas->onGraphChanged)
        graphCanvas->onGraphChanged();
    }
  };

  // Setup MIDI Keyboard and Clear Button
  addAndMakeVisible(midiKeyboard);
  midiKeyboard.setMidiChannel(1);
  midiKeyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                         lookAndFeel.getNeonColor());
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

  setResizable(true, true);
  setResizeLimits(600, 400, 1920, 1200);
  setSize(900, 700);

  openGLContext.attachTo(*this);
  startTimerHz(30);
}

ArpsEuclidyaEditor::~ArpsEuclidyaEditor() {
  openGLContext.detach();
  stopTimer();
  juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

void ArpsEuclidyaEditor::timerCallback() {
  if (audioProcessor.macrosDirty.exchange(false)) {
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
  }

  // Check for large sequences and update warning banner
  graphCanvas->checkForLargeSequences();
}

void ArpsEuclidyaEditor::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff111111));
  g.setColour(juce::Colour(0xff222222));
  if (isSidebarExpanded)
    g.fillRect(libraryPanel.getBounds());
  g.fillRect(macroBar.getBounds());
}

void ArpsEuclidyaEditor::resized() {
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

void ArpsEuclidyaEditor::addNodeFromLibrary(const juce::String &nodeType) {
  auto newNode = NodeFactory::createNode(
      nodeType.toStdString(), audioProcessor.midiHandler,
      audioProcessor.clockManager, audioProcessor.macros);
  if (newNode) {
    graphCanvas->addNodeAtDefaultPosition(newNode);
  }
}
