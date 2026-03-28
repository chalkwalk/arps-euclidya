#pragma once

#include "../GraphNode.h"

class UnzipNode : public GraphNode {
 public:
  UnzipNode() = default;
  ~UnzipNode() override = default;

  std::string getName() const override { return "Unzip"; }
  int getNumInputPorts() const override { return 1; }
  int getNumOutputPorts() const override { return 2; }

  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
