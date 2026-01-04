#include "GraphNode.h"

void GraphNode::setInputSequence(int inputPort, const NoteSequence& sequence)
{
    inputSequences[inputPort] = sequence;
}

NoteSequence GraphNode::getOutputSequence(int outputPort) const
{
    auto it = outputSequences.find(outputPort);
    if (it != outputSequences.end())
        return it->second;
        
    return {};
}

void GraphNode::addConnection(int thisOutputPort, GraphNode* targetNode, int targetInputPort)
{
    connections[thisOutputPort].push_back({targetNode, targetInputPort});
}
