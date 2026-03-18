#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class PatchManagementPanel : public juce::Component {
public:
  PatchManagementPanel(ArpsEuclidyaProcessor &p);
  ~PatchManagementPanel() override = default;

  void paint(juce::Graphics &g) override;
  void resized() override;

private:
  void savePatch();
  void loadPatch();

  ArpsEuclidyaProcessor &processor;

  juce::TextButton saveButton{"Save Patch"};
  juce::TextButton loadButton{"Load Patch"};

  std::unique_ptr<juce::FileChooser> fileChooser;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchManagementPanel)
};
