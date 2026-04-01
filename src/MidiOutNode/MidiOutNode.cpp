#include "MidiOutNode.h"

#include "../EuclideanMath.h"

MidiOutNode::MidiOutNode(NoteExpressionManager &midiCtx, ClockManager &clockCtx,
                         std::array<std::atomic<float> *, 32> macrosArray)
    : noteExpressionManager(midiCtx),
      clockManager(clockCtx),
      macros(macrosArray) {}

namespace {
bool stepsAreEqual(const std::vector<HeldNote> &a,
                   const std::vector<HeldNote> &b) {
  if (a.size() != b.size()) {
    return false;
  }
  for (size_t i = 0; i < a.size(); ++i) {
    if (!(a[i] == b[i])) {
      return false;
    }
  }
  return true;
}

int findClosestNoteIndex(const std::vector<HeldNote> &stepToFind,
                         const NoteSequence &oldSequence,
                         const NoteSequence &newSequence, int previousIndex) {
  if (newSequence.empty()) {
    return 0;
  }

  int nL = newSequence.size();

  for (int i = 0; i < nL; ++i) {
    int cI = ((previousIndex + i) % nL);
    if (stepsAreEqual(newSequence[(size_t)cI], stepToFind)) {
      return cI;
    }
  }

  for (int i = 0; i < nL; ++i) {
    if (stepsAreEqual(newSequence[(size_t)i], stepToFind)) {
      return i;
    }
  }

  int oL = std::max(1, (int)oldSequence.size());
  return ((previousIndex * nL) / oL) % nL;
}

// Clock division table: PPQ values for each division
// Index: 0=4/1, 1=2/1, 2=1/1, 3=1/2, 4=1/4, 5=1/8, 6=1/16, 7=1/32
constexpr double DIVISIONS[] = {16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125};
constexpr int NUM_DIVISIONS = 8;

int countOnes(const std::vector<bool> &pattern, int upTo) {
  int count = 0;
  for (int i = 0; i < upTo && i < (int)pattern.size(); ++i) {
    if (pattern[static_cast<size_t>(i)]) {
      count++;
    }
  }
  return count;
}

int findIndexOfNote(const std::vector<bool> &pattern, int n) {
  int seen = 0;
  for (int i = 0; i < (int)pattern.size(); ++i) {
    if (pattern[static_cast<size_t>(i)]) {
      if (seen == n) {
        return i;
      }
      seen++;
    }
  }
  return 0;  // Should not happen if beats > 0
}

}  // namespace

void MidiOutNode::clampParameters() {
  pSteps = std::max(1, pSteps);
  pBeats = std::clamp(pBeats, 1, pSteps);
  pOffset = std::clamp(pOffset, -(pSteps + 1) / 2, (pSteps + 1) / 2);

  rSteps = std::max(1, rSteps);
  rBeats = std::clamp(rBeats, 1, rSteps);
  rOffset = std::clamp(rOffset, -(rSteps + 1) / 2, (rSteps + 1) / 2);

  syncMode = static_cast<SyncMode>(ui_syncMode);
  patternMode = static_cast<PatternMode>(ui_patternMode);
  patternResetOnRelease = (ui_patternResetOnRelease != 0);
  rhythmResetOnRelease = (ui_rhythmResetOnRelease != 0);
  triplet = (ui_triplet != 0);

  ui_pBeatsMin = 1;
  ui_pBeatsMax = pSteps;
  ui_pOffsetMin = -(pSteps + 1) / 2;
  ui_pOffsetMax = (pSteps + 1) / 2;

  ui_rBeatsMin = 1;
  ui_rBeatsMax = rSteps;
  ui_rOffsetMin = -(rSteps + 1) / 2;
  ui_rOffsetMax = (rSteps + 1) / 2;
}

void MidiOutNode::flushPlayingNotes(NoteEventCollector &collector,
                                    int samplePosition) {
  for (const auto &note : playingNotes) {
    collector.addNoteOff(note.channel, note.noteNumber, 0.0f, samplePosition,
                         note.noteID);
  }
  playingNotes.clear();
}

