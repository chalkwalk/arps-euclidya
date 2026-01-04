#pragma once

#include "DataModel.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>
#include <string>

class GraphNode {
public:
    virtual ~GraphNode() = default;

    virtual std::string getName() const = 0;

    // Triggered when an upstream connection sets the dirty flag
    virtual void process() = 0;

    void setInputSequence(int inputPort, const NoteSequence& sequence);
    NoteSequence getOutputSequence(int outputPort) const;

    void addConnection(int thisOutputPort, GraphNode* targetNode, int targetInputPort);

    virtual std::vector<juce::Component*> getEditorControls() { return {}; }

protected:
    std::map<int, NoteSequence> inputSequences;
    std::map<int, NoteSequence> outputSequences;

    struct Connection {
        GraphNode* targetNode;
        int targetInputPort;
    };
    std::map<int, std::vector<Connection>> connections;
};
