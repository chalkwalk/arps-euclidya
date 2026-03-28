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

  // Call after loading a patch externally (e.g. from state restore)
  void refreshPatchName();

private:
  void toggleBrowser();
  void updateList();
  void loadPatchAtIndex(int index);
  void prevPatch();
  void nextPatch();
  void savePatch();
  void showMenu();

  ArpsEuclidyaProcessor &processor;
  PatchLibrary &library;

  bool isBrowserOpen = false;
  int currentPatchIndex = -1;
  std::vector<PatchLibrary::PatchInfo> currentResults;

  juce::TextButton prevButton{"<"};
  juce::TextButton nextButton{">"};
  juce::TextButton currentPatchButton{"Init"};
  juce::TextButton saveButton{"Save"};
  juce::TextButton menuButton{juce::String::charToString(0x2261)}; // ≡

  juce::Component browserOverlay;
  juce::TextEditor searchBox;
  juce::ComboBox bankSelector;
  juce::ListBox patchList;

  std::unique_ptr<juce::FileChooser> fileChooser;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchBrowserPanel)
};
