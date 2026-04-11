#include "TuningPanel.h"

#include "../PluginProcessor.h"

TuningPanel::TuningPanel(ArpsEuclidyaProcessor &p, TuningLibrary &lib)
    : processor(p), library(lib) {
  addAndMakeVisible(nameButton);
  addAndMakeVisible(menuButton);

  nameButton.onClick = [this] { toggleBrowser(); };
  menuButton.onClick = [this] { showMenu(); };

  addChildComponent(browserOverlay);
  browserOverlay.addAndMakeVisible(searchBox);
  searchBox.setTextToShowWhenEmpty("Search tunings...", juce::Colours::grey);
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

  browserOverlay.addAndMakeVisible(tuningList);
  tuningList.setModel(this);
  tuningList.setRowHeight(36);

  repopulateCategories();
  updateList();
  refreshTuningName();
}

void TuningPanel::paint(juce::Graphics &g) {
  // Status dot: green when a non-identity tuning is active
  bool hasTuning = processor.getActiveTuningName().isNotEmpty();
  juce::Colour dotColour = hasTuning ? juce::Colour(0xff00cc66)
                                     : juce::Colour(0xff555555);

  auto topBar = getLocalBounds().reduced(4).removeFromTop(32);
  // Draw dot to the left of the name button
  juce::Rectangle<float> dotArea =
      topBar.removeFromLeft(16).toFloat().reduced(3.0f);
  g.setColour(dotColour);
  g.fillEllipse(dotArea);

  if (isBrowserOpen) {
    g.setColour(juce::Colour(0xff1a1a2e));
    g.fillRoundedRectangle(browserOverlay.getBounds().toFloat(), 4.0f);
    g.setColour(juce::Colour(0xff00cc66).withAlpha(0.4f));
    g.drawRoundedRectangle(browserOverlay.getBounds().toFloat().reduced(0.5f),
                           4.0f, 1.0f);
  }
}

void TuningPanel::resized() {
  auto bounds = getLocalBounds().reduced(4);
  auto topBar = bounds.removeFromTop(32);

  // Status dot placeholder (painted in paint())
  topBar.removeFromLeft(16);
  topBar.removeFromLeft(4);

  menuButton.setBounds(topBar.removeFromRight(30));
  topBar.removeFromRight(4);
  nameButton.setBounds(topBar);

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
    tuningList.setBounds(overlayBounds);
  } else {
    browserOverlay.setBounds(0, 0, 0, 0);
  }
}

