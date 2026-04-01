#include "PatchLibrary.h"

#include "AppSettings.h"

// Forward-declare only the getNamedResource function from the FactoryPatchData
// binary target (built with NAMESPACE FactoryPatches in CMakeLists.txt).
// We cannot forward-declare `const int Init_euclidyaSize` because JUCE defines
// it as an inline constexpr in the generated header — not a separately-linked
// symbol. The getNamedResource function IS a proper out-of-line definition.
namespace FactoryPatches {
extern const char *getNamedResource(const char *name, int &size);
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory patch manifest: { resourceName, relativeDestPath }
// relativeDestPath is relative to getFactoryPatchDirectory()
// Add a new row here (and to patches/Factory/ + CMakeLists) for each new patch.
// ─────────────────────────────────────────────────────────────────────────────
struct FactoryPatchEntry {
  const char *resourceName;  // Key for FactoryPatches::getNamedResource
  const char *relPath;       // Relative path within Factory/
};

static const FactoryPatchEntry kFactoryPatches[] = {
    {"Init_euclidya", "Init.euclidya"},
    {"Tutorial_0_euclidya", "Tutorial_0.euclidya"},
    {"Tutorial_1_euclidya", "Tutorial_1.euclidya"},
    {"Tutorial_2_euclidya", "Tutorial_2.euclidya"},
    {"Tutorial_3_euclidya", "Tutorial_3.euclidya"},
    {"Tutorial_4_euclidya", "Tutorial_4.euclidya"},
    {"Tutorial_5_euclidya", "Tutorial_5.euclidya"},
    {"Tutorial_6_euclidya", "Tutorial_6.euclidya"},
};

// ─────────────────────────────────────────────────────────────────────────────

PatchLibrary::PatchLibrary() { scan(); }

bool PatchLibrary::installFactoryPatches() const {
  auto factoryDir = getFactoryPatchDirectory();
  if (!factoryDir.isDirectory()) {
    factoryDir.createDirectory();
  }

  // Check version sentinel — skip if already at current version
  auto sentinelFile = factoryDir.getChildFile(".version");
  if (sentinelFile.existsAsFile() &&
      sentinelFile.loadFileAsString().trim() == kFactoryPatchVersion) {
    return false;
  }

  DBG("PatchLibrary: installing/upgrading factory patches to v" +
      juce::String(kFactoryPatchVersion));

  int installed = 0;
  for (const auto &entry : kFactoryPatches) {
    int size = 0;
    const char *data =
        FactoryPatches::getNamedResource(entry.resourceName, size);
    if (data == nullptr || size == 0) {
      DBG("PatchLibrary: WARNING – resource not found: " +
          juce::String(entry.resourceName));
      continue;
    }

    auto dest = factoryDir.getChildFile(entry.relPath);
    dest.getParentDirectory().createDirectory();

    if (dest.replaceWithData(data, (size_t)size)) {
      ++installed;
    } else {
      DBG("PatchLibrary: WARNING – failed to write " + dest.getFullPathName());
    }
  }

  sentinelFile.replaceWithText(kFactoryPatchVersion);
  DBG("PatchLibrary: installed " + juce::String(installed) +
      " factory patch(es).");
  return installed > 0;
}

void PatchLibrary::scan() {
  allPatches.clear();

  installFactoryPatches();

  auto factoryDir = getFactoryPatchDirectory();
  auto userDir = getUserPatchDirectory();

  if (!factoryDir.isDirectory()) {
    factoryDir.createDirectory();
  }
  if (!userDir.isDirectory()) {
    userDir.createDirectory();
  }

  scanDirectory(factoryDir, Bank::Factory, factoryDir.getFullPathName());
  scanDirectory(userDir, Bank::User, userDir.getFullPathName());

  // Alphanumeric sort (Natural order, case-insensitive)
  std::sort(allPatches.begin(), allPatches.end(),
            [](const PatchInfo &a, const PatchInfo &b) {
              return a.name.compareNatural(b.name) < 0;
            });

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

PatchLibrary::PatchInfo PatchLibrary::parsePatchFile(
    const juce::File &file, Bank bank, const juce::String &rootPath) {
  PatchInfo info;
  info.file = file;
  info.bank = bank;

  // Derive category from subdirectory relative to bank root
  juce::String relativePath =
      file.getParentDirectory().getFullPathName().substring(rootPath.length());
  if (relativePath.startsWithChar('/') || relativePath.startsWithChar('\\')) {
    relativePath = relativePath.substring(1);
  }
  info.category = relativePath.isEmpty() ? "Uncategorised" : relativePath;

  // Default name from filename
  info.name = file.getFileNameWithoutExtension();

  // Read metadata-only from XML
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
    if (bank != Bank::All && p.bank != bank) {
      continue;
    }
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

  // Alphanumeric sort for categories
  std::sort(categories.begin(), categories.end(),
            [](const juce::String &a, const juce::String &b) {
              return a.compareNatural(b) < 0;
            });

  return categories;
}

std::vector<PatchLibrary::PatchInfo> PatchLibrary::search(
    const juce::String &query, Bank bank) const {
  std::vector<PatchInfo> results;
  juce::String lq = query.toLowerCase();
  for (const auto &p : allPatches) {
    if (bank != Bank::All && p.bank != bank) {
      continue;
    }
    if (p.name.toLowerCase().contains(lq) ||
        p.author.toLowerCase().contains(lq) ||
        p.description.toLowerCase().contains(lq) ||
        p.tags.toLowerCase().contains(lq) ||
        p.category.toLowerCase().contains(lq)) {
      results.push_back(p);
    }
  }
  return results;
}

std::vector<PatchLibrary::PatchInfo> PatchLibrary::getByCategory(
    const juce::String &category, Bank bank) const {
  std::vector<PatchInfo> results;
  for (const auto &p : allPatches) {
    if (bank != Bank::All && p.bank != bank) {
      continue;
    }
    if (p.category == category) {
      results.push_back(p);
    }
  }
  return results;
}

juce::File PatchLibrary::getFactoryPatchDirectory() {
  return AppSettings::getInstance().getResolvedPatchLibraryDir().getChildFile(
      "Factory");
}

juce::File PatchLibrary::getUserPatchDirectory() {
  return AppSettings::getInstance().getResolvedPatchLibraryDir().getChildFile(
      "User");
}
