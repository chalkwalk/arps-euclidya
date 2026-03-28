#pragma once

#include <JuceHeader.h>

#include <string>
#include <vector>

#include "NodeBlock.h"  // For NodeDragPreview

class ModuleLibraryPanel : public juce::Component,
                           public juce::ListBoxModel,
                           private juce::TextEditor::Listener {
 public:
  ModuleLibraryPanel();
  ~ModuleLibraryPanel() override;

  void paint(juce::Graphics &g) override;
  void resized() override;

  // Callback fired when a user presses Enter on a single result, or double
  // clicks an item
  std::function<void(const juce::String &)> onNodeSelected;

  // ListBoxModel overrides
  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override;
  void listBoxItemClicked(int rowNumber, const juce::MouseEvent &e) override;
  void listBoxItemDoubleClicked(int rowNumber,
                                const juce::MouseEvent &e) override;
  juce::var getDragSourceDescription(
      const juce::SparseSet<int> &selectedRows) override;

  // To allow the parent to focus the search bar directly (e.g., on a hotkey)
  void focusSearch();

 private:
  class LibraryListBox : public juce::ListBox {
   public:
    LibraryListBox(const juce::String &name, juce::ListBoxModel *m);
    void mouseDrag(const juce::MouseEvent &e) override;
  };

  void textEditorTextChanged(juce::TextEditor &editor) override;
  void textEditorReturnKeyPressed(juce::TextEditor &editor) override;
  void updateFilter();

  juce::TextEditor searchBox;
  LibraryListBox moduleList;

  std::vector<std::string> allNodeTypes;
  std::vector<std::string> filteredNodeTypes;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ModuleLibraryPanel)
};
