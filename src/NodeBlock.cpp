#include "NodeBlock.h"
#include "ArpsLookAndFeel.h"
#include "GraphCanvas.h"
#include "GraphEngine.h"
#include "MacroMappingMenu.h"
#include "SharedMacroUI.h"

NodeBlock::NodeBlock(std::shared_ptr<GraphNode> node,
                     juce::AudioProcessorValueTreeState &apvts,
                     GraphCanvas &canvas)
    : targetNode(node), parentCanvas(canvas) {

  titleLabel.setText(node->getName(), juce::dontSendNotification);
  titleLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
  titleLabel.setJustificationType(juce::Justification::centredLeft);
  titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  titleLabel.setInterceptsMouseClicks(false, false);
  addAndMakeVisible(titleLabel);

  deleteButton.setColour(juce::TextButton::buttonColourId,
                         juce::Colours::transparentBlack);
  deleteButton.setColour(juce::TextButton::buttonOnColourId,
                         juce::Colours::transparentBlack);
  deleteButton.setColour(juce::TextButton::textColourOffId,
                         juce::Colours::white);
  deleteButton.setColour(juce::TextButton::textColourOnId, juce::Colours::red);
  addAndMakeVisible(deleteButton);
  deleteButton.onClick = [this] {
    // Defer deletion: rebuild() destroys nodeBlocks (including this NodeBlock)
    // so we must not do it while this button's click handler is on the stack.
    juce::MessageManager::callAsync([cb = onDelete]() {
      if (cb)
        cb();
    });
  };

  auto layout = node->getLayout();
  for (const auto &element : layout.elements) {
    juce::Component *comp = nullptr;

    if (element.type == UIElementType::RotarySlider) {
      auto *slider = new CustomMacroSlider();
      slider->setRange(element.minValue, element.maxValue, 1);
      slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
      slider->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

      if (element.valueRef != nullptr) {
        slider->setValue(*element.valueRef, juce::dontSendNotification);
        slider->onValueChange = [node, slider, valRef = element.valueRef]() {
          *valRef = (int)slider->getValue();
          if (node->onNodeDirtied)
            node->onNodeDirtied();
        };
      }

      if (element.macroIndexRef != nullptr) {
        auto updateSliderVisibility = [slider](int macro) {
          if (macro == -1) {
            slider->removeColour(juce::Slider::rotarySliderFillColourId);
            slider->removeColour(juce::Slider::rotarySliderOutlineColourId);
          } else {
            slider->setColour(juce::Slider::rotarySliderFillColourId,
                              juce::Colours::orange);
            slider->setColour(juce::Slider::rotarySliderOutlineColourId,
                              juce::Colours::orange.withAlpha(0.3f));
          }
        };

        updateSliderVisibility(*element.macroIndexRef);

        slider->onRightClick = [this, node, slider,
                                macroRef = element.macroIndexRef, &apvts,
                                updateSliderVisibility]() {
          MacroMappingMenu::showMenu(slider, *macroRef,
                                     [this, node, macroRef, &apvts, slider,
                                      updateSliderVisibility](int macroIndex) {
                                       *macroRef = macroIndex;
                                       if (node->onMappingChanged)
                                         node->onMappingChanged();
                                     });
        };

        if (*element.macroIndexRef != -1) {
          juce::String paramID =
              "macro_" + juce::String(*element.macroIndexRef + 1);
          dynamicAttachments.push_back(
              std::make_unique<MacroAttachment>(apvts, paramID, *slider));
        }
      }
      comp = slider;
    } else if (element.type == UIElementType::Label) {
      auto *label = new juce::Label();
      label->setText(element.label, juce::dontSendNotification);
      label->setJustificationType(juce::Justification::centred);
      label->setInterceptsMouseClicks(false, false);
      comp = label;
    } else if (element.type == UIElementType::PushButton ||
               element.type == UIElementType::Toggle) {
      auto *button = new CustomMacroButton();
      button->setButtonText(element.label);

      if (element.type == UIElementType::Toggle) {
        button->setClickingTogglesState(true);
      }

      if (element.valueRef != nullptr) {
        // Initial state sync
        button->setToggleState(*element.valueRef != 0,
                               juce::dontSendNotification);

        button->onClick = [node, button, valRef = element.valueRef, element]() {
          if (element.macroIndexRef != nullptr && *element.macroIndexRef != -1)
            return;

          if (element.type == UIElementType::Toggle) {
            *valRef = button->getToggleState() ? 1 : 0;
          } else {
            // PushButton logic (e.g. toggle dest or trigger)
            *valRef = (*valRef == 0) ? 1 : 0;
          }

          if (node->onNodeDirtied)
            node->onNodeDirtied();
        };
      }

      if (element.macroIndexRef != nullptr) {
        button->onRightClick = [this, node, button,
                                macroRef = element.macroIndexRef, &apvts]() {
          MacroMappingMenu::showMenu(button, *macroRef,
                                     [this, node, macroRef](int macroIndex) {
                                       *macroRef = macroIndex;
                                       if (node->onMappingChanged)
                                         node->onMappingChanged();
                                     });
        };

        if (*element.macroIndexRef != -1) {
          juce::String paramID =
              "macro_" + juce::String(*element.macroIndexRef + 1);
          dynamicAttachments.push_back(
              std::make_unique<ButtonAttachment>(apvts, paramID, *button));
        }
      }
      comp = button;
    } else if (element.type == UIElementType::Custom) {
      if (auto custom = node->createCustomComponent(element.customType)) {
        comp = custom.release();
      }
    }

    if (comp != nullptr) {
      dynamicComponents.add(comp);
      addAndMakeVisible(comp);
    }
  }

  if (layout.elements.empty()) {
    customControls = node->createEditorComponent(apvts);
    if (customControls != nullptr) {
      addAndMakeVisible(customControls.get());
    }
  }

  // Physical bounds derived exclusively from grid properties
  int gridW = node->getGridWidth();
  int gridH = node->getGridHeight();

  // Tram lines math
  int width = (gridW * Layout::GridPitch) - Layout::TramlineMargin;
  int height = (gridH * Layout::GridPitch) - Layout::TramlineMargin;

  setSize(width, height);
  startTimerHz(12); // Steady UI sync
}