void MidiOutNode::process() {
  clampParameters();

  auto it = inputSequences.find(0);
  if (it != inputSequences.end()) {
    const NoteSequence &newSequence = it->second;
    int numSteps = (int)newSequence.size();

    if (numSteps == 0) {
      sequenceIndex = 0;
    } else if (!previousSequence.empty() &&
               sequenceIndex < (int)previousSequence.size()) {
      auto stepToFind = previousSequence[static_cast<size_t>(sequenceIndex)];
      sequenceIndex = findClosestNoteIndex(stepToFind, previousSequence,
                                           newSequence, sequenceIndex);
    } else {
      if (sequenceIndex >= numSteps) {
        sequenceIndex = 0;
      }
    }

    previousSequence = newSequence;
  }
}

void MidiOutNode::generateOutput(NoteEventCollector &collector,
                                 int samplePosition) {
  auto it0 = inputSequences.find(0);
  bool holdingNotes = (it0 != inputSequences.end() && !it0->second.empty());
  bool isPlaying = clockManager.isHostPlaying();
  double currentPpq = clockManager.getCumulativePpq();

  // --- Compute current division ---
  int divIdx = std::clamp(clockDivisionIndex, 0, NUM_DIVISIONS - 1);
  double division = DIVISIONS[divIdx];
  if (triplet) {
    division *= (2.0 / 3.0);
  }

  // --- Reset/Sync State Tracking ---
  if (wasHoldingNotes && !holdingNotes) {
    if (patternResetOnRelease) {
      sequenceIndex = 0;
      patternIndex = 0;
      visualPatternIndex = 0;
    }
    if (rhythmResetOnRelease) {
      rhythmIndex = 0;
      visualRhythmIndex = 0;
    }
    syncArmed = true;
  }

  // Detect onset (start trigger)
  if (!wasHoldingNotes && holdingNotes) {
    if (syncMode == SyncMode::Gestural) {
      anchorPpq = currentPpq;
      forceTick = true;
      syncArmed = false;
    } else if (syncMode == SyncMode::Synchronized ||
               syncMode == SyncMode::Deterministic) {
      // Snap anchor to the nearest past division boundary of the DAW grid
      anchorPpq = std::floor(currentPpq / division) * division;
      forceTick = true;
      syncArmed = false;
    } else if (syncMode == SyncMode::Forgiving) {
      // Snap anchor to the nearest past boundary
      anchorPpq = std::floor(currentPpq / division) * division;
      double lateness = currentPpq - anchorPpq;
      double graceThreshold = division / 8.0;

      if (lateness < graceThreshold) {
        // Close enough to the beat — fire immediately and schedule phase slip
        // to correct the early drift over the next few beats.
        forgivingSlipFraction = lateness / division;
      } else {
        // Too late — behave like Synchronized: wait for the next tick
        forgivingSlipFraction = 0.0;
      }
      forceTick = true;
      syncArmed = false;
    }
  }

  // Detect transport start
  if (!wasPlaying && isPlaying) {
    // If transport starts exactly on a grid division and we're holding notes,
    // fire it. We check if floor(current/div) is effectively 0.0 or unchanged.
    if (holdingNotes && std::fmod(currentPpq, division) < 0.001) {
      forceTick = true;
    }
  }

  // --- Tick Detection ---
  bool isTick = false;

  // For Forgiving mode, compute an adjusted division for phase-slip
  // convergence. Each tick the slip fraction halves, shortening the effective
  // division slightly so that subsequent ticks arrive a little earlier until
  // we're back in phase.
  double adjustedDivision = division;
  if (syncMode == SyncMode::Forgiving && forgivingSlipFraction > 0.0) {
    adjustedDivision = division * (1.0 - forgivingSlipFraction);
  }

  double effectivePpq = (syncMode == SyncMode::Deterministic)
                            ? currentPpq
                            : (currentPpq - anchorPpq);
  double prevEffective = (syncMode == SyncMode::Deterministic)
                             ? lastTickPpq
                             : (lastTickPpq - anchorPpq);

  double currTick = std::floor(effectivePpq / adjustedDivision);
  double prevTick = std::floor(prevEffective / adjustedDivision);

  if (currTick > prevTick || forceTick) {
    if (syncMode == SyncMode::Gestural || isPlaying) {
      isTick = true;
      forceTick = false;
      // Decay the slip each time a real tick fires
      if (syncMode == SyncMode::Forgiving && forgivingSlipFraction > 0.0) {
        forgivingSlipFraction *= 0.5;
        if (forgivingSlipFraction < 0.001)
          forgivingSlipFraction = 0.0;
      }
    }
  }

  wasHoldingNotes = holdingNotes;
  wasPlaying = isPlaying;
  lastTickPpq = currentPpq;

  // Cleanup playing notes on every tick
  if (isTick) {
    flushPlayingNotes(collector, samplePosition);
    lastTickPlayedNote = false;
  }

  if (!isPlaying && wasPlaying) {
    flushPlayingNotes(collector, samplePosition);
  }

  if (!holdingNotes) {
    return;
  }

  if (isTick) {
    const auto &sequence = it0->second;
    clampParameters();

    // --- Retrieve Parameters (Macros etc) ---
    int actualRSteps =
        macroRSteps != -1 && macros[static_cast<size_t>(macroRSteps)] != nullptr
            ? 1 + (int)std::round(
                      macros[static_cast<size_t>(macroRSteps)]->load() * 31.0f)
            : rSteps;
    int actualRBeats =
        macroRBeats != -1 && macros[static_cast<size_t>(macroRBeats)] != nullptr
            ? 1 + (int)std::round(
                      macros[static_cast<size_t>(macroRBeats)]->load() *
                      (float)(actualRSteps - 1))
            : rBeats;
    actualRBeats = std::clamp(actualRBeats, 1, actualRSteps);
    int halfR = (actualRSteps + 1) / 2;
    int actualROffset =
        macroROffset != -1 &&
                macros[static_cast<size_t>(macroROffset)] != nullptr
            ? (int)std::round(
                  (macros[static_cast<size_t>(macroROffset)]->load() - 0.5f) *
                  (float)actualRSteps)
            : rOffset;
    actualROffset = std::clamp(actualROffset, -halfR, halfR);

    std::vector<bool> rhythmPattern = EuclideanMath::generatePattern(
        actualRSteps, actualRBeats, actualROffset);

    if (rhythmPattern.empty()) {
      return;
    }

    // --- Pattern Logic ---
    int actualPSteps =
        macroPSteps != -1 && macros[static_cast<size_t>(macroPSteps)] != nullptr
            ? 1 + (int)std::round(
                      macros[static_cast<size_t>(macroPSteps)]->load() * 31.0f)
            : pSteps;

    int actualPBeats =
        macroPBeats != -1 && macros[static_cast<size_t>(macroPBeats)] != nullptr
            ? 1 + (int)std::round(
                      macros[static_cast<size_t>(macroPBeats)]->load() *
                      (float)(actualPSteps - 1))
            : pBeats;
    actualPBeats = std::clamp(actualPBeats, 1, actualPSteps);
    int halfP = (actualPSteps + 1) / 2;
    int actualPOffset =
        macroPOffset != -1 &&
                macros[static_cast<size_t>(macroPOffset)] != nullptr
            ? (int)std::round(
                  (macros[static_cast<size_t>(macroPOffset)]->load() - 0.5f) *
                  (float)actualPSteps)
            : pOffset;
    actualPOffset = std::clamp(actualPOffset, -halfP, halfP);

    std::vector<bool> pattern = EuclideanMath::generatePattern(
        actualPSteps, actualPBeats, actualPOffset);

    if (pattern.empty()) {
      return;
    }

    // --- Mode-Specific Index Calculation ---
    if (syncMode == SyncMode::Deterministic) {
      long long absTick = (long long)currTick;
      rhythmIndex = (int)(absTick % (long long)actualRSteps);

      long long cycles = absTick / (long long)actualRSteps;
      int partialSteps = (int)(absTick % (long long)actualRSteps);
      long long nR = (cycles * (long long)actualRBeats) +
                     (long long)countOnes(rhythmPattern, partialSteps);

      if (patternMode == PatternMode::Clocked) {
        if (nR > 0) {
          // Pos(nR, Pattern)
          long long k = nR - 1;
          long long pCycles = k / (long long)actualPBeats;
          int pNoteIdx = (int)(k % (long long)actualPBeats);
          long long pos = (pCycles * (long long)actualPSteps) +
                          (long long)findIndexOfNote(pattern, pNoteIdx);

          patternIndex = (int)(pos % (long long)actualPSteps);
          sequenceIndex = (int)(pos % (long long)sequence.size());
        } else {
          patternIndex = 0;
          sequenceIndex = 0;
        }
      } else {
        // Gated: Rhythm counts map directly to Indices
        patternIndex = (int)(nR % (long long)actualPSteps);
        sequenceIndex = (int)(nR % (long long)sequence.size());
      }
    }

    bool isRhythmBeat =
        rhythmPattern[(size_t)rhythmIndex % rhythmPattern.size()];
    visualRhythmIndex = rhythmIndex % (int)rhythmPattern.size();

    if (syncMode != SyncMode::Deterministic) {
      rhythmIndex = ((rhythmIndex + 1) % (int)rhythmPattern.size());
    }

    if (!isRhythmBeat) {
      return;
    }

    // --- Note Triggering & Advance ---
    if (syncMode != SyncMode::Deterministic) {
      if (patternMode == PatternMode::Clocked) {
        // Skip over rests instantaneously
        int limit = (int)pattern.size();
        while (!pattern[(size_t)patternIndex % pattern.size()] && limit-- > 0) {
          patternIndex = ((patternIndex + 1) % actualPSteps);
          sequenceIndex = ((sequenceIndex + 1) % (int)sequence.size());
        }
      }
    }

    bool shouldPlay = pattern[(size_t)patternIndex % pattern.size()];
    if (shouldPlay) {
      const auto &step =
          sequence[(size_t)sequenceIndex % (size_t)sequence.size()];
      visualPatternIndex = patternIndex % (int)pattern.size();

      if (!step.empty()) {
        lastTickPlayedNote = true;
      }

      for (const HeldNote &noteTrigger : step) {
        float currentPressure = 0.0f;
        float currentTimbre = 0.0f;

        if (noteTrigger.sourceNoteNumber != -1) {
          currentPressure = noteExpressionManager.getMpeZ(
              noteTrigger.sourceChannel, noteTrigger.sourceNoteNumber,
              noteTrigger.sourceNoteID);
          currentTimbre = noteExpressionManager.getMpeY(
              noteTrigger.sourceChannel, noteTrigger.sourceNoteNumber,
              noteTrigger.sourceNoteID);
        }

        float actualPressMod =
            macroPressureToVelocity != -1 &&
                    macros[(size_t)macroPressureToVelocity] != nullptr
                ? macros[(size_t)macroPressureToVelocity]->load()
                : pressureToVelocity;

        float actualTimbMod =
            macroTimbreToVelocity != -1 &&
                    macros[(size_t)macroTimbreToVelocity] != nullptr
                ? macros[(size_t)macroTimbreToVelocity]->load()
                : timbreToVelocity;

        float finalVelocity = std::clamp(
            noteTrigger.velocity +
                (actualPressMod * (currentPressure - noteTrigger.velocity)) +
                (actualTimbMod * (currentTimbre - noteTrigger.velocity)),
            0.0f, 1.0f);

        // Calculate a new noteID if possible (for CLAP tracking)
        // For now we use -1, but we could generate a sequence here if needed.
        int32_t outNoteID = -1;

        collector.addNoteOn(outputChannel, noteTrigger.noteNumber,
                            finalVelocity, samplePosition, outNoteID);

        playingNotes.push_back(
            {outputChannel, noteTrigger.noteNumber, outNoteID});
      }
    }

    if (syncMode != SyncMode::Deterministic) {
      // Advance to prepare for next rhythm beat
      patternIndex = ((patternIndex + 1) % actualPSteps);
      sequenceIndex = ((sequenceIndex + 1) % (int)sequence.size());
    }
  }
}

