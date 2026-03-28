#pragma once

#include "PatchLibrary.h"
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class PatchBrowserPanel : public juce::Component, public juce::ListBoxModel {
public:
  PatchBrowserPanel(ArpsEuclidyaProcessor &p, PatchLibrary &lib);
  ~PatchBrowserPanel() override = default;

  void paint(juce::Graphics &g) override;
  void resized() override;

  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override;
  void listBoxItemClicked(int row, const juce::MouseEvent &) override;

private:
  void toggleBrowser();
  void updateList();
  void savePatch();
  void showMenu();

  ArpsEuclidyaProcessor &processor;
  PatchLibrary &library;

  bool isBrowserOpen = false;
  std::vector<PatchLibrary::PatchInfo> currentResults;

  juce::TextButton prevButton{"<"};
  juce::TextButton nextButton{">"};
  juce::TextButton currentPatchButton{"Default Patch"};
  juce::TextButton saveButton{"Save"};
  juce::TextButton menuButton{"..."};

  juce::Component browserOverlay;
  juce::TextEditor searchBox;
  juce::ComboBox bankSelector;
  juce::ListBox patchList;

  std::unique_ptr<juce::FileChooser> fileChooser;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchBrowserPanel)
};
