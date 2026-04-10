#pragma once

#include <juce_core/juce_core.h>

#include <vector>

// Bump this string whenever factory patches change to force re-extraction.
static constexpr const char *kFactoryPatchVersion = "1.3";

class PatchLibrary {
 public:
  enum class Bank { Factory, User, All };

  struct PatchInfo {
    juce::File file;
    juce::String name;
    juce::String author;
    juce::String description;
    juce::String tags;
    juce::String category;  // derived from subdirectory path
    Bank bank;
    juce::String created;
    juce::String modified;
  };

  PatchLibrary();

  // Re-scan the filesystem and rebuild the index.
  // Calls installFactoryPatches() first to ensure factory bank is up-to-date.
  void scan();

  // Extract embedded factory patches to the factory directory if needed.
  // Returns true if any patches were (re)installed.
  bool installFactoryPatches() const;

  // Get all patches, optionally filtered by bank
  std::vector<PatchInfo> getPatches(Bank bank = Bank::All) const;

  // Get unique category names for a given bank
  std::vector<juce::String> getCategories(Bank bank = Bank::All) const;

  // Search patches by query string (matches name, author, description, tags)
  std::vector<PatchInfo> search(const juce::String &query,
                                Bank bank = Bank::All) const;

  // Filter patches by category
  std::vector<PatchInfo> getByCategory(const juce::String &category,
                                       Bank bank = Bank::All) const;

  // Resolved directory accessors
  static juce::File getFactoryPatchDirectory();
  static juce::File getUserPatchDirectory();

  int getCount() const { return (int)allPatches.size(); }

 private:
  void scanDirectory(const juce::File &dir, Bank bank,
                     const juce::String &rootPath);
  static PatchInfo parsePatchFile(const juce::File &file, Bank bank,
                                  const juce::String &rootPath);

  std::vector<PatchInfo> allPatches;
};