juce::String MidiOutNode::getCycleLengthInfo() const {
  // Get input sequence length
  auto it = inputSequences.find(0);
  if (it == inputSequences.end() || it->second.empty()) {
    return "No input";
  }

  int L = (int)it->second.size();

  // Resolve actual pattern params (accounting for macros)
  int actualPSteps =
      macroPSteps != -1 && macros[(size_t)macroPSteps] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroPSteps]->load() * 31.0f)
          : pSteps;

  int actualPBeats =
      macroPBeats != -1 && macros[(size_t)macroPBeats] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroPBeats]->load() *
                                (float)(actualPSteps - 1))
          : pBeats;
  actualPBeats = std::clamp(actualPBeats, 1, actualPSteps);

  // Resolve actual rhythm params
  int actualRSteps =
      macroRSteps != -1 && macros[(size_t)macroRSteps] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroRSteps]->load() * 31.0f)
          : rSteps;

  // Pattern cycle: number of played notes before the melodic sequence repeats
  // Formula: K * L / GCD(L, N) where K=beats, N=steps, L=input length
  auto gcd = [](int a, int b) -> int {
    while (b != 0) {
      int t = b;
      b = a % b;
      a = t;
    }
    return a;
  };
  auto lcm = [&gcd](int a, int b) -> int { return a / gcd(a, b) * b; };

  int patternCycle = actualPBeats * L / gcd(L, actualPSteps);

  // Total cycle in clock ticks: LCM(pattern_cycle, rSteps)
  int totalTicks = lcm(patternCycle, actualRSteps);

  // Convert to quarter beats using clock division
  int divIdx = std::clamp(clockDivisionIndex, 0, NUM_DIVISIONS - 1);
  double divisionPpq = DIVISIONS[divIdx];
  if (triplet) {
    divisionPpq *= (2.0 / 3.0);
  }

  double quarterBeats = totalTicks * divisionPpq;
  double bars = quarterBeats / 4.0;

  // Format output
  juce::String info;
  info << totalTicks << " ticks = ";

  // Show quarter beats as fraction if not whole
  if (std::abs(quarterBeats - std::round(quarterBeats)) < 0.001) {
    info << (int)quarterBeats;
  } else {
    info << juce::String(quarterBeats, 1);
  }
  info << " beats";

  // Show bars
  if (std::abs(bars - std::round(bars)) < 0.001) {
    info << " (" << (int)bars << " bar" << ((int)bars != 1 ? "s" : "") << ")";
  } else {
    info << " (" << juce::String(bars, 2) << " bars)";
  }

  return info;
}

