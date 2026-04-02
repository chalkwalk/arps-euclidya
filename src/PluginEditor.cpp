#include "PluginEditor.h"

#include "MacroParameter.h"
#include "NodeFactory.h"

ArpsEuclidyaEditor::ArpsEuclidyaEditor(ArpsEuclidyaProcessor &p)
    : AudioProcessorEditor(p),
      audioProcessor(p),
      patchBrowser(p, patchLibrary),
      midiKeyboard(p.keyboardState,
                   juce::MidiKeyboardComponent::horizontalKeyboard) {
  // Apply custom Neon styling entirely
  juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

  // Setup Macros
  addAndMakeVisible(macroBar);
  for (int i = 0; i < 32; ++i) {
    auto *wrapper = new MacroControl();
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

  addAndMakeVisible(patchBrowser);

  // Standalone Transport (Only if running as standalone app)
  if (juce::PluginHostType::getPluginLoadedAs() ==
      juce::AudioProcessor::wrapperType_Standalone) {
    transportBar = std::make_unique<TransportBar>(audioProcessor.clockManager);
    addAndMakeVisible(transportBar.get());
  }

  // Setup Graph Canvas
  graphCanvas = std::make_unique<GraphCanvas>(audioProcessor.graphEngine,
                                              audioProcessor.apvts,
                                              audioProcessor.graphLock);
  addAndMakeVisible(graphCanvas.get());

  graphCanvas->performMutation = [this](std::function<void()> mutation) {
    audioProcessor.performGraphMutation(std::move(mutation));
  };
  graphCanvas->onUndo = [this] { audioProcessor.undoManager.undo(); };
  graphCanvas->onRedo = [this] { audioProcessor.undoManager.redo(); };

  graphCanvas->onNodeDropped = [this](const juce::String &nodeType,
                                      juce::Point<int> screenPos) {
    audioProcessor.performGraphMutation([this, nodeType, screenPos]() {
      auto newNode = NodeFactory::createNode(
          nodeType.toStdString(), audioProcessor.noteExpressionManager,
          audioProcessor.clockManager, audioProcessor.macros);
      if (newNode) {
        graphCanvas->addNodeAtPosition(newNode, screenPos);
        if (!graphCanvas->attemptSignalPathInsertion(newNode.get(),
                                                     screenPos)) {
          graphCanvas->attemptProximityConnection(newNode.get(), screenPos);
        }
      }
    });
  };

  graphCanvas->onNodeReplaceRequest = [this](GraphNode *oldNode,
                                             const juce::String &newType) {
    if (oldNode == nullptr) {
      return;
    }

    audioProcessor.performGraphMutation([this, oldNode, newType]() {
      auto newNode = NodeFactory::createNode(
          newType.toStdString(), audioProcessor.noteExpressionManager,
          audioProcessor.clockManager, audioProcessor.macros);

      if (newNode) {
        newNode->gridX = oldNode->gridX;
        newNode->gridY = oldNode->gridY;
        newNode->nodeX = oldNode->nodeX;
        newNode->nodeY = oldNode->nodeY;

        // Transfer Inputs
        int inLimit =
            std::min(oldNode->getNumInputPorts(), newNode->getNumInputPorts());
        for (int i = 0; i < inLimit; ++i) {
          // Find source for oldNode's input i
          for (const auto &n : audioProcessor.graphEngine.getNodes()) {
            for (const auto &[outPort, connVec] : n->getConnections()) {
              for (const auto &conn : connVec) {
                if (conn.targetNode == oldNode && conn.targetInputPort == i) {
                  // Transfer to new node
                  audioProcessor.graphEngine.addExplicitConnection(
                      n.get(), outPort, newNode.get(), i);
                }
              }
            }
          }
        }

        // Transfer Outputs
        int outLimit = std::min(oldNode->getNumOutputPorts(),
                                newNode->getNumOutputPorts());
        for (int i = 0; i < outLimit; ++i) {
          const auto &conns = oldNode->getConnections();
          if (conns.count(i)) {
            for (const auto &conn : conns.at(i)) {
              audioProcessor.graphEngine.addExplicitConnection(
                  newNode.get(), i, conn.targetNode, conn.targetInputPort);
            }
          }
        }

        audioProcessor.removeNode(oldNode);
        audioProcessor.addNode(newNode);
        graphCanvas->rebuild();
      }
    });
    if (graphCanvas->onGraphChanged) {
      graphCanvas->onGraphChanged();
    }
  };

  graphCanvas->onNodeCloneRequest = [this](GraphNode *original, int gridX,
                                           int gridY) {
    if (original == nullptr) {
      return;
    }

    audioProcessor.performGraphMutation([this, original, gridX, gridY]() {
      auto newNode = NodeFactory::createNode(
          original->getName(), audioProcessor.noteExpressionManager,
          audioProcessor.clockManager, audioProcessor.macros);

      if (newNode) {
        // Clone state from original
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
      }
    });

    if (graphCanvas->onGraphChanged) {
      graphCanvas->onGraphChanged();
    }
  };

  graphCanvas->onTransportToggle = [this]() {
    // Only toggle for standalone
    if (juce::PluginHostType::getPluginLoadedAs() ==
        juce::AudioProcessor::wrapperType_Standalone) {
      audioProcessor.clockManager.setPlaying(
          !audioProcessor.clockManager.isStandaloneRunning());
    }
  };

  // Setup MIDI Keyboard and Clear Button
  addAndMakeVisible(midiKeyboard);
  midiKeyboard.setMidiChannel(1);
  midiKeyboard.setColour(juce::MidiKeyboardComponent::keyDownOverlayColourId,
                         ArpsLookAndFeel::getNeonColor());
  midiKeyboard.clearKeyMappings();  // Disable computer keyboard input
  midiKeyboard.setWantsKeyboardFocus(false);

  addAndMakeVisible(clearNotesButton);
  clearNotesButton.setButtonText("Clear Notes");
  clearNotesButton.onClick = [this]() {
    audioProcessor.keyboardState.allNotesOff(0);
    audioProcessor.noteExpressionManager.clearAllNotes();
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

        bool mapped = audioProcessor.macroParams[(size_t)i]->isMapped();
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
  if (isSidebarExpanded) {
    g.fillRect(libraryPanel.getBounds());
  }
  g.fillRect(macroBar.getBounds());
  g.fillRect(patchBrowser.getBounds());
}

void ArpsEuclidyaEditor::resized() {
  auto bounds = getLocalBounds();

  // Patch Browser (height is 40 when collapsed, 400 when browser is open)
  int patchHeight =
      patchBrowser.getHeight() > 0 ? patchBrowser.getHeight() : 40;
  auto patchBounds = bounds.removeFromTop(patchHeight);
  patchBrowser.setBounds(patchBounds);

  // Standalone Transport Bar (only layout if present)
  if (transportBar != nullptr) {
    auto transportBounds = bounds.removeFromTop(40);
    transportBar->setBounds(transportBounds);
  }

  // Macro bar below patch panel
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
      nodeType.toStdString(), audioProcessor.noteExpressionManager,
      audioProcessor.clockManager, audioProcessor.macros);
  if (newNode) {
    graphCanvas->addNodeAtDefaultPosition(newNode);
  }
}

void ArpsEuclidyaEditor::rebuildCanvas() {
  if (graphCanvas != nullptr) {
    graphCanvas->rebuild();
  }
}
