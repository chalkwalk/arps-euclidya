#pragma once

#include "GraphNode.h"

class ZipNode : public GraphNode {
public:
  ZipNode() = default;
  ~ZipNode() override = default;

  std::string getName() const override { return "Zip"; }
  int getNumInputPorts() const override { return 2; }
  int getNumOutputPorts() const override { return 1; }

  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