void MidiOutNode::saveNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    xml->setAttribute("pSteps", pSteps);
    xml->setAttribute("pBeats", pBeats);
    xml->setAttribute("pOffset", pOffset);
    xml->setAttribute("rSteps", rSteps);
    xml->setAttribute("rBeats", rBeats);
    xml->setAttribute("rOffset", rOffset);

    xml->setAttribute("macroPSteps", macroPSteps);
    xml->setAttribute("macroPBeats", macroPBeats);
    xml->setAttribute("macroPOffset", macroPOffset);
    xml->setAttribute("macroRSteps", macroRSteps);
    xml->setAttribute("macroRBeats", macroRBeats);
    xml->setAttribute("macroROffset", macroROffset);

    xml->setAttribute("syncMode", (int)syncMode);
    xml->setAttribute("patternMode", (int)patternMode);
    xml->setAttribute("patternResetOnRelease", patternResetOnRelease ? 1 : 0);
    xml->setAttribute("rhythmResetOnRelease", rhythmResetOnRelease ? 1 : 0);
    xml->setAttribute("clockDivisionIndex", clockDivisionIndex);
    xml->setAttribute("triplet", triplet ? 1 : 0);
    xml->setAttribute("outputChannel", outputChannel);

    xml->setAttribute("pressureToVelocity", pressureToVelocity);
    xml->setAttribute("timbreToVelocity", timbreToVelocity);
    xml->setAttribute("macroPressureToVelocity", macroPressureToVelocity);
    xml->setAttribute("macroTimbreToVelocity", macroTimbreToVelocity);
  }
}

