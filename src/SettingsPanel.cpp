#include "SettingsPanel.h"

#include "PluginProcessor.h"

SettingsPanel::SettingsPanel(ArpsEuclidyaProcessor &p) : processor(p) {
  addAndMakeVisible(titleLabel);
  titleLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
  titleLabel.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(authorLabel);
  addAndMakeVisible(authorEditor);
  authorEditor.setText(AppSettings::getInstance().getDefaultAuthor(), false);
  authorEditor.onTextChange = [this]() {
    AppSettings::getInstance().setDefaultAuthor(authorEditor.getText());
  };

  addAndMakeVisible(libraryRootLabel);
  addAndMakeVisible(libraryRootEditor);
  libraryRootEditor.setText(AppSettings::getInstance().getPatchLibraryRoot(),
                            false);
  libraryRootEditor.onTextChange = [this]() {
    AppSettings::getInstance().setPatchLibraryRoot(libraryRootEditor.getText());
  };

  addAndMakeVisible(browseLibraryButton);
  browseLibraryButton.onClick = [this]() {
    juce::File currentFolder =
        juce::File(AppSettings::getInstance().getPatchLibraryRoot());
    if (!currentFolder.isDirectory()) {
      currentFolder =
          juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    }

    fileChooser = std::make_unique<juce::FileChooser>(
        "Select Patch Library Root", currentFolder);

    auto flags = juce::FileBrowserComponent::canSelectDirectories |
                 juce::FileBrowserComponent::openMode;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser &fc) {
      if (fc.getResult().isDirectory()) {
        juce::String path = fc.getResult().getFullPathName();
        libraryRootEditor.setText(path);
        AppSettings::getInstance().setPatchLibraryRoot(path);
      }
    });
  };

  // --- MPE Settings ---
  addAndMakeVisible(mpeEnabledToggle);
  mpeEnabledToggle.setToggleState(AppSettings::getInstance().getMpeEnabled(),
                                  juce::dontSendNotification);
  mpeEnabledToggle.onClick = [this]() {
    bool state = mpeEnabledToggle.getToggleState();
    AppSettings::getInstance().setMpeEnabled(state);
    processor.getNoteExpressionManager().setLegacyMode(!state);
    updateMpeState();
  };

  addAndMakeVisible(ignoreMpeMasterPressureToggle);
  ignoreMpeMasterPressureToggle.setToggleState(
      AppSettings::getInstance().getIgnoreMpeMasterPressure(),
      juce::dontSendNotification);
  ignoreMpeMasterPressureToggle.onClick = [this]() {
    bool state = ignoreMpeMasterPressureToggle.getToggleState();
    AppSettings::getInstance().setIgnoreMpeMasterPressure(state);
    processor.getNoteExpressionManager().setIgnoreMpeMasterPressure(state);
  };
  updateMpeState();

  // --- Collapsible MIDI Diagnostics ---
  addAndMakeVisible(showDiagToggle);
  showDiagToggle.setToggleState(false, juce::dontSendNotification);
  showDiagToggle.onClick = [this]() {
    diagVisible = showDiagToggle.getToggleState();
    diagLabel.setVisible(diagVisible);
    diagEditor.setVisible(diagVisible);
    clearDiagButton.setVisible(diagVisible);
    copyDiagButton.setVisible(diagVisible);
    resized();
  };

  addChildComponent(diagLabel);
  addChildComponent(diagEditor);
  diagEditor.setMultiLine(true);
  diagEditor.setReadOnly(true);
  diagEditor.setScrollbarsShown(true);
  diagEditor.setCaretVisible(false);

  addChildComponent(clearDiagButton);
  clearDiagButton.onClick = [this]() {
    diagEditor.clear();
    logLineCount = 0;
  };

  addChildComponent(copyDiagButton);
  copyDiagButton.onClick = [this]() {
    juce::SystemClipboard::copyTextToClipboard(diagEditor.getText());
  };

  startTimerHz(15);
  setSize(400, 500);
}

SettingsPanel::~SettingsPanel() { stopTimer(); }

void SettingsPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
}

