#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct HeldNote {
    int noteNumber = 0;
    int channel = 1;
    float velocity = 0.0f;
    float mpeX = 0.0f; // Pitch Bend
    float mpeY = 0.0f; // CC74 / Timbre
    float mpeZ = 0.0f; // Pressure

    bool operator==(const HeldNote& other) const {
        return noteNumber == other.noteNumber && channel == other.channel;
    }
};

using NoteSequence = std::vector<std::vector<HeldNote>>;