void MidiOutNode::loadNodeState(juce::XmlElement *xml) {
  if (xml != nullptr) {
    pSteps = xml->getIntAttribute("pSteps", 16);
    pBeats = xml->getIntAttribute("pBeats", 4);
    pOffset = xml->getIntAttribute("pOffset", 0);
    rSteps = xml->getIntAttribute("rSteps", 16);
    rBeats = xml->getIntAttribute("rBeats", 16);
    rOffset = xml->getIntAttribute("rOffset", 0);

    macroPSteps = xml->getIntAttribute("macroPSteps", -1);
    macroPBeats = xml->getIntAttribute("macroPBeats", -1);
    macroPOffset = xml->getIntAttribute("macroPOffset", -1);
    macroRSteps = xml->getIntAttribute("macroRSteps", -1);
    macroRBeats = xml->getIntAttribute("macroRBeats", -1);
    macroROffset = xml->getIntAttribute("macroROffset", -1);

    // Load new syncMode, fallback to deprecated transportSyncMode
    if (xml->hasAttribute("syncMode")) {
      syncMode = (SyncMode)xml->getIntAttribute("syncMode", 1);
    } else {
      int oldMode = xml->getIntAttribute("transportSyncMode", 0);
      syncMode = (oldMode == 0) ? SyncMode::Synchronized : SyncMode::Gestural;
    }

    patternMode = (PatternMode)xml->getIntAttribute("patternMode",
                                                    (int)PatternMode::Gated);

    patternResetOnRelease =
        xml->getIntAttribute("patternResetOnRelease", 1) != 0;
    rhythmResetOnRelease = xml->getIntAttribute("rhythmResetOnRelease", 1) != 0;
    clockDivisionIndex = xml->getIntAttribute("clockDivisionIndex", 5);
    triplet = xml->getIntAttribute("triplet", 0) != 0;
    outputChannel = xml->getIntAttribute("outputChannel", 1);

    pressureToVelocity = xml->getDoubleAttribute("pressureToVelocity", 0.0);
    timbreToVelocity = xml->getDoubleAttribute("timbreToVelocity", 0.0);
    macroPressureToVelocity =
        xml->getIntAttribute("macroPressureToVelocity", -1);
    macroTimbreToVelocity = xml->getIntAttribute("macroTimbreToVelocity", -1);

    // Sync UI proxies
    ui_syncMode = static_cast<int>(syncMode);
    ui_patternMode = static_cast<int>(patternMode);
    ui_patternResetOnRelease = patternResetOnRelease ? 1 : 0;
    ui_rhythmResetOnRelease = rhythmResetOnRelease ? 1 : 0;
    ui_triplet = triplet ? 1 : 0;
  }
}