void NodeBlock::timerCallback() {
  auto layout = targetNode->getLayout();
  for (int i = 0; i < dynamicComponents.size(); ++i) {
    if (i < (int)layout.elements.size()) {
      auto &element = layout.elements[i];
      auto *comp = dynamicComponents[i];

      if (auto *slider = dynamic_cast<juce::Slider *>(comp)) {
        if (element.valueRef != nullptr && !slider->isMouseButtonDown()) {
          slider->setValue(*element.valueRef, juce::dontSendNotification);
        }
      } else if (auto *button = dynamic_cast<juce::TextButton *>(comp)) {
        if (element.label != button->getButtonText()) {
          button->setButtonText(element.label);
        }
        if (element.valueRef != nullptr && !button->isMouseButtonDown()) {
          bool state = (*element.valueRef != 0);
          if (button->getToggleState() != state) {
            button->setToggleState(state, juce::dontSendNotification);
          }
        }
      } else if (auto *label = dynamic_cast<juce::Label *>(comp)) {
        if (element.label != label->getText()) {
          label->setText(element.label, juce::dontSendNotification);
        }
      }
    }
  }
}

void NodeBlock::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();

  // Unified Node body: rounded rectangle (no distinct header bar anymore)
  g.setColour(ArpsLookAndFeel::getForegroundSlate());
  g.fillRoundedRectangle(bounds, 6.0f);

  // Border (Neon tinted)
  if (parentCanvas.getSelectedNode() == targetNode.get()) {
    // Outer glow for selected node
    g.setColour(juce::Colour(0xff0df0e3));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f,
                           2.5f);

    // Selection Halo
    g.setColour(ArpsLookAndFeel::getNeonColor().withAlpha(0.4f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().expanded(4.0f), 8.0f,
                           3.0f);
  }

  // Highlight for proximity auto-connection
  if (parentCanvas.getProximityTargetNode() == targetNode.get()) {
    auto zone = parentCanvas.getProximityZone();
    g.setColour(juce::Colour(0x600df0e3)); // Semi-transparent neon
    if (zone == GraphCanvas::ProximityZone::Left)
      g.fillRoundedRectangle(
          getLocalBounds().removeFromLeft(getWidth() / 4).toFloat(), 6.0f);
    else if (zone == GraphCanvas::ProximityZone::Right)
      g.fillRoundedRectangle(
          getLocalBounds().removeFromRight(getWidth() / 4).toFloat(), 6.0f);
  } else {
    g.setColour(juce::Colour(0xff0df0e3).withAlpha(0.3f));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(0.5f), 6.0f,
                           1.5f);
  }

  // Draw input ports (Half stadiums flush with left edge)
  int numIn = targetNode->getNumInputPorts();
  for (int i = 0; i < numIn; ++i) {
    auto rect = getInputPortRect(i).toFloat();

    // Half-stadium path (rounded on the right, flat on the left)
    juce::Path p;
    p.addRoundedRectangle(rect.getX() - rect.getWidth(), rect.getY(),
                          rect.getWidth() * 2.0f, rect.getHeight(),
                          rect.getHeight() * 0.5f);

    g.saveState();
    g.reduceClipRegion(rect.toNearestInt()); // Clip to just the right half

    g.setColour(juce::Colour(0xff44cc44));
    g.fillPath(p);
    g.setColour(juce::Colour(0xff228822));
    g.strokePath(p, juce::PathStrokeType(1.5f));

    g.restoreState();
  }

  // Draw output ports (Half stadiums flush with right edge)
  int numOut = targetNode->getNumOutputPorts();
  for (int i = 0; i < numOut; ++i) {
    auto rect = getOutputPortRect(i).toFloat();

    // Half-stadium path (rounded on the left, flat on the right)
    juce::Path p;
    p.addRoundedRectangle(rect.getX(), rect.getY(), rect.getWidth() * 2.0f,
                          rect.getHeight(), rect.getHeight() * 0.5f);

    g.saveState();
    g.reduceClipRegion(rect.toNearestInt()); // Clip to just the left half

    g.setColour(juce::Colour(0xff4499ff));
    g.fillPath(p);
    g.setColour(juce::Colour(0xff2266aa));
    g.strokePath(p, juce::PathStrokeType(1.5f));

    g.restoreState();
  }
}

