#pragma once

#include <juce_core/juce_core.h>

#include <vector>

class TuningLibrary {
 public:
  enum class Bank : std::uint8_t { Factory, User, All };

  struct TuningInfo {
    juce::File sclFile;
    juce::File kbmFile;  // may be invalid (no .kbm → default mapping)
    juce::String name;
    juce::String description;
    juce::String category;
    Bank bank = Bank::User;
  };

  TuningLibrary();

  // Re-scan both Factory and User directories.
  void scan();

  [[nodiscard]] std::vector<TuningInfo> getTunings(Bank bank = Bank::All) const;

  // Case-insensitive substring search over name/description/category.
  [[nodiscard]] std::vector<TuningInfo> search(const juce::String& query,
                                               Bank bank = Bank::All) const;

  // Sorted list of unique category names within the given bank.
  [[nodiscard]] std::vector<juce::String> getCategories(
      Bank bank = Bank::All) const;

  static juce::File getFactoryTuningDirectory();
  static juce::File getUserTuningDirectory();

 private:
  static constexpr const char* kFactoryTuningVersion = "1.2";

  [[nodiscard]] static bool installFactoryTunings();

  void scanDirectory(const juce::File& dir, Bank bank,
                     const juce::String& rootPath);

  static TuningInfo parseTuningFile(const juce::File& sclFile, Bank bank,
                                    const juce::String& rootPath);

  std::vector<TuningInfo> allTunings;
};
