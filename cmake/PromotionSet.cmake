# ---------------------------------------------------------------------------
# WHAT IS ON ITS WAY OUT, DECLARED.
#
# `DESIGN.md` says this project "contributes its Scala tuning parser and
# receives one canonical Euclidean implementation". The receiving half has
# happened -- `chalkwalk-music` supplies the generator, the scale and the
# tuning table, and `src/Tuning/TuningTable.h` is already a seam over it. The
# contributing half has not, and until now nothing said which files it is.
#
# This is that list, and it is short on purpose. A promotion set is a
# DECLARATION, not an inventory of everything that happens to compile without
# JUCE today -- half this tree's node headers do that by accident, because the
# JUCE is in their .cpp.
#
#   Scales/HarmonicAnalysis.h   A consonance score for a step in a tuning.
#                               Music theory by any reading, JUCE-free in BOTH
#                               halves (the .cpp too), and it depends on
#                               nothing but the shared tuning table -- which is
#                               what makes it extractable rather than merely
#                               JUCE-free.
#   Tuning/TuningTable.h        Already a seam over chalkwalk-music. On the
#                               list so the seam itself is held to the
#                               library's floor: it is the one file here whose
#                               whole job is to compile against that library.
#
# ADDING A NAME HERE IS THE DECLARATION THAT A FILE IS LEAVING. Removing one
# says it has left, or that it never was. Neither should happen as a side
# effect of making a build go green.
# ---------------------------------------------------------------------------
set(ARPS_PROMOTION_SET
    Scales/HarmonicAnalysis.h
    Tuning/TuningTable.h)
