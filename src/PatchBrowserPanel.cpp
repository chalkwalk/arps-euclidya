#include "PatchBrowserPanel.h"

#include "AppSettings.h"
#include "SettingsPanel.h"

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
  bankSelector.onChange = [this] { updateList(); };

  browserOverlay.addAndMakeVisible(patchList);
  patchList.setModel(this);
  patchList.setRowHeight(36);

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
  }
  repaint();
}

void PatchBrowserPanel::updateList() {
  PatchLibrary::Bank bank = PatchLibrary::Bank::All;
  if (bankSelector.getSelectedId() == 2) {
    bank = PatchLibrary::Bank::Factory;
  }
  if (bankSelector.getSelectedId() == 3) {
    bank = PatchLibrary::Bank::User;
  }

  juce::String query = searchBox.getText();
  if (query.isEmpty()) {
    currentResults = library.getPatches(bank);
  } else {
    currentResults = library.search(query, bank);
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
  fileChooser = std::make_unique<juce::FileChooser>(
      "Save Patch", library.getUserPatchDirectory(), "*.euclidya");

  auto flags = juce::FileBrowserComponent::saveMode |
               juce::FileBrowserComponent::canSelectFiles |
               juce::FileBrowserComponent::warnAboutOverwriting;

  fileChooser->launchAsync(flags, [this](const juce::FileChooser &fc) {
    auto file = fc.getResult();
    if (file != juce::File()) {
      processor.savePatch(file.withFileExtension(".euclidya"));
      library.scan();
      updateList();
    }
  });
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