void TuningPanel::toggleBrowser() {
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

void TuningPanel::repopulateCategories() {
  TuningLibrary::Bank bank = TuningLibrary::Bank::All;
  if (bankSelector.getSelectedId() == 2) { bank = TuningLibrary::Bank::Factory; }
  if (bankSelector.getSelectedId() == 3) { bank = TuningLibrary::Bank::User; }

  categoryFilter.clear(juce::dontSendNotification);
  categoryFilter.addItem("All Categories", 1);
  int id = 2;
  for (const auto &cat : library.getCategories(bank)) {
    categoryFilter.addItem(cat, id++);
  }
  categoryFilter.setSelectedId(1, juce::dontSendNotification);
}

void TuningPanel::updateList() {
  TuningLibrary::Bank bank = TuningLibrary::Bank::All;
  if (bankSelector.getSelectedId() == 2) { bank = TuningLibrary::Bank::Factory; }
  if (bankSelector.getSelectedId() == 3) { bank = TuningLibrary::Bank::User; }

  juce::String query = searchBox.getText();
  if (query.isEmpty()) {
    currentResults = library.getTunings(bank);
  } else {
    currentResults = library.search(query, bank);
  }

  if (categoryFilter.getSelectedId() > 1) {
    juce::String selectedCat = categoryFilter.getText();
    currentResults.erase(
        std::remove_if(currentResults.begin(), currentResults.end(),
                       [&](const TuningLibrary::TuningInfo &t) {
                         return t.category != selectedCat;
                       }),
        currentResults.end());
  }

  tuningList.updateContent();
  repaint();
}

int TuningPanel::getNumRows() { return (int)currentResults.size(); }

void TuningPanel::paintListBoxItem(int rowNumber, juce::Graphics &g, int width,
                                   int height, bool rowIsSelected) {
  if (rowNumber < 0 || rowNumber >= (int)currentResults.size()) { return; }
  juce::ignoreUnused(height);

  if (rowIsSelected) {
    g.fillAll(juce::Colour(0xff00cc66).withAlpha(0.15f));
  } else if (rowNumber == currentTuningIndex) {
    g.fillAll(juce::Colour(0xff00cc66).withAlpha(0.08f));
  }

  const auto &t = currentResults[(size_t)rowNumber];

  g.setColour(rowNumber == currentTuningIndex ? juce::Colour(0xff00cc66)
                                              : juce::Colours::white);
  g.setFont(juce::FontOptions(14.0f));
  g.drawText(t.name, 8, 2, width - 16, 16,
             juce::Justification::centredLeft, true);

  juce::String subtitle = t.category;
  if (t.bank == TuningLibrary::Bank::Factory) {
    subtitle += "  [Factory]";
  }
  g.setColour(juce::Colours::grey);
  g.setFont(juce::FontOptions(11.0f));
  g.drawText(subtitle, 8, 18, width - 16, 14,
             juce::Justification::centredLeft, true);
}

void TuningPanel::listBoxItemClicked(int row, const juce::MouseEvent & /*e*/) {
  if (row < 0 || row >= (int)currentResults.size()) { return; }
  applyTuningAtIndex(row);
}

void TuningPanel::applyTuningAtIndex(int index) {
  if (index < 0 || index >= (int)currentResults.size()) { return; }
  const auto &t = currentResults[(size_t)index];
  processor.setActiveTuning(t.sclFile, t.kbmFile);
  currentTuningIndex = index;
  refreshTuningName();
  repaint();
}

void TuningPanel::refreshTuningName() {
  juce::String name = processor.getActiveTuningName();
  nameButton.setButtonText(name.isNotEmpty() ? name : "12-TET");
  repaint();
}

void TuningPanel::showMenu() {
  juce::PopupMenu menu;
  menu.addItem(1, "Import .scl...");
  menu.addItem(2, "Clear Tuning");

  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetComponent(&menuButton),
      [this](int result) {
        if (result == 1) {
          fileChooser = std::make_unique<juce::FileChooser>(
              "Import Scala Tuning",
              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
              "*.scl");
          fileChooser->launchAsync(
              juce::FileBrowserComponent::openMode |
                  juce::FileBrowserComponent::canSelectFiles,
              [this](const juce::FileChooser &fc) {
                auto sclFile = fc.getResult();
                if (!sclFile.existsAsFile()) { return; }

                // Check for a paired .kbm with the same base name
                juce::File kbmFile = sclFile.getParentDirectory().getChildFile(
                    sclFile.getFileNameWithoutExtension() + ".kbm");
                if (!kbmFile.existsAsFile()) { kbmFile = juce::File{}; }

                // Copy to User tuning library
                juce::File userDir = TuningLibrary::getUserTuningDirectory();
                userDir.createDirectory();
                juce::File destScl =
                    userDir.getChildFile(sclFile.getFileName());
                sclFile.copyFileTo(destScl);

                juce::File destKbm{};
                if (kbmFile.existsAsFile()) {
                  destKbm = userDir.getChildFile(kbmFile.getFileName());
                  kbmFile.copyFileTo(destKbm);
                }

                library.scan();
                repopulateCategories();
                updateList();

                processor.setActiveTuning(destScl, destKbm);
                refreshTuningName();
              });
        } else if (result == 2) {
          processor.clearActiveTuning();
          currentTuningIndex = -1;
          refreshTuningName();
        }
      });
}
