#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "AppSettings.h"

class ArpsEuclidyaProcessor;

class SettingsPanel : public juce::Component, public juce::Timer {
 public:
  SettingsPanel(ArpsEuclidyaProcessor &processor);
  ~SettingsPanel() override;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void timerCallback() override;

 private:
  ArpsEuclidyaProcessor &processor;
  juce::Label titleLabel{"title", "Settings"};

  juce::Label authorLabel{"authorLabel", "Default Author:"};
  juce::TextEditor authorEditor;

  juce::Label libraryRootLabel{"libraryRootLabel", "Patch Library Root:"};
  juce::TextEditor libraryRootEditor;
  juce::TextButton browseLibraryButton{"..."};

  std::unique_ptr<juce::FileChooser> fileChooser;

  juce::Label diagLabel{"diagLabel", "MIDI Diagnostics:"};
  juce::TextEditor diagEditor;
  juce::TextButton clearDiagButton{"Clear Log"};
  juce::TextButton copyDiagButton{"Copy Log"};
  juce::ToggleButton showDiagToggle{"Show MIDI Log"};
  bool diagVisible = false;

  juce::ToggleButton mpeEnabledToggle{"Enable MPE"};
  juce::ToggleButton ignoreMpeMasterPressureToggle{
      "Ignore MPE Master Channel Pressure"};

  void updateMpeState();

  int logLineCount = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
