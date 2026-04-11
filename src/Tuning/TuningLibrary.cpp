#include "TuningLibrary.h"

#include "../AppSettings.h"

// Forward-declare the getNamedResource function from the FactoryTuningData
// binary target (built with NAMESPACE FactoryTunings in CMakeLists.txt).
namespace FactoryTunings {
extern const char* getNamedResource(const char* name, int& size);
}

// ─────────────────────────────────────────────────────────────────────────────
// Factory tuning manifest: { resourceName, relativeDestPath }
// ─────────────────────────────────────────────────────────────────────────────
struct FactoryTuningEntry {
  const char* resourceName;
  const char* relPath;
};

static const std::array<FactoryTuningEntry, 7> kFactoryTunings = {{
    {"_19tet_scl", "19-tet.scl"},
    {"_22tet_scl", "22-tet.scl"},
    {"_31tet_scl", "31-tet.scl"},
    {"_53tet_scl", "53-tet.scl"},
    {"pythagorean_scl", "pythagorean.scl"},
    {"meantone_qc_scl", "meantone_qc.scl"},
    {"ji_5limit_scl", "ji_5limit.scl"},
}};

// ─────────────────────────────────────────────────────────────────────────────

TuningLibrary::TuningLibrary() { scan(); }

bool TuningLibrary::installFactoryTunings() {
  auto factoryDir = getFactoryTuningDirectory();
  if (!factoryDir.isDirectory()) {
    factoryDir.createDirectory();
  }

  auto sentinelFile = factoryDir.getChildFile(".version");
  if (sentinelFile.existsAsFile() &&
      sentinelFile.loadFileAsString().trim() == kFactoryTuningVersion) {
    return false;
  }

  DBG("TuningLibrary: installing/upgrading factory tunings to v" +
      juce::String(kFactoryTuningVersion));

  int installed = 0;
  for (const auto& entry : kFactoryTunings) {
    int size = 0;
    const char* data =
        FactoryTunings::getNamedResource(entry.resourceName, size);
    if (data == nullptr || size == 0) {
      DBG("TuningLibrary: WARNING – resource not found: " +
          juce::String(entry.resourceName));
      continue;
    }

    auto dest = factoryDir.getChildFile(entry.relPath);
    dest.getParentDirectory().createDirectory();

    if (dest.replaceWithData(data, (size_t)size)) {
      ++installed;
    } else {
      DBG("TuningLibrary: WARNING – failed to write " +
          dest.getFullPathName());
    }
  }

  sentinelFile.replaceWithText(kFactoryTuningVersion);
  DBG("TuningLibrary: installed " + juce::String(installed) +
      " factory tuning(s).");
  return installed > 0;
}

void TuningLibrary::scan() {
  allTunings.clear();

  (void)installFactoryTunings();

  auto factoryDir = getFactoryTuningDirectory();
  auto userDir = getUserTuningDirectory();

  if (!factoryDir.isDirectory()) {
    factoryDir.createDirectory();
  }
  if (!userDir.isDirectory()) {
    userDir.createDirectory();
  }

  scanDirectory(factoryDir, Bank::Factory, factoryDir.getFullPathName());
  scanDirectory(userDir, Bank::User, userDir.getFullPathName());

  std::sort(allTunings.begin(), allTunings.end(),
            [](const TuningInfo& a, const TuningInfo& b) {
              return a.name.compareNatural(b.name) < 0;
            });

  DBG("TuningLibrary: scanned " + juce::String(allTunings.size()) +
      " tunings (" +
      juce::String(getTunings(Bank::Factory).size()) + " factory, " +
      juce::String(getTunings(Bank::User).size()) + " user)");
}

void TuningLibrary::scanDirectory(const juce::File& dir, Bank bank,
                                  const juce::String& rootPath) {
  for (const auto& entry : juce::RangedDirectoryIterator(
           dir, true, "*.scl", juce::File::findFiles)) {
    auto info = parseTuningFile(entry.getFile(), bank, rootPath);
    allTunings.push_back(std::move(info));
  }
}

TuningLibrary::TuningInfo TuningLibrary::parseTuningFile(
    const juce::File& sclFile, Bank bank, const juce::String& rootPath) {
  TuningInfo info;
  info.sclFile = sclFile;
  info.bank = bank;

  // Derive category from subdirectory relative to bank root
  juce::String relativePath =
      sclFile.getParentDirectory().getFullPathName().substring(
          rootPath.length());
  if (relativePath.startsWithChar('/') || relativePath.startsWithChar('\\')) {
    relativePath = relativePath.substring(1);
  }
  info.category = relativePath.isEmpty() ? "Uncategorised" : relativePath;

  // Default name from filename
  info.name = sclFile.getFileNameWithoutExtension();

  // Check for a paired .kbm file (same base name, same directory)
  juce::File kbmCandidate =
      sclFile.getParentDirectory().getChildFile(info.name + ".kbm");
  if (kbmCandidate.existsAsFile()) {
    info.kbmFile = kbmCandidate;
  }

  // Read description from first non-comment, non-empty line of .scl
  juce::String sclText = sclFile.loadFileAsString();
  juce::StringArray lines;
  lines.addLines(sclText);
  bool skippedDescription = false;
  for (const auto& rawLine : lines) {
    juce::String line = rawLine.trim();
    if (line.startsWithChar('!') || line.isEmpty()) { continue; }
    if (!skippedDescription) {
      // First non-comment non-empty line is the description / name
      if (line.isNotEmpty()) {
        info.name = line;
      }
      skippedDescription = true;
    } else {
      break;
    }
  }

  return info;
}

std::vector<TuningLibrary::TuningInfo> TuningLibrary::getTunings(
    Bank bank) const {
  if (bank == Bank::All) { return allTunings; }
  std::vector<TuningInfo> result;
  for (const auto& t : allTunings) {
    if (t.bank == bank) { result.push_back(t); }
  }
  return result;
}

std::vector<TuningLibrary::TuningInfo> TuningLibrary::search(
    const juce::String& query, Bank bank) const {
  std::vector<TuningInfo> results;
  juce::String lq = query.toLowerCase();
  for (const auto& t : allTunings) {
    if (bank != Bank::All && t.bank != bank) { continue; }
    if (t.name.toLowerCase().contains(lq) ||
        t.description.toLowerCase().contains(lq) ||
        t.category.toLowerCase().contains(lq)) {
      results.push_back(t);
    }
  }
  return results;
}

std::vector<juce::String> TuningLibrary::getCategories(Bank bank) const {
  std::vector<juce::String> categories;
  for (const auto& t : allTunings) {
    if (bank != Bank::All && t.bank != bank) { continue; }
    bool found = false;
    for (const auto& c : categories) {
      if (c == t.category) {
        found = true;
        break;
      }
    }
    if (!found) { categories.push_back(t.category); }
  }
  std::sort(categories.begin(), categories.end(),
            [](const juce::String& a, const juce::String& b) {
              return a.compareNatural(b) < 0;
            });
  return categories;
}

juce::File TuningLibrary::getFactoryTuningDirectory() {
  return AppSettings::getInstance()
      .getResolvedTuningLibraryDir()
      .getChildFile("Factory");
}

juce::File TuningLibrary::getUserTuningDirectory() {
  return AppSettings::getInstance()
      .getResolvedTuningLibraryDir()
      .getChildFile("User");
}
