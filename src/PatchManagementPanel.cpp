#include "PatchManagementPanel.h"
#include "SettingsPanel.h"

PatchManagementPanel::PatchManagementPanel(ArpsEuclidyaProcessor &p)
    : processor(p) {
  addAndMakeVisible(saveButton);
  saveButton.onClick = [this] { savePatch(); };

  addAndMakeVisible(loadButton);
  loadButton.onClick = [this] { loadPatch(); };

  addAndMakeVisible(settingsButton);
  settingsButton.onClick = [this] { showSettings(); };
}

void PatchManagementPanel::paint(juce::Graphics &g) {
  // Semi-transparent background for the panel area
  g.setColour(juce::Colours::black.withAlpha(0.2f));
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
}

void PatchManagementPanel::resized() {
  auto bounds = getLocalBounds().reduced(4);
  int settingsWidth = 30;
  int buttonWidth = (bounds.getWidth() - settingsWidth - 8) / 2;

  saveButton.setBounds(bounds.removeFromLeft(buttonWidth));
  bounds.removeFromLeft(4);
  loadButton.setBounds(bounds.removeFromLeft(buttonWidth));
  bounds.removeFromLeft(4);
  settingsButton.setBounds(bounds.removeFromLeft(settingsWidth));
}

void PatchManagementPanel::savePatch() {
  fileChooser = std::make_unique<juce::FileChooser>(
      "Save Patch",
      juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
      "*.euclidya");

  auto flags = juce::FileBrowserComponent::saveMode |
               juce::FileBrowserComponent::canSelectFiles |
               juce::FileBrowserComponent::warnAboutOverwriting;

  fileChooser->launchAsync(flags, [this](const juce::FileChooser &fc) {
    auto file = fc.getResult();
    if (file != juce::File()) {
      processor.savePatch(file.withFileExtension(".euclidya"));
    }
  });
}

void PatchManagementPanel::loadPatch() {
  fileChooser = std::make_unique<juce::FileChooser>(
      "Load Patch",
      juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
      "*.euclidya");

  auto flags = juce::FileBrowserComponent::openMode |
               juce::FileBrowserComponent::canSelectFiles;

  fileChooser->launchAsync(flags, [this](const juce::FileChooser &fc) {
    auto file = fc.getResult();
    if (file != juce::File()) {
      processor.loadPatch(file);
    }
  });
}

void PatchManagementPanel::showSettings() {
  juce::PopupMenu menu;
  menu.addItem(1, "Settings...");

  menu.showMenuAsync(
      juce::PopupMenu::Options().withTargetComponent(settingsButton),
      [this](int result) {
        if (result == 1) {
          auto panel = std::make_unique<SettingsPanel>();
          juce::CallOutBox::launchAsynchronously(
              std::move(panel), settingsButton.getScreenBounds(), nullptr);
        }
      });
}
