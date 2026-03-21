#pragma once

#include "../GraphNode.h"

class ReverseNode : public GraphNode {
public:
  ReverseNode() = default;
  ~ReverseNode() override = default;

  std::string getName() const override { return "Reverse"; }

  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
