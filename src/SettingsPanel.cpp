#include "SettingsPanel.h"

SettingsPanel::SettingsPanel() {
  addAndMakeVisible(titleLabel);
  titleLabel.setFont(juce::Font(18.0f, juce::Font::bold));
  titleLabel.setJustificationType(juce::Justification::centred);

  addAndMakeVisible(authorLabel);
  addAndMakeVisible(authorEditor);
  authorEditor.setText(AppSettings::getInstance().getDefaultAuthor(), false);
  authorEditor.onTextChange = [this]() {
    AppSettings::getInstance().setDefaultAuthor(authorEditor.getText());
  };

  addAndMakeVisible(libraryRootLabel);
  addAndMakeVisible(libraryRootEditor);
  libraryRootEditor.setText(AppSettings::getInstance().getPatchLibraryRoot(),
                            false);
  libraryRootEditor.onTextChange = [this]() {
    AppSettings::getInstance().setPatchLibraryRoot(libraryRootEditor.getText());
  };

  addAndMakeVisible(browseLibraryButton);
  browseLibraryButton.onClick = [this]() {
    juce::File currentFolder =
        juce::File(AppSettings::getInstance().getPatchLibraryRoot());
    if (!currentFolder.isDirectory()) {
      currentFolder =
          juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    }

    fileChooser = std::make_unique<juce::FileChooser>(
        "Select Patch Library Root", currentFolder);

    auto flags = juce::FileBrowserComponent::canSelectDirectories |
                 juce::FileBrowserComponent::openMode;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser &fc) {
      if (fc.getResult().isDirectory()) {
        juce::String path = fc.getResult().getFullPathName();
        libraryRootEditor.setText(path);
        AppSettings::getInstance().setPatchLibraryRoot(path);
      }
    });
  };

  setSize(400, 200);
}

void SettingsPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
}

void SettingsPanel::resized() {
  auto bounds = getLocalBounds().reduced(15);

  titleLabel.setBounds(bounds.removeFromTop(30));
  bounds.removeFromTop(10);

  auto authorRow = bounds.removeFromTop(24);
  authorLabel.setBounds(authorRow.removeFromLeft(120));
  authorEditor.setBounds(authorRow);

  bounds.removeFromTop(10);

  auto rootRow = bounds.removeFromTop(24);
  libraryRootLabel.setBounds(rootRow.removeFromLeft(120));
  browseLibraryButton.setBounds(rootRow.removeFromRight(30));
  rootRow.removeFromRight(5);
  libraryRootEditor.setBounds(rootRow);
}
