#include "ModuleLibraryPanel.h"

#include "ArpsLookAndFeel.h"
#include "NodeFactory.h"

ModuleLibraryPanel::ModuleLibraryPanel() : moduleList("ModuleList", this) {
  addAndMakeVisible(searchBox);
  searchBox.setTextToShowWhenEmpty("Search modules...", juce::Colours::grey);
  searchBox.setReturnKeyStartsNewLine(false);
  searchBox.setWantsKeyboardFocus(true);
  searchBox.addListener(this);

  addAndMakeVisible(moduleList);
  moduleList.setRowHeight(30);
  moduleList.setMultipleSelectionEnabled(false);

  allNodeTypes = NodeFactory::getAvailableNodeTypes();
  updateFilter();
}

ModuleLibraryPanel::~ModuleLibraryPanel() { searchBox.removeListener(this); }

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
  if (rowNumber < 0 || rowNumber >= (int)filteredNodeTypes.size()) return;

  if (rowIsSelected) g.fillAll(ArpsLookAndFeel::getBackgroundCharcoal());

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

//==============================================================================
ModuleLibraryPanel::LibraryListBox::LibraryListBox(const juce::String &name,
                                                   juce::ListBoxModel *m)
    : juce::ListBox(name, m) {}

void ModuleLibraryPanel::LibraryListBox::mouseDrag(const juce::MouseEvent &e) {
  if (auto *m = getListBoxModel()) {
    auto selectedRows = getSelectedRows();
    if (selectedRows.size() > 0 && e.mouseWasDraggedSinceMouseDown()) {
      if (auto *container =
              juce::DragAndDropContainer::findParentDragContainerFor(this)) {
        if (!container->isDragAndDropActive()) {
          auto desc = m->getDragSourceDescription(selectedRows);
          if (!desc.isVoid()) {
            juce::String nodeType = desc.toString();
            auto meta = NodeFactory::getPreviewMetadata(nodeType.toStdString());

            NodeDragPreview preview(nodeType, meta.gridW, meta.gridH,
                                    meta.numIn, meta.numOut);
            juce::Image snapshot =
                preview.createComponentSnapshot(preview.getLocalBounds());

            // Center the preview on the mouse
            juce::Point<int> offset(-preview.getWidth() / 2,
                                    -preview.getHeight() / 2);

            container->startDragging(
                desc, this, juce::ScaledImage(snapshot, 1.0f), true, &offset);
            return;
          }
        }
      }
    }
  }
  ListBox::mouseDrag(e);
}
