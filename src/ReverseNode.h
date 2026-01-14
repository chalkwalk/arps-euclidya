#pragma once

#include "GraphNode.h"

class ReverseNode : public GraphNode {
public:
  ReverseNode() = default;
  ~ReverseNode() override = default;

  std::string getName() const override { return "Reverse Node"; }

  void process() override;
};
