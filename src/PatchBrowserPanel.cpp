#include "PatchBrowserPanel.h"

#include "AppSettings.h"
#include "SettingsPanel.h"

// ---------------------------------------------------------------------------
// Modal dialog for editing patch metadata before saving
// ---------------------------------------------------------------------------
class PatchSaveDialog : public juce::Component {
 public:
  using Metadata = ArpsEuclidyaProcessor::PatchMetadata;

  PatchSaveDialog(const Metadata &meta,
                  const std::vector<juce::String> &existingCategories,
                  std::function<void(Metadata, juce::String)> onSave)
      : onSaveCallback(std::move(onSave)) {
    auto setupLabel = [this](juce::Label &lbl, const char *text) {
      addAndMakeVisible(lbl);
      lbl.setText(text, juce::dontSendNotification);
      lbl.setFont(juce::Font(juce::FontOptions(13.0f)));
      lbl.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
      lbl.setJustificationType(juce::Justification::centredRight);
    };

    auto setupEditor = [this](juce::TextEditor &ed, const juce::String &text) {
      addAndMakeVisible(ed);
      ed.setText(text, false);
      ed.setColour(juce::TextEditor::backgroundColourId,
                   juce::Colour(0xff2a2a3e));
      ed.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff404060));
      ed.setColour(juce::TextEditor::focusedOutlineColourId,
                   juce::Colour(0xff00cccc));
      ed.setColour(juce::TextEditor::textColourId, juce::Colours::white);
    };

    setupLabel(nameLabel, "Name");
    setupEditor(nameEditor, meta.name);
    nameEditor.setSelectAllWhenFocused(true);

    setupLabel(authorLabel, "Author");
    juce::String authorText = meta.author.isNotEmpty()
                                  ? meta.author
                                  : AppSettings::getInstance().getDefaultAuthor();
    setupEditor(authorEditor, authorText);

    setupLabel(descLabel, "Description");
    setupEditor(descEditor, meta.description);

    setupLabel(tagsLabel, "Tags");
    setupEditor(tagsEditor, meta.tags);
    tagsEditor.setTextToShowWhenEmpty("space-separated",
                                      juce::Colours::dimgrey);

    setupLabel(categoryLabel, "Category");
    addAndMakeVisible(categoryCombo);
    categoryCombo.setColour(juce::ComboBox::backgroundColourId,
                            juce::Colour(0xff2a2a3e));
    categoryCombo.setColour(juce::ComboBox::outlineColourId,
                            juce::Colour(0xff404060));
    categoryCombo.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    categoryCombo.setColour(juce::ComboBox::arrowColourId,
                            juce::Colours::lightgrey);

    categoryCombo.addItem("(none)", 1);
    int comboId = 2;
    int preSelectId = 1;
    for (const auto &cat : existingCategories) {
      categoryNames.push_back(cat);
      categoryCombo.addItem(cat, comboId);
      if (cat.equalsIgnoreCase(meta.category))
        preSelectId = comboId;
      ++comboId;
    }
    categoryCombo.addSeparator();
    newCategoryId = comboId;
    categoryCombo.addItem("New category...", newCategoryId);
    categoryCombo.setSelectedId(preSelectId, juce::dontSendNotification);

    categoryCombo.onChange = [this] {
      newCategoryEditor.setVisible(categoryCombo.getSelectedId() == newCategoryId);
    };

    setupEditor(newCategoryEditor, {});
    newCategoryEditor.setTextToShowWhenEmpty("Category name...",
                                             juce::Colours::dimgrey);
    newCategoryEditor.setVisible(false);

    addAndMakeVisible(saveButton);
    saveButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colour(0xff007799));
    saveButton.setColour(juce::TextButton::textColourOffId,
                         juce::Colours::white);
    saveButton.onClick = [this] { doSave(); };

    addAndMakeVisible(cancelButton);
    cancelButton.onClick = [this] {
      if (auto *dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->closeButtonPressed();
    };

    setSize(420, 250);
  }

  void resized() override {
    const int labelW = 82;
    const int gap = 6;
    const int rowH = 26;
    const int pad = 12;
    const int controlX = pad + labelW + gap;
    const int controlW = getWidth() - controlX - pad;
    int y = pad;

    auto layoutRow = [&](juce::Label &lbl, juce::Component &ctrl) {
      lbl.setBounds(pad, y, labelW, rowH);
      ctrl.setBounds(controlX, y, controlW, rowH);
      y += rowH + gap;
    };

    layoutRow(nameLabel, nameEditor);
    layoutRow(authorLabel, authorEditor);
    layoutRow(categoryLabel, categoryCombo);

    // New-category editor sits in a reserved row (hidden when not needed)
    newCategoryEditor.setBounds(controlX, y, controlW, rowH);
    if (newCategoryEditor.isVisible())
      y += rowH + gap;

    layoutRow(descLabel, descEditor);
    layoutRow(tagsLabel, tagsEditor);

    const int btnW = 80;
    const int btnH = 28;
    cancelButton.setBounds(getWidth() - pad - btnW, getHeight() - pad - btnH,
                           btnW, btnH);
    saveButton.setBounds(getWidth() - pad - btnW * 2 - gap,
                         getHeight() - pad - btnH, btnW, btnH);
  }

 private:
  void doSave() {
    Metadata meta;
    meta.name = nameEditor.getText().trim();
    meta.author = authorEditor.getText().trim();
    meta.description = descEditor.getText().trim();
    meta.tags = tagsEditor.getText().trim();

    if (meta.name.isEmpty()) {
      nameEditor.setColour(juce::TextEditor::outlineColourId,
                           juce::Colours::red);
      nameEditor.repaint();
      return;
    }

    juce::String category;
    int selId = categoryCombo.getSelectedId();
    if (selId == newCategoryId) {
      category = newCategoryEditor.getText().trim();
    } else if (selId >= 2) {
      int idx = selId - 2;
      if (idx < (int)categoryNames.size())
        category = categoryNames[(size_t)idx];
    }

    onSaveCallback(meta, category);

    if (auto *dw = findParentComponentOfClass<juce::DialogWindow>())
      dw->closeButtonPressed();
  }

  std::function<void(Metadata, juce::String)> onSaveCallback;
  std::vector<juce::String> categoryNames;
  int newCategoryId = 0;

  juce::Label nameLabel{"", "Name"};
  juce::Label authorLabel{"", "Author"};
  juce::Label descLabel{"", "Description"};
  juce::Label tagsLabel{"", "Tags"};
  juce::Label categoryLabel{"", "Category"};
  juce::TextEditor nameEditor;
  juce::TextEditor authorEditor;
  juce::TextEditor descEditor;
  juce::TextEditor tagsEditor;
  juce::TextEditor newCategoryEditor;
  juce::ComboBox categoryCombo;
  juce::TextButton saveButton{"Save"};
  juce::TextButton cancelButton{"Cancel"};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PatchSaveDialog)
};

