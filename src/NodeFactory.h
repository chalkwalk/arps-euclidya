#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "GraphNode.h"

class NoteExpressionManager;
class ClockManager;

class NodeFactory {
 public:
  static std::shared_ptr<GraphNode> createNode(const std::string &type,
                                               NoteExpressionManager &midiCtx,
                                               ClockManager &clockCtx);

  static std::vector<std::string> getAvailableNodeTypes();

  static std::vector<std::pair<std::string, std::vector<std::string>>>
  getNodeCategories();

  static std::map<std::string, std::vector<std::string>> getNodeTags();

  struct NodeMetadata {
    int gridW = 1, gridH = 1, numIn = 1, numOut = 1;
  };

  static NodeMetadata getPreviewMetadata(const std::string &type);
};
