#pragma once

#include "AppSettings.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SettingsPanel : public juce::Component {
public:
  SettingsPanel();
  ~SettingsPanel() override = default;

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  juce::Label titleLabel{"title", "Settings"};

  juce::Label authorLabel{"authorLabel", "Default Author:"};
  juce::TextEditor authorEditor;

  juce::Label libraryRootLabel{"libraryRootLabel", "Patch Library Root:"};
  juce::TextEditor libraryRootEditor;
  juce::TextButton browseLibraryButton{"..."};

  std::unique_ptr<juce::FileChooser> fileChooser;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
