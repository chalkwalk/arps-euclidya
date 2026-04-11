#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "TuningLibrary.h"

class ArpsEuclidyaProcessor;

class TuningPanel : public juce::Component, public juce::ListBoxModel {
 public:
  TuningPanel(ArpsEuclidyaProcessor &p, TuningLibrary &lib);
  ~TuningPanel() override = default;

  void paint(juce::Graphics &g) override;
  void resized() override;

  // ListBoxModel
  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override;
  void listBoxItemClicked(int row, const juce::MouseEvent &e) override;

  // Call after loading a patch externally to refresh the displayed tuning name.
  void refreshTuningName();

  [[nodiscard]] int getPreferredHeight() const { return isBrowserOpen ? 400 : 40; }

 private:
  void toggleBrowser();
  void updateList();
  void repopulateCategories();
  void applyTuningAtIndex(int index);
  void showMenu();

  ArpsEuclidyaProcessor &processor;
  TuningLibrary &library;

  bool isBrowserOpen = false;
  int currentTuningIndex = -1;
  std::vector<TuningLibrary::TuningInfo> currentResults;

  juce::TextButton nameButton{"12-TET"};
  juce::TextButton menuButton{juce::String::charToString(0x2261)};  // ≡

  juce::Component browserOverlay;
  juce::TextEditor searchBox;
  juce::ComboBox bankSelector;
  juce::ComboBox categoryFilter;
  juce::ListBox tuningList;

  std::unique_ptr<juce::FileChooser> fileChooser;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TuningPanel)
};
