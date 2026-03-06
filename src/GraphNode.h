#pragma once

#include "DataModel.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <vector>

class GraphNode {
public:
  GraphNode() : nodeId(juce::Uuid()) {}
  virtual ~GraphNode() = default;

  juce::Uuid nodeId;

  virtual std::string getName() const = 0;

  // Port counts — override in subclasses with non-standard port layouts
  virtual int getNumInputPorts() const { return 1; }
  virtual int getNumOutputPorts() const { return 1; }

  // Grid footprint (integer grid cells)
  virtual int getGridWidth() const { return 1; }
  virtual int getGridHeight() const { return 1; }

  // Canvas position (persistent grid coordinates)
  int gridX = 0;
  int gridY = 0;

  // Legacy float coordinates (kept temporarily for runtime dragging during
  // Phase 1-3 transition)
  float nodeX = 0.0f;
  float nodeY = 0.0f;

  virtual void saveNodeState(juce::XmlElement *xml) { juce::ignoreUnused(xml); }
  virtual void loadNodeState(juce::XmlElement *xml) { juce::ignoreUnused(xml); }

  // Base-level save/load for position (called by GraphEngine)
  void saveBaseState(juce::XmlElement *xml) {
    if (xml) {
      xml->setAttribute("gridX", gridX);
      xml->setAttribute("gridY", gridY);
    }
  }
  void loadBaseState(juce::XmlElement *xml) {
    if (xml) {
      if (xml->hasAttribute("gridX")) {
        gridX = xml->getIntAttribute("gridX", 0);
        gridY = xml->getIntAttribute("gridY", 0);
      } else {
        // Legacy float patch conversion: snap to nearest 100px grid
        float x = (float)xml->getDoubleAttribute("nodeX", 0.0);
        float y = (float)xml->getDoubleAttribute("nodeY", 0.0);
        gridX = (int)std::round(x / 100.0f);
        gridY = (int)std::round(y / 100.0f);
      }

      // Keep float sync for Phase 1-3 transitional logic
      nodeX = (float)(gridX * 100);
      nodeY = (float)(gridY * 100);
    }
  }

  // Triggered when an upstream connection sets the dirty flag
  virtual void process() = 0;

  virtual std::unique_ptr<juce::Component>
  createEditorComponent(juce::AudioProcessorValueTreeState &apvts) {
    juce::ignoreUnused(apvts);
    return nullptr;
  }

  // Return pairs of (parameter display name, pointer to macro index field)
  // Used by PluginProcessor to build dynamic macro parameter names
  virtual std::vector<std::pair<juce::String, int *>> getMacroMappings() {
    return {};
  }

  std::function<void()> onNodeDirtied;

  void setInputSequence(int inputPort, const NoteSequence &sequence);
  void clearInputSequence(int inputPort);
  const NoteSequence &getOutputSequence(int outputPort) const;

  struct Connection {
    GraphNode *targetNode;
    int targetInputPort;
  };

  void addConnection(int thisOutputPort, GraphNode *targetNode,
                     int targetInputPort);
  const std::map<int, std::vector<Connection>> &getConnections() const {
    return connections;
  }
  void clearConnections();

protected:
  std::map<int, NoteSequence> inputSequences;
  std::map<int, NoteSequence> outputSequences;

  std::map<int, std::vector<Connection>> connections;
};
