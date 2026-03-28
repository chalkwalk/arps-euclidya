#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <memory>

class AppSettings {
 public:
  static AppSettings &getInstance() {
    static AppSettings instance;
    return instance;
  }

  // Author name to auto-populate metadata
  juce::String getDefaultAuthor();
  void setDefaultAuthor(const juce::String &author);

  // Custom library root (empty = use default app-data path)
  juce::String getPatchLibraryRoot();
  void setPatchLibraryRoot(const juce::String &path);

  // Gets the actual resolved path to the user/factory patch library
  juce::File getResolvedPatchLibraryDir();

  juce::PropertiesFile *getPropertiesFile() { return propertiesFile.get(); }

 private:
  AppSettings();
  ~AppSettings() = default;

  std::unique_ptr<juce::PropertiesFile> propertiesFile;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppSettings)
};
