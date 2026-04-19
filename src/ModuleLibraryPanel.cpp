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

int ModuleLibraryPanel::getNumRows() { return (int)filteredEntries.size(); }

void ModuleLibraryPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                          int width, int height,
                                          bool rowIsSelected) {
  if (rowNumber < 0 || rowNumber >= (int)filteredEntries.size()) {
    return;
  }

  const auto &entry = filteredEntries[(size_t)rowNumber];

  if (entry.isHeader) {
    // Category header — darker background, muted label, no selection highlight
    g.fillAll(ArpsLookAndFeel::getBackgroundCharcoal().darker(0.4f));
    g.setColour(juce::Colours::lightgrey.withAlpha(0.7f));
    g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    g.drawText(juce::String(entry.name).toUpperCase(), 8, 0, width - 16, height,
               juce::Justification::centredLeft, true);
    return;
  }

  if (rowIsSelected) {
    g.fillAll(ArpsLookAndFeel::getBackgroundCharcoal());
  }

  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(entry.name, 18, 0, width - 26, height,
             juce::Justification::centredLeft, true);
}

void ModuleLibraryPanel::listBoxItemClicked(int rowNumber,
                                            const juce::MouseEvent &e) {
  juce::ignoreUnused(e);
  // Deselect headers so they are never highlighted
  if (rowNumber >= 0 && rowNumber < (int)filteredEntries.size() &&
      filteredEntries[(size_t)rowNumber].isHeader) {
    moduleList.deselectAllRows();
  }
}

juce::var ModuleLibraryPanel::getDragSourceDescription(
    const juce::SparseSet<int> &selectedRows) {
  if (selectedRows.size() > 0) {
    int row = selectedRows[0];
    if (row >= 0 && row < (int)filteredEntries.size() &&
        !filteredEntries[(size_t)row].isHeader) {
      return {juce::String{filteredEntries[(size_t)row].name}};
    }
  }
  return {};
}

void ModuleLibraryPanel::listBoxItemDoubleClicked(int rowNumber,
                                                  const juce::MouseEvent &e) {
  juce::ignoreUnused(e);
  if (rowNumber >= 0 && rowNumber < (int)filteredEntries.size() &&
      !filteredEntries[(size_t)rowNumber].isHeader) {
    if (onNodeSelected) {
      onNodeSelected(filteredEntries[(size_t)rowNumber].name);
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
  // "Enter-to-add" when exactly one non-header entry matches
  std::string singleMatch;
  int nodeCount = 0;
  for (const auto &entry : filteredEntries) {
    if (!entry.isHeader) {
      singleMatch = entry.name;
      ++nodeCount;
    }
  }
  if (nodeCount == 1 && onNodeSelected) {
    onNodeSelected(singleMatch);
    searchBox.clear();
    updateFilter();
  }
}

void ModuleLibraryPanel::updateFilter() {
  auto searchText = searchBox.getText().trim().toLowerCase();

  filteredEntries.clear();

  if (searchText.isEmpty()) {
    // Categorized view
    for (const auto &[category, nodes] : NodeFactory::getNodeCategories()) {
      filteredEntries.push_back({category, true});
      for (const auto &node : nodes)
        filteredEntries.push_back({node, false});
    }
  } else {
    // Tokenised search: all tokens must match name, category, or any tag.
    // Build lookup maps once per search update.
    std::map<std::string, std::string> categoryOf;
    for (const auto &[cat, nodes] : NodeFactory::getNodeCategories())
      for (const auto &node : nodes)
        categoryOf[node] = cat;

    auto tagMap = NodeFactory::getNodeTags();

    // Split search text on whitespace into tokens.
    juce::StringArray tokens;
    tokens.addTokens(searchText, " \t", "");
    tokens.removeEmptyStrings();

    for (const auto &node : allNodeTypes) {
      bool allMatch = true;
      for (const auto &token : tokens) {
        bool found = juce::String(node).toLowerCase().contains(token);

        if (!found) {
          auto catIt = categoryOf.find(node);
          if (catIt != categoryOf.end())
            found = juce::String(catIt->second).toLowerCase().contains(token);
        }

        if (!found) {
          auto tagIt = tagMap.find(node);
          if (tagIt != tagMap.end()) {
            for (const auto &tag : tagIt->second) {
              if (juce::String(tag).toLowerCase().contains(token)) {
                found = true;
                break;
              }
            }
          }
        }

        if (!found) {
          allMatch = false;
          break;
        }
      }
      if (allMatch)
        filteredEntries.push_back({node, false});
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