void NodeBlock::resized() {
  auto bounds = getLocalBounds();

  // Floating Header Area
  auto header = bounds.removeFromTop(HEADER_HEIGHT);

  // Right aligned delete button
  deleteButton.setBounds(header.removeFromRight(HEADER_HEIGHT).reduced(3));

  // Title
  titleLabel.setBounds(header.withTrimmedLeft(6));

  // Body: dynamic controls
  auto layout = targetNode->getLayout();

  // Base offset for internal elements (below header, inside margins)
  int startX = PORT_MARGIN;
  int startY = HEADER_HEIGHT;

  // We use a sub-grid of 25 pixels for internal placement
  const int subGridSize = 25;

  for (int i = 0; i < dynamicComponents.size(); ++i) {
    if (i < (int)layout.elements.size()) {
      auto &element = layout.elements[i];
      auto elementBounds = element.gridBounds;

      dynamicComponents[i]->setBounds(
          startX + elementBounds.getX() * subGridSize,
          startY + elementBounds.getY() * subGridSize,
          elementBounds.getWidth() * subGridSize,
          elementBounds.getHeight() * subGridSize);
    }
  }

  if (customControls != nullptr) {
    customControls->setBounds(bounds.reduced(PORT_MARGIN, 4));
  }
}

void NodeBlock::mouseDown(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  // Notify canvas of selection (for z-ordering and cable highlighting)
  if (onSelected)
    onSelected();

  auto pos = e.getPosition();

  // Check if we hit a port
  int outPort = hitTestOutputPort(pos);
  if (outPort >= 0) {
    isDraggingCable = true;
    isDraggingNode = false;
    if (onPortDragStart) {
      auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
      onPortDragStart(this, outPort, true, canvasPos);
    }
    return;
  }

  int inPort = hitTestInputPort(pos);
  if (inPort >= 0) {
    isDraggingCable = true;
    isDraggingNode = false;
    if (onPortDragStart) {
      auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
      onPortDragStart(this, inPort, false, canvasPos);
    }
    return;
  }

  // Otherwise, start dragging the node
  isDraggingNode = true;
  isDraggingCable = false;

  // No longer using standard ComponentDragger since we do strict grid hopping
  // But we store the starting point of the drag
  dragStartGridX = targetNode->gridX;
  dragStartGridY = targetNode->gridY;
  dragStartWorldX = targetNode->nodeX;
  dragStartWorldY = targetNode->nodeY;

  parentCanvas.setGhostTarget(dragStartGridX, dragStartGridY,
                              targetNode->getGridWidth(),
                              targetNode->getGridHeight(), targetNode.get());
}

