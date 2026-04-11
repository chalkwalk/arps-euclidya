#include "ScalaParser.h"

#include <cmath>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static double ratioToCents(double numerator, double denominator) {
  return 1200.0 * std::log2(numerator / denominator);
}

// Return true and fill value if the token is a ratio (e.g. "3/2").
static bool parseRatio(const juce::String& token, double& outCents) {
  int slash = token.indexOfChar('/');
  if (slash < 0) { return false; }
  double num = token.substring(0, slash).getDoubleValue();
  double den = token.substring(slash + 1).getDoubleValue();
  if (den == 0.0) { return false; }
  outCents = ratioToCents(num, den);
  return true;
}

// ---------------------------------------------------------------------------
// ScalaParser::parseSclCents
// ---------------------------------------------------------------------------

std::vector<double> ScalaParser::parseSclCents(const juce::String& sclText,
                                               juce::String& nameOut) {
  std::vector<double> result;
  nameOut.clear();

  juce::StringArray lines;
  lines.addLines(sclText);

  int phase = 0;  // 0 = before description, 1 = before N, 2 = reading pitches
  int expectedN = 0;

  for (const auto& rawLine : lines) {
    juce::String line = rawLine.trim();
    if (line.startsWithChar('!')) { continue; }

    if (phase == 0) {
      nameOut = line;
      phase = 1;
      continue;
    }
    if (phase == 1) {
      expectedN = line.getIntValue();
      phase = 2;
      continue;
    }
    // phase == 2: read pitch entries
    if (line.isEmpty()) { continue; }
    // Strip inline comments (anything after a !)
    int commentPos = line.indexOfChar('!');
    if (commentPos >= 0) { line = line.substring(0, commentPos).trim(); }
    if (line.isEmpty()) { continue; }

    double cents = 0.0;
    if (!parseRatio(line, cents)) {
      cents = line.getDoubleValue();
    }
    result.push_back(cents);
    if ((int)result.size() >= expectedN) { break; }
  }

  return result;
}

// ---------------------------------------------------------------------------
// ScalaParser::parseKbm
// ---------------------------------------------------------------------------

ScalaParser::KbmData ScalaParser::parseKbm(const juce::String& kbmText) {
  KbmData kbm;
  juce::StringArray lines;
  lines.addLines(kbmText);

  std::vector<juce::String> dataLines;
  for (const auto& rawLine : lines) {
    juce::String line = rawLine.trim();
    if (line.startsWithChar('!') || line.isEmpty()) { continue; }
    // Strip inline comments
    int commentPos = line.indexOfChar('!');
    if (commentPos >= 0) { line = line.substring(0, commentPos).trim(); }
    if (!line.isEmpty()) { dataLines.push_back(line); }
  }

  // Header: 7 lines
  if (dataLines.size() >= 1) { kbm.mapSize = dataLines[0].getIntValue(); }
  if (dataLines.size() >= 2) { kbm.firstNote = dataLines[1].getIntValue(); }
  if (dataLines.size() >= 3) { kbm.lastNote = dataLines[2].getIntValue(); }
  if (dataLines.size() >= 4) { kbm.middleNote = dataLines[3].getIntValue(); }
  if (dataLines.size() >= 5) { kbm.refNote = dataLines[4].getIntValue(); }
  if (dataLines.size() >= 6) { kbm.refFreq = dataLines[5].getDoubleValue(); }
  if (dataLines.size() >= 7) { kbm.repeatDegree = dataLines[6].getIntValue(); }

  // Mapping entries
  for (size_t i = 7; i < dataLines.size(); ++i) {
    juce::String entry = dataLines[i].trim();
    if (entry.equalsIgnoreCase("x")) {
      kbm.mapping.push_back(-1);
    } else {
      kbm.mapping.push_back(entry.getIntValue());
    }
  }

  return kbm;
}

// ---------------------------------------------------------------------------
// ScalaParser::defaultKbm
// ---------------------------------------------------------------------------