PatchBrowserPanel::PatchBrowserPanel(ArpsEuclidyaProcessor &p,
                                     PatchLibrary &lib)
    : processor(p), library(lib) {
  addAndMakeVisible(prevButton);
  addAndMakeVisible(nextButton);
  addAndMakeVisible(currentPatchButton);
  addAndMakeVisible(saveButton);
  addAndMakeVisible(menuButton);

  prevButton.onClick = [this] { prevPatch(); };
  nextButton.onClick = [this] { nextPatch(); };
  currentPatchButton.onClick = [this] { toggleBrowser(); };
  saveButton.onClick = [this] { savePatch(); };
  menuButton.onClick = [this] { showMenu(); };

  addChildComponent(browserOverlay);
  browserOverlay.addAndMakeVisible(searchBox);
  searchBox.setTextToShowWhenEmpty("Search patches...", juce::Colours::grey);
  searchBox.onTextChange = [this] { updateList(); };

  browserOverlay.addAndMakeVisible(bankSelector);
  bankSelector.addItem("All Banks", 1);
  bankSelector.addItem("Factory", 2);
  bankSelector.addItem("User", 3);
  bankSelector.setSelectedId(1, juce::dontSendNotification);
  bankSelector.onChange = [this] {
    repopulateCategories();
    updateList();
  };

  browserOverlay.addAndMakeVisible(categoryFilter);
  categoryFilter.onChange = [this] { updateList(); };

  browserOverlay.addAndMakeVisible(patchList);
  patchList.setModel(this);
  patchList.setRowHeight(36);

  repopulateCategories();
  updateList();
}

void PatchBrowserPanel::paint(juce::Graphics &g) {
  g.setColour(juce::Colours::black.withAlpha(0.2f));
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);

  if (isBrowserOpen) {
    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillRoundedRectangle(browserOverlay.getBounds().toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff00ffff).withAlpha(0.3f));
    g.drawRoundedRectangle(browserOverlay.getBounds().toFloat().reduced(0.5f),
                           4.0f, 1.0f);
  }
}

