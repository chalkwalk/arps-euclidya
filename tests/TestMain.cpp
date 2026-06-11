#include <juce_events/juce_events.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

// Manages the JUCE MessageManager lifetime for the test run.
// We need getInstance() to avoid assertion failures in JUCE internals
// during headless construction of nodes that use juce::String or Uuid.
// Never call initialiseJuce_GUI() and never instantiate juce::Component.
struct JuceTestListener : Catch::EventListenerBase {
  using EventListenerBase::EventListenerBase;

  void testRunStarting(Catch::TestRunInfo const&) override {
    juce::MessageManager::getInstance();
  }

  void testRunEnded(Catch::TestRunStats const&) override {
    juce::DeletedAtShutdown::deleteAll();
    juce::MessageManager::deleteInstance();
  }
};

CATCH_REGISTER_LISTENER(JuceTestListener)