void NodeBlock::mouseDrag(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  if (isDraggingCable) {
    if (onPortDragging) {
      auto canvasPos = e.getEventRelativeTo(&parentCanvas).getPosition();
      onPortDragging(canvasPos);
    }
    return;
  }

  if (isDraggingNode) {
    // Actually, juce::MouseEvent::getDistanceFromDragStartX() is measured in
    // the component's local coordinates. When the component is scaled via
    // AffineTransform, its local coordinate system remains constant (e.g.,
    // width is always 190). So the distance drag is exactly how many abstract
    // pixels we've moved.
    int deltaX = e.getDistanceFromDragStartX();
    int deltaY = e.getDistanceFromDragStartY();

    // Convert to grid slots: roughly 100px per slot
    int gridDeltaX = (int)std::round((float)deltaX / Layout::GridPitchFloat);
    int gridDeltaY = (int)std::round((float)deltaY / Layout::GridPitchFloat);

    int newGridX = dragStartGridX + gridDeltaX;
    int newGridY = dragStartGridY + gridDeltaY;

    if (newGridX != parentCanvas.getGhostX() ||
        newGridY != parentCanvas.getGhostY()) {
      // Set ghost target; parentCanvas handles the overlap check to tint it
      // red/green
      parentCanvas.setGhostTarget(
          newGridX, newGridY, targetNode->getGridWidth(),
          targetNode->getGridHeight(), targetNode.get());
    }

    // We ALWAYS move the floating physical coordinate of the node to follow the
    // mouse linearly. This must be outside the if block so it updates
    // continuously!
    targetNode->nodeX = dragStartGridX * Layout::GridPitchFloat +
                        Layout::TramlineOffset + deltaX;
    targetNode->nodeY = dragStartGridY * Layout::GridPitchFloat +
                        Layout::TramlineOffset + deltaY;

    if (onPositionChanged)
      onPositionChanged();

    parentCanvas.updateProximityHighlight(
        e.getEventRelativeTo(&parentCanvas).getPosition(), targetNode.get());
  }
}

void NodeBlock::mouseUp(const juce::MouseEvent &e) {
  if (e.mods.isMiddleButtonDown()) {
    return;
  }

  auto mousePosOnCanvas = e.getEventRelativeTo(&parentCanvas).getPosition();

  if (isDraggingCable && onPortDragEnd) {
    onPortDragEnd(mousePosOnCanvas);
  }
  if (isDraggingNode) {
    if (e.mods.isCtrlDown()) {
      int finalX = parentCanvas.getGhostX();
      int finalY = parentCanvas.getGhostY();
      cancelDrag(); // Revert original
      parentCanvas.requestNodeClone(targetNode.get(), finalX, finalY);
      return;
    }

    // Snap to the ghost slot if it's valid...
    if (parentCanvas.isGhostValid()) {
      targetNode->gridX = parentCanvas.getGhostX();
      targetNode->gridY = parentCanvas.getGhostY();
    } else {
      // Otherwise, instead of just popping back to start, attempt to find the
      // nearest valid spiral
      auto nearest = parentCanvas.getEngine().findClosestFreeSpot(
          parentCanvas.getGhostX(), parentCanvas.getGhostY(),
          targetNode->getGridWidth(), targetNode->getGridHeight(),
          targetNode.get());
      targetNode->gridX = nearest.x;
      targetNode->gridY = nearest.y;
    }

    // Lock the physical layout to the final grid slot
    targetNode->nodeX =
        (float)(targetNode->gridX * Layout::GridPitch) + Layout::TramlineOffset;
    targetNode->nodeY =
        (float)(targetNode->gridY * Layout::GridPitch) + Layout::TramlineOffset;

    parentCanvas.clearGhostTarget();

    if (onPositionChanged)
      onPositionChanged();

    if (!parentCanvas.attemptSignalPathInsertion(targetNode.get(),
                                                 mousePosOnCanvas)) {
      parentCanvas.attemptProximityConnection(targetNode.get(),
                                              mousePosOnCanvas);
    }
    parentCanvas.clearProximityHighlight();
  }

  isDraggingCable = false;
  isDraggingNode = false;
}

void NodeBlock::cancelDrag() {
  if (isDraggingNode) {
    targetNode->gridX = dragStartGridX;
    targetNode->gridY = dragStartGridY;
    targetNode->nodeX = dragStartWorldX;
    targetNode->nodeY = dragStartWorldY;
    parentCanvas.clearGhostTarget();
    if (onPositionChanged)
      onPositionChanged();
  }
  isDraggingNode = false;
  isDraggingCable = false;
}

