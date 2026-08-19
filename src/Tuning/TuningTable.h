#pragma once

// TuningTable — provided by chalkwalk-music (`libs/music`, MIT, JUCE-free).
// See `../../ECOSYSTEM.md`. A using-declaration, so every existing call site
// keeps working and nothing here can drift.
//
// The shared type drops the `sclFile`/`kbmFile` members this one used to
// carry. They were write-only: set by the parser and never read, because the
// processor tracks the active paths itself (`activeSclRelPath`). Provenance is
// the application's business, not the tuning's.

#include <chalkwalk/music/Tuning.h>

using chalkwalk::music::TuningTable;
