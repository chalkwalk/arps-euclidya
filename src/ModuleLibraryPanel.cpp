#include "ModuleLibraryPanel.h"
#include "NodeFactory.h"

ModuleLibraryPanel::ModuleLibraryPanel() {
  addAndMakeVisible(searchBox);
  searchBox.setTextToShowWhenEmpty("Search modules...", juce::Colours::grey);
  searchBox.setReturnKeyStartsNewLine(false);
  searchBox.setWantsKeyboardFocus(true);
  searchBox.addListener(this);

  addAndMakeVisible(moduleList);
  moduleList.setModel(this);
  moduleList.setRowHeight(30);
  moduleList.setMultipleSelectionEnabled(false);

  allNodeTypes = NodeFactory::getAvailableNodeTypes();
  updateFilter();
}

ModuleLibraryPanel::~ModuleLibraryPanel() {
  searchBox.removeListener(this);
  moduleList.setModel(nullptr);
}

void ModuleLibraryPanel::paint(juce::Graphics &g) {
  g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void ModuleLibraryPanel::resized() {
  auto bounds = getLocalBounds();
  searchBox.setBounds(bounds.removeFromTop(30).reduced(2));
  moduleList.setBounds(bounds);
}

int ModuleLibraryPanel::getNumRows() { return (int)filteredNodeTypes.size(); }

void ModuleLibraryPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                          int width, int height,
                                          bool rowIsSelected) {
  if (rowNumber < 0 || rowNumber >= (int)filteredNodeTypes.size())
    return;

  if (rowIsSelected)
    g.fillAll(findColour(juce::ListBox::backgroundColourId).brighter(0.1f));

  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(filteredNodeTypes[(size_t)rowNumber], 10, 0, width - 20, height,
             juce::Justification::centredLeft, true);
}

void ModuleLibraryPanel::listBoxItemClicked(int rowNumber,
                                            const juce::MouseEvent &e) {
  juce::ignoreUnused(rowNumber, e);
  // Single click just selects; drag is handled by getDragSourceDescription.
}

juce::var ModuleLibraryPanel::getDragSourceDescription(
    const juce::SparseSet<int> &selectedRows) {
  if (selectedRows.size() > 0) {
    int row = selectedRows[0];
    if (row >= 0 && row < (int)filteredNodeTypes.size()) {
      return juce::var(juce::String(filteredNodeTypes[(size_t)row]));
    }
  }
  return {};
}

void ModuleLibraryPanel::listBoxItemDoubleClicked(int rowNumber,
                                                  const juce::MouseEvent &e) {
  juce::ignoreUnused(e);
  if (rowNumber >= 0 && rowNumber < (int)filteredNodeTypes.size()) {
    if (onNodeSelected) {
      onNodeSelected(filteredNodeTypes[(size_t)rowNumber]);
      searchBox.clear();
      updateFilter();
    }
  }
}

void ModuleLibraryPanel::textEditorTextChanged(juce::TextEditor &editor) {
  juce::ignoreUnused(editor);
  updateFilter();
}

void ModuleLibraryPanel::textEditorReturnKeyPressed(juce::TextEditor &editor) {
  juce::ignoreUnused(editor);
  // "Enter-to-add" single result functionality
  if (filteredNodeTypes.size() == 1) {
    if (onNodeSelected) {
      onNodeSelected(filteredNodeTypes[0]);

      // Clear search after adding to quickly add something else
      searchBox.clear();
      updateFilter();
    }
  }
}

void ModuleLibraryPanel::updateFilter() {
  auto searchText = searchBox.getText().trim().toLowerCase();

  filteredNodeTypes.clear();
  for (const auto &node : allNodeTypes) {
    if (searchText.isEmpty() ||
        juce::String(node).toLowerCase().contains(searchText)) {
      filteredNodeTypes.push_back(node);
    }
  }

  moduleList.updateContent();
}

void ModuleLibraryPanel::focusSearch() { searchBox.grabKeyboardFocus(); }
