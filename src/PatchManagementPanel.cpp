#include "PatchManagementPanel.h"

PatchManagementPanel::PatchManagementPanel(ArpsEuclidyaProcessor &p)
    : processor(p) {
  addAndMakeVisible(saveButton);
  saveButton.onClick = [this] { savePatch(); };

  addAndMakeVisible(loadButton);
  loadButton.onClick = [this] { loadPatch(); };
}

void PatchManagementPanel::paint(juce::Graphics &g) {
  // Semi-transparent background for the panel area
  g.setColour(juce::Colours::black.withAlpha(0.2f));
  g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
}

void PatchManagementPanel::resized() {
  auto bounds = getLocalBounds().reduced(4);
  int buttonWidth = bounds.getWidth() / 2 - 2;

  saveButton.setBounds(bounds.removeFromLeft(buttonWidth));
  bounds.removeFromLeft(4);
  loadButton.setBounds(bounds.removeFromLeft(buttonWidth));
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
