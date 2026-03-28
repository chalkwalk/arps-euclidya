#include "PatchLibrary.h"
#include "AppSettings.h"

PatchLibrary::PatchLibrary() { scan(); }

void PatchLibrary::scan() {
  allPatches.clear();

  auto factoryDir = getFactoryPatchDirectory();
  auto userDir = getUserPatchDirectory();

  // Ensure directories exist
  if (!factoryDir.isDirectory()) {
    factoryDir.createDirectory();
  }
  if (!userDir.isDirectory()) {
    userDir.createDirectory();
  }

  scanDirectory(factoryDir, Bank::Factory, factoryDir.getFullPathName());
  scanDirectory(userDir, Bank::User, userDir.getFullPathName());

  DBG("PatchLibrary: scanned " + juce::String(allPatches.size()) +
      " patches (" + juce::String(getPatches(Bank::Factory).size()) +
      " factory, " + juce::String(getPatches(Bank::User).size()) + " user)");
}

void PatchLibrary::scanDirectory(const juce::File &dir, Bank bank,
                                 const juce::String &rootPath) {
  for (const auto &entry : juce::RangedDirectoryIterator(
           dir, true, "*.euclidya", juce::File::findFiles)) {
    auto info = parsePatchFile(entry.getFile(), bank, rootPath);
    allPatches.push_back(std::move(info));
  }
}

PatchLibrary::PatchInfo
PatchLibrary::parsePatchFile(const juce::File &file, Bank bank,
                             const juce::String &rootPath) const {
  PatchInfo info;
  info.file = file;
  info.bank = bank;

  // Derive category from relative path
  juce::String relativePath =
      file.getParentDirectory().getFullPathName().substring(rootPath.length());
  if (relativePath.startsWithChar('/') || relativePath.startsWithChar('\\')) {
    relativePath = relativePath.substring(1);
  }
  info.category = relativePath.isEmpty() ? "Uncategorised" : relativePath;

  // Default name from filename
  info.name = file.getFileNameWithoutExtension();

  // Try to read metadata from XML without loading the full patch
  auto xml = juce::XmlDocument::parse(file);
  if (xml != nullptr && xml->hasTagName("ArpsEuclidyaState")) {
    auto *metaXml = xml->getChildByName("Metadata");
    if (metaXml != nullptr) {
      juce::String metaName = metaXml->getStringAttribute("name");
      if (metaName.isNotEmpty()) {
        info.name = metaName;
      }
      info.author = metaXml->getStringAttribute("author");
      info.description = metaXml->getStringAttribute("description");
      info.tags = metaXml->getStringAttribute("tags");
      info.created = metaXml->getStringAttribute("created");
      info.modified = metaXml->getStringAttribute("modified");
    }
  }

  return info;
}

std::vector<PatchLibrary::PatchInfo> PatchLibrary::getPatches(Bank bank) const {
  if (bank == Bank::All) {
    return allPatches;
  }

  std::vector<PatchInfo> result;
  for (const auto &p : allPatches) {
    if (p.bank == bank) {
      result.push_back(p);
    }
  }
  return result;
}

std::vector<juce::String> PatchLibrary::getCategories(Bank bank) const {
  std::vector<juce::String> categories;
  for (const auto &p : allPatches) {
    if (bank != Bank::All && p.bank != bank)
      continue;

    bool found = false;
    for (const auto &c : categories) {
      if (c == p.category) {
        found = true;
        break;
      }
    }
    if (!found) {
      categories.push_back(p.category);
    }
  }
  return categories;
}

std::vector<PatchLibrary::PatchInfo>
PatchLibrary::search(const juce::String &query, Bank bank) const {
  std::vector<PatchInfo> results;
  juce::String lowerQuery = query.toLowerCase();

  for (const auto &p : allPatches) {
    if (bank != Bank::All && p.bank != bank)
      continue;

    if (p.name.toLowerCase().contains(lowerQuery) ||
        p.author.toLowerCase().contains(lowerQuery) ||
        p.description.toLowerCase().contains(lowerQuery) ||
        p.tags.toLowerCase().contains(lowerQuery) ||
        p.category.toLowerCase().contains(lowerQuery)) {
      results.push_back(p);
    }
  }
  return results;
}

std::vector<PatchLibrary::PatchInfo>
PatchLibrary::getByCategory(const juce::String &category, Bank bank) const {
  std::vector<PatchInfo> results;
  for (const auto &p : allPatches) {
    if (bank != Bank::All && p.bank != bank)
      continue;
    if (p.category == category) {
      results.push_back(p);
    }
  }
  return results;
}

juce::File PatchLibrary::getFactoryPatchDirectory() const {
  return AppSettings::getInstance().getResolvedPatchLibraryDir().getChildFile(
      "Factory");
}

juce::File PatchLibrary::getUserPatchDirectory() const {
  return AppSettings::getInstance().getResolvedPatchLibraryDir().getChildFile(
      "User");
}
