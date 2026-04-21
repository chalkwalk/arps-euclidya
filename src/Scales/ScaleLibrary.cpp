#include "ScaleLibrary.h"

#include <juce_core/juce_core.h>

// Forward-declare the getNamedResource function from the FactoryScaleData
// binary target (built with NAMESPACE FactoryScales in CMakeLists.txt).
namespace FactoryScales {
const char *getNamedResource(const char *name, int &size);
}  // namespace FactoryScales

// Mangled resource names for the 17 factory 12-TET scales (filename + "_json")
static const char *const kFactoryScaleResources[] = {
    "Lydian_json",         "Ionian_json",         "Mixolydian_json",
    "Dorian_json",         "Aeolian_json",         "Phrygian_json",
    "Locrian_json",        "Major_json",            "Minor_json",
    "HarmonicMinor_json",  "MelodicMinor_json",    "MajorPentatonic_json",
    "MinorPentatonic_json","WholeTone_json",        "Diminished_json",
    "Blues_json",          "Chromatic_json",
};

// ─────────────────────────────────────────────────────────────────────────────

Scale ScaleLibrary::parseScaleJSON(const juce::String &json) {
  Scale scale;
  auto parsed = juce::JSON::parse(json);
  if (parsed.isVoid()) {
    return scale;
  }
  auto *obj = parsed.getDynamicObject();
  if (obj == nullptr) {
    return scale;
  }
  scale.name = obj->getProperty("name").toString();
  scale.tuningName = obj->getProperty("tuningName").toString();
  if (auto *arr = obj->getProperty("stepMask").getArray()) {
    for (const auto &v : *arr) {
      scale.stepMask.push_back((bool)v);
    }
  }
  return scale;
}

std::vector<Scale> ScaleLibrary::loadFactoryScales() {
  std::vector<Scale> result;
  constexpr int n = static_cast<int>(sizeof(kFactoryScaleResources) /
                                     sizeof(kFactoryScaleResources[0]));
  for (int i = 0; i < n; ++i) {
    int size = 0;
    const char *data = FactoryScales::getNamedResource(kFactoryScaleResources[i], size);
    if (data == nullptr || size <= 0) {
      continue;
    }
    auto json = juce::String::fromUTF8(data, size);
    auto scale = parseScaleJSON(json);
    if (scale.name.isNotEmpty() && !scale.stepMask.empty()) {
      result.push_back(std::move(scale));
    }
  }
  // Sort factory 12-TET scales in canonical order (matches old SCALES array)
  static const juce::StringArray kOrder = {
      "Lydian", "Ionian", "Mixolydian", "Dorian", "Aeolian", "Phrygian",
      "Locrian", "Major", "Minor", "Harmonic Minor", "Melodic Minor",
      "Major Pentatonic", "Minor Pentatonic", "Whole Tone", "Diminished",
      "Blues", "Chromatic"};
  std::sort(result.begin(), result.end(), [&](const Scale &a, const Scale &b) {
    int ia = kOrder.indexOf(a.name);
    int ib = kOrder.indexOf(b.name);
    if (ia < 0) { ia = 999; }
    if (ib < 0) { ib = 999; }
    if (ia != ib) { return ia < ib; }
    return a.name < b.name;
  });
  return result;
}

ScaleLibrary::ScaleLibrary() {
  factoryScales = loadFactoryScales();
}

juce::File ScaleLibrary::getUserScaleDirectory(const juce::String &tuningName) {
  auto base = juce::File::getSpecialLocation(
                  juce::File::userApplicationDataDirectory)
                  .getChildFile("ArpsEuclidya")
                  .getChildFile("Scales");
  juce::String folder = tuningName.isEmpty() ? "12-TET" : tuningName;
  return base.getChildFile(folder);
}

void ScaleLibrary::scanUserScales(const juce::String &tuningName) {
  // Remove all existing user scales for this tuning, then re-scan
  userScales.erase(
      std::remove_if(userScales.begin(), userScales.end(),
                     [&](const Scale &s) { return s.tuningName == tuningName; }),
      userScales.end());

  auto dir = getUserScaleDirectory(tuningName);
  if (!dir.isDirectory()) {
    return;
  }
  for (auto it = juce::RangedDirectoryIterator(dir, false, "*.json"); it != end(it); ++it) {
    auto json = it->getFile().loadFileAsString();
    auto scale = parseScaleJSON(json);
    if (scale.name.isNotEmpty() && !scale.stepMask.empty()) {
      userScales.push_back(std::move(scale));
    }
  }
}

std::vector<juce::String> ScaleLibrary::getScaleNames(
    const juce::String &tuningName) const {
  std::vector<juce::String> names;
  for (const auto &s : factoryScales) {
    if (s.tuningName == tuningName) {
      names.push_back(s.name);
    }
  }
  for (const auto &s : userScales) {
    if (s.tuningName == tuningName) {
      names.push_back(s.name);
    }
  }
  return names;
}

const Scale *ScaleLibrary::findScale(const juce::String &name,
                                     const juce::String &tuningName) const {
  for (const auto &s : userScales) {
    if (s.name == name && s.tuningName == tuningName) {
      return &s;
    }
  }
  for (const auto &s : factoryScales) {
    if (s.name == name && s.tuningName == tuningName) {
      return &s;
    }
  }
  return nullptr;
}

void ScaleLibrary::saveUserScale(const Scale &scale) {
  auto dir = getUserScaleDirectory(scale.tuningName);
  dir.createDirectory();

  juce::DynamicObject::Ptr obj = new juce::DynamicObject();
  obj->setProperty("name", scale.name);
  obj->setProperty("tuningName", scale.tuningName);
  juce::Array<juce::var> maskArr;
  for (bool b : scale.stepMask) {
    maskArr.add(b);
  }
  obj->setProperty("stepMask", maskArr);

  auto json = juce::JSON::toString(juce::var(obj.get()), true);
  auto file = dir.getChildFile(scale.name + ".json");
  file.replaceWithText(json);

  // Update in-memory list (replace if name already exists)
  for (auto &s : userScales) {
    if (s.name == scale.name && s.tuningName == scale.tuningName) {
      s = scale;
      return;
    }
  }
  userScales.push_back(scale);
}