juce::Rectangle<int> NodeBlock::getInputPortRect(int portIndex) const {
  int y = HEADER_HEIGHT + 10 + portIndex * PORT_SPACING;
  // Flush with the left edge. Width is RADIUS. Height is 2*RADIUS.
  return juce::Rectangle<int>(0, y - PORT_RADIUS, PORT_RADIUS, PORT_RADIUS * 2);
}

juce::Rectangle<int> NodeBlock::getOutputPortRect(int portIndex) const {
  int y = HEADER_HEIGHT + 10 + portIndex * PORT_SPACING;
  // Flush with the right edge. Width is RADIUS. Height is 2*RADIUS.
  return juce::Rectangle<int>(getWidth() - PORT_RADIUS, y - PORT_RADIUS,
                              PORT_RADIUS, PORT_RADIUS * 2);
}

juce::Point<int> NodeBlock::getInputPortCentre(int portIndex) const {
  auto rect = getInputPortRect(portIndex);
  return getPosition() + rect.getCentre();
}

juce::Point<int> NodeBlock::getOutputPortCentre(int portIndex) const {
  auto rect = getOutputPortRect(portIndex);
  return getPosition() + rect.getCentre();
}

int NodeBlock::hitTestInputPort(juce::Point<int> localPoint) const {
  int numIn = targetNode->getNumInputPorts();
  auto localFloat = localPoint.toFloat();
  for (int i = 0; i < numIn; ++i) {
    auto rect = getInputPortRect(i);
    auto centre = rect.getCentre().toFloat();
    if (localFloat.getDistanceFrom(centre) <= (float)PORT_HIT_RADIUS)
      return i;
  }
  return -1;
}

int NodeBlock::hitTestOutputPort(juce::Point<int> localPoint) const {
  int numOut = targetNode->getNumOutputPorts();
  auto localFloat = localPoint.toFloat();
  for (int i = 0; i < numOut; ++i) {
    auto rect = getOutputPortRect(i);
    auto centre = rect.getCentre().toFloat();
    if (localFloat.getDistanceFrom(centre) <= (float)PORT_HIT_RADIUS)
      return i;
  }
  return -1;
}

NodeDragPreview::NodeDragPreview(const juce::String &nodeType, int gridW,
                                 int gridH, int numIn, int numOut)
    : type(nodeType), gridW(gridW), gridH(gridH), numIn(numIn), numOut(numOut) {
  int width = (gridW * Layout::GridPitch) - Layout::TramlineMargin;
  int height = (gridH * Layout::GridPitch) - Layout::TramlineMargin;
  setSize(width, height);
  setInterceptsMouseClicks(false, false);
}

void NodeDragPreview::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds().toFloat();

  // Draw translucent body
  g.setColour(ArpsLookAndFeel::getForegroundSlate().withAlpha(0.6f));
  g.fillRoundedRectangle(bounds, 6.0f);

  // Border
  g.setColour(juce::Colours::white.withAlpha(0.4f));
  g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

  // Header/Title
  g.setColour(juce::Colours::white.withAlpha(0.8f));
  g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
  g.drawText(type, 10, 0, getWidth() - 20, NodeBlock::HEADER_HEIGHT,
             juce::Justification::centredLeft);

  // Ports
  g.setColour(ArpsLookAndFeel::getBackgroundCharcoal().withAlpha(0.8f));
  for (int i = 0; i < numIn; ++i) {
    int py = NodeBlock::HEADER_HEIGHT + 10 + i * NodeBlock::PORT_SPACING;
    g.fillEllipse(0.0f, (float)(py - NodeBlock::PORT_RADIUS),
                  (float)NodeBlock::PORT_RADIUS,
                  (float)(NodeBlock::PORT_RADIUS * 2));
  }
  for (int i = 0; i < numOut; ++i) {
    int py = NodeBlock::HEADER_HEIGHT + 10 + i * NodeBlock::PORT_SPACING;
    g.fillEllipse((float)(getWidth() - NodeBlock::PORT_RADIUS),
                  (float)(py - NodeBlock::PORT_RADIUS),
                  (float)NodeBlock::PORT_RADIUS,
                  (float)(NodeBlock::PORT_RADIUS * 2));
  }
}