void PatchBrowserPanel::resized() {
  auto bounds = getLocalBounds().reduced(4);
  auto topBar = bounds.removeFromTop(32);

  prevButton.setBounds(topBar.removeFromLeft(30));
  topBar.removeFromLeft(4);
  nextButton.setBounds(topBar.removeFromLeft(30));
  topBar.removeFromLeft(8);

  menuButton.setBounds(topBar.removeFromRight(30));
  topBar.removeFromRight(4);
  saveButton.setBounds(topBar.removeFromRight(60));
  topBar.removeFromRight(8);

  currentPatchButton.setBounds(topBar);

  if (isBrowserOpen) {
    bounds.removeFromTop(4);
    browserOverlay.setBounds(bounds);

    auto overlayBounds = browserOverlay.getLocalBounds().reduced(4);
    auto searchRow = overlayBounds.removeFromTop(24);
    bankSelector.setBounds(searchRow.removeFromLeft(100));
    searchRow.removeFromLeft(4);
    searchBox.setBounds(searchRow);

    overlayBounds.removeFromTop(4);
    categoryFilter.setBounds(overlayBounds.removeFromTop(24));

    overlayBounds.removeFromTop(4);
    patchList.setBounds(overlayBounds);
  } else {
    browserOverlay.setBounds(0, 0, 0, 0);
  }
}

void PatchBrowserPanel::toggleBrowser() {
  isBrowserOpen = !isBrowserOpen;
  browserOverlay.setVisible(isBrowserOpen);

  if (isBrowserOpen) {
    updateList();
    setSize(getWidth(), 400);
  } else {
    setSize(getWidth(), 40);
  }

  if (auto *parent = getParentComponent()) {
    parent->resized();
    parent->repaint();
  }
  repaint();
}

void PatchBrowserPanel::repopulateCategories() {
  PatchLibrary::Bank bank = PatchLibrary::Bank::All;
  if (bankSelector.getSelectedId() == 2) bank = PatchLibrary::Bank::Factory;
  if (bankSelector.getSelectedId() == 3) bank = PatchLibrary::Bank::User;

  categoryFilter.clear(juce::dontSendNotification);
  categoryFilter.addItem("All Categories", 1);
  int id = 2;
  for (const auto &cat : library.getCategories(bank)) {
    categoryFilter.addItem(cat, id++);
  }
  categoryFilter.setSelectedId(1, juce::dontSendNotification);
}

void PatchBrowserPanel::updateList() {
  PatchLibrary::Bank bank = PatchLibrary::Bank::All;
  if (bankSelector.getSelectedId() == 2) bank = PatchLibrary::Bank::Factory;
  if (bankSelector.getSelectedId() == 3) bank = PatchLibrary::Bank::User;

  juce::String query = searchBox.getText();
  if (query.isEmpty()) {
    currentResults = library.getPatches(bank);
  } else {
    currentResults = library.search(query, bank);
  }

  // Apply category filter (ID 1 = "All Categories" = no filter)
  if (categoryFilter.getSelectedId() > 1) {
    juce::String selectedCat = categoryFilter.getText();
    currentResults.erase(
        std::remove_if(currentResults.begin(), currentResults.end(),
                       [&](const PatchLibrary::PatchInfo &p) {
                         return p.category != selectedCat;
                       }),
        currentResults.end());
  }

  patchList.updateContent();
  repaint();
}

int PatchBrowserPanel::getNumRows() { return (int)currentResults.size(); }

void PatchBrowserPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                         int width, int height,
                                         bool rowIsSelected) {
  if (rowNumber < 0 || rowNumber >= (int)currentResults.size()) {
    return;
  }
  juce::ignoreUnused(height);

  if (rowIsSelected) {
    g.fillAll(juce::Colour(0xff00ffff).withAlpha(0.15f));
  } else if (rowNumber == currentPatchIndex) {
    g.fillAll(juce::Colour(0xff00ffff).withAlpha(0.08f));
  }

  auto &patch = currentResults[(size_t)rowNumber];

  g.setColour(rowNumber == currentPatchIndex ? juce::Colour(0xff00ffff)
                                             : juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(patch.name, 8, 2, width - 16, 16,
             juce::Justification::centredLeft);

  g.setColour(juce::Colours::grey);
  g.setFont(11.0f);
  juce::String subtitle;
  if (patch.author.isNotEmpty()) {
    subtitle = patch.author + "  ·  ";
  }
  subtitle += patch.category;
  if (patch.bank == PatchLibrary::Bank::Factory) {
    subtitle += "  [Factory]";
  }
  g.drawText(subtitle, 8, 18, width - 16, 14, juce::Justification::centredLeft);
}

void PatchBrowserPanel::listBoxItemClicked(
    int row, const juce::MouseEvent & /*unused*/) {
  loadPatchAtIndex(row);
  toggleBrowser();
}

void PatchBrowserPanel::loadPatchAtIndex(int index) {
  if (index >= 0 && index < (int)currentResults.size()) {
    currentPatchIndex = index;
    auto &patch = currentResults[(size_t)index];
    processor.loadPatch(patch.file);
    // Back-fill category from filesystem for patches that pre-date this field.
    // Exclude the sentinel "Uncategorised" value — that means no real category.
    if (processor.currentPatchMetadata.category.isEmpty() &&
        patch.category != "Uncategorised") {
      processor.currentPatchMetadata.category = patch.category;
    }
    currentPatchButton.setButtonText(patch.name);
  }
}

void PatchBrowserPanel::prevPatch() {
  if (currentResults.empty()) {
    updateList();
    if (currentResults.empty()) {
      return;
    }
  }
  int newIndex = currentPatchIndex - 1;
  if (newIndex < 0) {
    newIndex = (int)currentResults.size() - 1;
  }
  loadPatchAtIndex(newIndex);
}

void PatchBrowserPanel::nextPatch() {
  if (currentResults.empty()) {
    updateList();
    if (currentResults.empty()) {
      return;
    }
  }
  int newIndex = currentPatchIndex + 1;
  if (newIndex >= (int)currentResults.size()) {
    newIndex = 0;
  }
  loadPatchAtIndex(newIndex);
}

void PatchBrowserPanel::refreshPatchName() {
  currentPatchButton.setButtonText(processor.currentPatchMetadata.name.isEmpty()
                                       ? "Init"
                                       : processor.currentPatchMetadata.name);
}

void PatchBrowserPanel::savePatch() {
  auto categories = library.getCategories(PatchLibrary::Bank::User);

  auto *dialog = new PatchSaveDialog(
      processor.currentPatchMetadata, categories,
      [this](ArpsEuclidyaProcessor::PatchMetadata meta, const juce::String &category) {
        juce::String safeName =
            meta.name.replaceCharacters("/\\:*?\"<>|", "_________").trim();
        if (safeName.isEmpty()) {
          safeName = "Patch";
        }

        juce::File dir = PatchLibrary::getUserPatchDirectory();
        if (category.isNotEmpty()) {
          dir = dir.getChildFile(category);
        }
        dir.createDirectory();

        juce::File file = dir.getChildFile(safeName + ".euclidya");

        // Preserve the original creation timestamp on re-saves
        meta.created = processor.currentPatchMetadata.created;
        meta.category = category;
        processor.currentPatchMetadata = meta;

        if (processor.savePatch(file)) {
          currentPatchButton.setButtonText(meta.name);
          library.scan();
          repopulateCategories();
          updateList();
        }
      });

  juce::DialogWindow::LaunchOptions opts;
  opts.dialogTitle = "Save Patch";
  opts.dialogBackgroundColour = juce::Colour(0xff1a1a2e);
  opts.content.setOwned(dialog);
  opts.escapeKeyTriggersCloseButton = true;
  opts.useNativeTitleBar = true;
  opts.resizable = false;
  opts.launchAsync();
}

void PatchBrowserPanel::showMenu() {
  juce::PopupMenu menu;
  menu.addItem(1, "Import Patch...");
  menu.addItem(2, "Refresh Library");
  menu.addSeparator();
  menu.addItem(3, "Reveal Library Folder...");
  menu.addSeparator();
  menu.addItem(10, "Settings...");

  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetComponent(menuButton),
      [this](int result) {
        if (result == 1) {
          fileChooser = std::make_unique<juce::FileChooser>(
              "Import Patch",
              juce::File::getSpecialLocation(
                  juce::File::userDocumentsDirectory),
              "*.euclidya");
          fileChooser->launchAsync(
              juce::FileBrowserComponent::openMode |
                  juce::FileBrowserComponent::canSelectFiles,
              [this](const juce::FileChooser &fc) {
                if (fc.getResult() != juce::File()) {
                  processor.loadPatch(fc.getResult());
                  currentPatchButton.setButtonText(
                      fc.getResult().getFileNameWithoutExtension());
                }
              });
        } else if (result == 2) {
          library.scan();
          repopulateCategories();
          updateList();
        } else if (result == 3) {
          library.getFactoryPatchDirectory().revealToUser();
        } else if (result == 10) {
          auto *settingsPanel = new SettingsPanel(processor);
          settingsPanel->setSize(450, 500);
          juce::DialogWindow::LaunchOptions opts;
          opts.dialogTitle = "Settings";
          opts.dialogBackgroundColour = juce::Colour(0xff222222);
          opts.content.setOwned(settingsPanel);
          opts.escapeKeyTriggersCloseButton = true;
          opts.useNativeTitleBar = true;
          opts.resizable = false;
          opts.launchAsync();
        }
      });
}