ScalaParser::KbmData ScalaParser::defaultKbm(int scaleSize) {
  KbmData kbm;
  kbm.mapSize = scaleSize;
  kbm.firstNote = 0;
  kbm.lastNote = 127;
  kbm.middleNote = 60;
  kbm.refNote = 69;
  kbm.refFreq = 440.0;
  kbm.repeatDegree = scaleSize;
  kbm.mapping.resize((size_t)scaleSize);
  for (int i = 0; i < scaleSize; ++i) { kbm.mapping[(size_t)i] = i; }
  return kbm;
}

// ---------------------------------------------------------------------------
// ScalaParser::computeTable
// ---------------------------------------------------------------------------

TuningTable ScalaParser::computeTable(const std::vector<double>& scaleCents,
                                      const KbmData& kbm,
                                      const juce::String& name,
                                      const juce::File& sclFile,
                                      const juce::File& kbmFile) {
  TuningTable table;
  table.name = name;
  table.sclFile = sclFile;
  table.kbmFile = kbmFile;

  if (scaleCents.empty()) { return table; }

  // scaleCents[0..N-2] are the N-1 scale steps; scaleCents[N-1] is the period.
  // Build an array of scale degree → cents-from-unison (including degree 0 = 0.0).
  int scaleSize = (int)scaleCents.size();  // = N from the SCL file
  std::vector<double> degreeCents((size_t)(scaleSize + 1));
  degreeCents[0] = 0.0;
  for (int i = 0; i < scaleSize; ++i) {
    degreeCents[(size_t)i + 1u] = scaleCents[(size_t)i];
  }
  double periodCents = scaleCents[(size_t)(scaleSize - 1)];

  int mapSize = kbm.mapSize > 0 ? kbm.mapSize : scaleSize;

  // Resolve the reference note's cents-from-zero and base frequency
  auto resolveCentFromZero = [&](int midiNote) -> double {
    int keyRelative = midiNote - kbm.middleNote;
    // Floor division (handles negative correctly)
    int octave = 0;
    int mapPos = 0;
    if (keyRelative >= 0) {
      octave = keyRelative / mapSize;
      mapPos = keyRelative % mapSize;
    } else {
      // For negative: floor division
      octave = (keyRelative - mapSize + 1) / mapSize;
      mapPos = keyRelative - octave * mapSize;
    }
    if (mapPos < 0 || mapPos >= (int)kbm.mapping.size()) { return 0.0; }
    int degree = kbm.mapping[(size_t)mapPos];
    if (degree < 0) { return std::numeric_limits<double>::quiet_NaN(); }
    degree = juce::jlimit(0, scaleSize, degree);
    double base = degreeCents[(size_t)degree];
    double shift = (double)octave * periodCents;
    return base + shift;
  };

  double refCentFromZero = resolveCentFromZero(kbm.refNote);
  bool refIsNaN = std::isnan(refCentFromZero);

  for (int n = 0; n < 128; ++n) {
    double centFromZero = resolveCentFromZero(n);
    if (std::isnan(centFromZero) || refIsNaN) {
      table.centsDeviation[(size_t)n] = 0.0f;
      continue;
    }

    // Tuned frequency of note n
    double tunedFreq =
        kbm.refFreq * std::pow(2.0, (centFromZero - refCentFromZero) / 1200.0);

    // 12-TET frequency of note n
    double twelveFreq = 440.0 * std::pow(2.0, (n - 69) / 12.0);

    table.centsDeviation[(size_t)n] =
        (float)(1200.0 * std::log2(tunedFreq / twelveFreq));
  }

  return table;
}

// ---------------------------------------------------------------------------
// ScalaParser::parse
// ---------------------------------------------------------------------------

TuningTable ScalaParser::parse(const juce::File& sclFile,
                               const juce::File& kbmFile) {
  juce::String sclText = sclFile.loadFileAsString();
  juce::String scaleName;
  std::vector<double> scaleCents = parseSclCents(sclText, scaleName);

  if (scaleName.isEmpty()) {
    scaleName = sclFile.getFileNameWithoutExtension();
  }

  KbmData kbm = (kbmFile.existsAsFile())
                    ? parseKbm(kbmFile.loadFileAsString())
                    : defaultKbm((int)scaleCents.size());

  return computeTable(scaleCents, kbm, scaleName, sclFile, kbmFile);
}