std::vector<bool> MidiOutNode::getPattern() const {
  int actualPSteps =
      macroPSteps != -1 && macros[(size_t)macroPSteps] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroPSteps]->load() * 31.0f)
          : pSteps;
  int actualPBeats =
      macroPBeats != -1 && macros[(size_t)macroPBeats] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroPBeats]->load() *
                                (float)(actualPSteps - 1))
          : pBeats;
  int actualPOffset =
      macroPOffset != -1 && macros[(size_t)macroPOffset] != nullptr
          ? (int)std::round((macros[(size_t)macroPOffset]->load() - 0.5f) *
                            (float)actualPSteps)
          : pOffset;

  return EuclideanMath::generatePattern(
      std::max(1, actualPSteps), std::clamp(actualPBeats, 1, actualPSteps),
      actualPOffset);
}

std::vector<bool> MidiOutNode::getRhythm() const {
  int actualRSteps =
      macroRSteps != -1 && macros[(size_t)macroRSteps] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroRSteps]->load() * 31.0f)
          : rSteps;
  int actualRBeats =
      macroRBeats != -1 && macros[(size_t)macroRBeats] != nullptr
          ? 1 + (int)std::round(macros[(size_t)macroRBeats]->load() *
                                (float)(actualRSteps - 1))
          : rBeats;
  int actualROffset =
      macroROffset != -1 && macros[(size_t)macroROffset] != nullptr
          ? (int)std::round((macros[(size_t)macroROffset]->load() - 0.5f) *
                            (float)actualRSteps)
          : rOffset;

  return EuclideanMath::generatePattern(
      std::max(1, actualRSteps), std::clamp(actualRBeats, 1, actualRSteps),
      actualROffset);
}
