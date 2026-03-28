#pragma once

#include "../GraphNode.h"

class SortNode : public GraphNode {
 public:
  SortNode() = default;
  ~SortNode() override = default;

  std::string getName() const override { return "Sort"; }

  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
