#pragma once

#include "../GraphNode.h"

class ZipNode : public GraphNode {
 public:
  ZipNode() = default;
  ~ZipNode() override = default;

  std::string getName() const override { return "Zip"; }
  int getNumInputPorts() const override { return 2; }
  int getNumOutputPorts() const override { return 1; }

  PortType getInputPortType(int /*port*/) const override { return PortType::Agnostic; }
  PortType getOutputPortType(int /*port*/) const override { return PortType::Agnostic; }

  void process() override;
  int getGridWidth() const override { return 1; }
  int getGridHeight() const override { return 1; }
};
