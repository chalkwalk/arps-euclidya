#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <map>
#include <vector>

#include "DataModel.h"
#include "LayoutConstants.h"
#include "NodeLayout.h"

class GraphNode {
 public:
  GraphNode() : nodeId(juce::Uuid()) {}
  virtual ~GraphNode() = default;

  juce::Uuid nodeId;
  bool bypassed = false;

  virtual std::string getName() const = 0;

  // UI and Grid Layout (the source of truth for node footprint)
  virtual NodeLayout getLayout() const { return {}; }

  // Port counts — override in subclasses with non-standard port layouts
  virtual int getNumInputPorts() const { return 1; }
  virtual int getNumOutputPorts() const { return 1; }

  // Grid footprint (derived from layout by default, but overridable)
  virtual int getGridWidth() const {
    auto layout = getLayout();
    return (layout.gridWidth > 0) ? layout.gridWidth : 1;
  }
  virtual int getGridHeight() const {
    auto layout = getLayout();
    return (layout.gridHeight > 0) ? layout.gridHeight : 1;
  }

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
      xml->setAttribute("bypassed", bypassed);
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
        gridX = (int)std::round(x / Layout::GridPitchFloat);
        gridY = (int)std::round(y / Layout::GridPitchFloat);
      }

      // Keep float sync for Phase 1-3 transitional logic
      nodeX = (float)(gridX * Layout::GridPitch) + Layout::TramlineOffset;
      nodeY = (float)(gridY * Layout::GridPitch) + Layout::TramlineOffset;
      bypassed = xml->getBoolAttribute("bypassed", false);
    }
  }

  // Triggered when an upstream connection sets the dirty flag
  virtual void process() = 0;

  virtual std::unique_ptr<juce::Component> createEditorComponent(
      juce::AudioProcessorValueTreeState &apvts) {
    juce::ignoreUnused(apvts);
    return nullptr;
  }

  virtual std::unique_ptr<juce::Component> createCustomComponent(
      const juce::String &name,
      juce::AudioProcessorValueTreeState *apvts = nullptr) {
    juce::ignoreUnused(name, apvts);
    return nullptr;
  }

  // Return pairs of (parameter display name, pointer to macro index field)
  // Used by PluginProcessor to build dynamic macro parameter names
  virtual std::vector<std::pair<juce::String, int *>> getMacroMappings() {
    return {};
  }

  // Hook called by NodeBlock when a UI control mapped to this node changes
  virtual void parameterChanged() {}

  bool isDirty = false;
  std::function<void()> onNodeDirtied;
  std::function<void()> onMappingChanged;

  void setInputSequence(int inputPort, const NoteSequence &sequence);
  void clearInputSequence(int inputPort);
  const NoteSequence &getInputSequence(int inputPort) const;
  const NoteSequence &getOutputSequence(int outputPort) const;
  void setOutputSequence(int outputPort, const NoteSequence &sequence);

  struct Connection {
    GraphNode *targetNode;
    int targetInputPort;
  };

  void addConnection(int thisOutputPort, GraphNode *targetNode,
                     int targetInputPort);
  void removeConnectionTo(GraphNode *targetNode, int targetInputPort);
  void removeAllConnectionsTo(GraphNode *targetNode);
  const std::map<int, std::vector<Connection>> &getConnections() const {
    return connections;
  }
  void clearConnections();

 protected:
  std::map<int, NoteSequence> inputSequences;
  std::map<int, NoteSequence> outputSequences;

  std::map<int, std::vector<Connection>> connections;
};
