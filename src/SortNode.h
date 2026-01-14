#pragma once

#include "GraphNode.h"

class SortNode : public GraphNode {
public:
  SortNode() = default;
  ~SortNode() override = default;

  std::string getName() const override { return "Sort Node"; }

  void process() override;
};
