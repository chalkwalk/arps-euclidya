# Contributing to Arps Euclidya

This document defines the operational procedures and priorities for contributing to the Arps Euclidya repository. All contributions are subject to the GPLv3 license.

## 1. Developmental Priorities

Arps Euclidya is a MIDI-only plugin. While all forms of contribution are accepted, current project needs are prioritized as follows:

1.  **Cross-DAW and Cross-OS Validation:** The existing testing baseline is primarily restricted to Linux (Bitwig, and occasionally Reaper). Verifying compilation, runtime stability, and state management on macOS and Windows, as well as across other DAWs, is the highest priority.
2.  **General Development:** UI/UX enhancements, the introduction of new module types, and general bug fixes are secondary priorities at this stage.

## 2. Issue Tracking and Proposals

* **Significant Changes:** Before beginning work on architectural modifications, large refactors, or new features, open an issue to propose and discuss the implementation strategy.
* **Trivial Changes:** Minor corrections, such as isolated bug fixes or documentation typos, do not require a preliminary issue and can be submitted directly.

## 3. Workflow

The project utilizes a standard Git workflow:

1.  Fork the repository.
2.  Create a targeted feature or bugfix branch.
3.  Commit changes with precise, descriptive commit messages.
4.  Submit a pull request targeting the primary branch.

## 4. Coding Standards

The project relies on strict compiler and tooling standards to maintain code quality:

* **Formatting:** We enforce a strict, automated formatting standard using `clang-format` (configured via `.clang-format` in the root directory). All C++ code must be formatted using this configuration before submission.
* **Linting:** We utilize `clang-tidy` (configured via `.clang-tidy` in the root directory) to enforce modern C++ practices and catch bugs. All code must pass these checks.
* Match the existing C++ and JUCE stylistic conventions present in the codebase beyond automated tooling.
* Adhere to the general principle of improving code readability and structure where modifications are made.

## 5. Testing Requirements

Automated unit and integration testing pipelines are not yet implemented. Contributors must execute the following manual verification protocol prior to opening a pull request:

1.  **Compilation & Strict Warnings:** Ensure the project compiles successfully. Note that when compiling with Clang, the project enforces strict scrutiny (including `-Wall`, `-Wextra`, `-Wpedantic`, `-Wsign-conversion`, `-Wfloat-conversion`, and `-Wshadow`) and treats all warnings as errors (`-Werror`). Your code must satisfy these checks.
2.  Manually verify the intended functionality using the Standalone build.
3.  Manually verify the intended functionality using at least one compiled plugin format. The CLAP architecture is the preferred test target, aligning with current project baselines.
