#include "AppSettings.h"

AppSettings::AppSettings() {
  juce::PropertiesFile::Options options;
  options.applicationName = "Arps Euclidya";
  options.folderName = "ChalkWalkMusic";
  options.filenameSuffix = ".settings";
  options.osxLibrarySubFolder = "Application Support";
  options.commonToAllUsers = false;
  options.ignoreCaseOfKeyNames = false;
  options.storageFormat = juce::PropertiesFile::storeAsXML;

  propertiesFile = std::make_unique<juce::PropertiesFile>(options);
}

juce::String AppSettings::getDefaultAuthor() {
  return propertiesFile->getValue("defaultAuthor", "");
}

void AppSettings::setDefaultAuthor(const juce::String &author) {
  propertiesFile->setValue("defaultAuthor", author);
  propertiesFile->saveIfNeeded();
}

juce::String AppSettings::getPatchLibraryRoot() {
  return propertiesFile->getValue("patchLibraryRoot", "");
}

void AppSettings::setPatchLibraryRoot(const juce::String &path) {
  propertiesFile->setValue("patchLibraryRoot", path);
  propertiesFile->saveIfNeeded();
}

juce::File AppSettings::getResolvedPatchLibraryDir() {
  juce::String customRoot = getPatchLibraryRoot();
  if (customRoot.isNotEmpty() && juce::File(customRoot).isDirectory()) {
    return juce::File(customRoot).getChildFile("Patches");
  }

  // Default fallback: ~/Library/Application Support/ChalkWalkMusic/Arps
  // Euclidya/Patches
  juce::File appDataDir =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);

#if JUCE_MAC || JUCE_LINUX
  juce::File baseDir =
      appDataDir.getChildFile("ChalkWalkMusic").getChildFile("Arps Euclidya");
#elif JUCE_WINDOWS
  juce::File baseDir =
      appDataDir.getChildFile("ChalkWalkMusic").getChildFile("Arps Euclidya");
#else
  juce::File baseDir =
      appDataDir.getChildFile("ChalkWalkMusic").getChildFile("Arps Euclidya");
#endif

  return baseDir.getChildFile("Patches");
}

bool AppSettings::getIgnoreMpeMasterPressure() {
  return propertiesFile->getBoolValue("ignoreMpeMasterPressure", false);
}

void AppSettings::setIgnoreMpeMasterPressure(bool shouldIgnore) {
  propertiesFile->setValue("ignoreMpeMasterPressure", shouldIgnore);
  propertiesFile->saveIfNeeded();
}

bool AppSettings::getMpeEnabled() {
  return propertiesFile->getBoolValue("mpeEnabled", false);
}

void AppSettings::setMpeEnabled(bool enabled) {
  propertiesFile->setValue("mpeEnabled", enabled);
  propertiesFile->saveIfNeeded();
}