void SettingsPanel::resized() {
  auto bounds = getLocalBounds().reduced(15);

  titleLabel.setBounds(bounds.removeFromTop(30));
  bounds.removeFromTop(10);

  auto authorRow = bounds.removeFromTop(24);
  authorLabel.setBounds(authorRow.removeFromLeft(120));
  authorEditor.setBounds(authorRow);

  bounds.removeFromTop(10);

  auto rootRow = bounds.removeFromTop(24);
  libraryRootLabel.setBounds(rootRow.removeFromLeft(120));
  browseLibraryButton.setBounds(rootRow.removeFromRight(30));
  rootRow.removeFromRight(5);
  libraryRootEditor.setBounds(rootRow);

  bounds.removeFromTop(10);

  // --- MPE section ---
  mpeEnabledToggle.setBounds(bounds.removeFromTop(24));
  bounds.removeFromTop(4);
  auto ignoreRow = bounds.removeFromTop(24);
  ignoreRow.removeFromLeft(20);  // indent under MPE toggle
  ignoreMpeMasterPressureToggle.setBounds(ignoreRow);

  bounds.removeFromTop(10);

  // --- Collapsible diagnostics ---
  showDiagToggle.setBounds(bounds.removeFromTop(24));

  if (diagVisible) {
    bounds.removeFromTop(5);

    auto diagHeaderRow = bounds.removeFromTop(24);
    diagLabel.setBounds(diagHeaderRow.removeFromLeft(200));
    copyDiagButton.setBounds(diagHeaderRow.removeFromRight(80));
    diagHeaderRow.removeFromRight(5);
    clearDiagButton.setBounds(diagHeaderRow.removeFromRight(80));

    bounds.removeFromTop(5);
    diagEditor.setBounds(bounds);
  }
}

void SettingsPanel::updateMpeState() {
  bool mpeOn = mpeEnabledToggle.getToggleState();
  ignoreMpeMasterPressureToggle.setEnabled(mpeOn);
  if (!mpeOn) {
    ignoreMpeMasterPressureToggle.setToggleState(false,
                                                 juce::dontSendNotification);
    AppSettings::getInstance().setIgnoreMpeMasterPressure(false);
    processor.getNoteExpressionManager().setIgnoreMpeMasterPressure(false);
  }
}

void SettingsPanel::timerCallback() {
  int start1, size1, start2, size2;
  processor.midiLogFifo.prepareToRead(128, start1, size1, start2, size2);

  if (size1 > 0 || size2 > 0) {
    auto appendLog = [this](const MidiLogEvent &ev) {
      if (logLineCount > 1000) {
        diagEditor.clear();
        logLineCount = 0;
      }
      juce::String typeStr;
      if (ev.logType == 10)
        typeStr = "In NoteOn";
      else if (ev.logType == 11)
        typeStr = "In NoteOff";
      else if (ev.logType == 12)
        typeStr = "In CC";
      else if (ev.logType == 13)
        typeStr = "In ChanPress";
      else if (ev.logType == 14)
        typeStr = "In PitchBend";
      else if (ev.logType == 15)
        typeStr = "In PolyAT";
      else if (ev.logType == 0)
        typeStr = "Out NoteOn ";
      else if (ev.logType == 1)
        typeStr = "Out NoteOff";
      else if (ev.logType == 2)
        typeStr = "Out CC     ";
      else if (ev.logType == 3)
        typeStr = "Out TICK   ";
      else if (ev.logType == 4)
        typeStr = "Out ArpOn  ";
      else if (ev.logType == 5)
        typeStr = "Out ArpOff ";
      else if (ev.logType == 7)
        typeStr = "Out Arp PB ";
      else if (ev.logType == 20)
        typeStr = "CLAP NoteOn";
      else if (ev.logType == 21)
        typeStr = "CLAP NoteOff";
      else if (ev.logType == 22)
        typeStr = "CLAP Tuning";
      else if (ev.logType == 25)
        typeStr = "PROTOCOL   ";
      else
        typeStr = "Unknown (" + juce::String(ev.logType) + ")";

      // Format PPQ as bar.beat.sixteenth (4/4 assumed, 1-indexed)
      int totalSixteenths = (int)(ev.ppq * 4.0);
      int bar = totalSixteenths / 16 + 1;
      int beat = (totalSixteenths % 16) / 4 + 1;
      int sixteenth = totalSixteenths % 4 + 1;
      juce::String pos = juce::String(bar) + "." + juce::String(beat) + "." +
                         juce::String(sixteenth);

      juce::String msg = pos + " [" + typeStr + "] Ch: " +
                         juce::String(ev.channel) +
                         (ev.logType >= 20 ? " | Note: " : " | d1: ") +
                         juce::String(ev.data1) +
                         " | d2: " + juce::String(ev.data2) + "\n";
      diagEditor.insertTextAtCaret(msg);
      logLineCount++;
    };

    for (int i = 0; i < size1; ++i) {
      appendLog(processor.midiLogBuffer[(size_t)(start1 + i)]);
    }
    for (int i = 0; i < size2; ++i) {
      appendLog(processor.midiLogBuffer[(size_t)(start2 + i)]);
    }
    processor.midiLogFifo.finishedRead(size1 + size2);
  }
}
