# ---------------------------------------------------------------------------
# Where JUCE comes from (../ECOSYSTEM.md).
#
# JUCE is 94 MB of working tree and four plugins in this ecosystem pin the same
# commit, so four checkouts is 376 MB of the same files. CHALKWALK_JUCE_DIR --
# a cache variable or an environment variable -- points at one shared checkout
# instead.
#
# Unset, nothing changes: this repository's own JUCE submodule is used and a
# fresh clone builds with no extra steps. That is deliberate. The submodule
# stays the source of truth for WHICH commit this project wants, and sharing is
# an optimisation a developer opts into, not a dependency.
#
# To use it, and free the local checkout:
#
#     cmake -B build -DCHALKWALK_JUCE_DIR=$HOME/Programming/.juce/JUCE
#     git submodule deinit JUCE          # the pin stays recorded in git
#
# THE SHARED CHECKOUT CARRIES THE UNION OF EVERY PROJECT'S JUCE PATCHES. They
# touch disjoint files today, so the union is well defined -- but it does mean
# building against patches another plugin needed. That coupling is the price of
# one checkout and it is accepted knowingly; see ECOSYSTEM.md.
# ---------------------------------------------------------------------------
if(NOT CHALKWALK_JUCE_DIR AND DEFINED ENV{CHALKWALK_JUCE_DIR})
    set(CHALKWALK_JUCE_DIR "$ENV{CHALKWALK_JUCE_DIR}")
endif()
set(CHALKWALK_JUCE_DIR "${CHALKWALK_JUCE_DIR}" CACHE PATH
    "Shared JUCE checkout; empty means use this repository's own submodule")

if(CHALKWALK_JUCE_DIR)
    if(NOT EXISTS "${CHALKWALK_JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR
            "CHALKWALK_JUCE_DIR is set to '${CHALKWALK_JUCE_DIR}' but there is no "
            "JUCE there. Point it at a JUCE checkout, or unset it to use the "
            "submodule.")
    endif()
    set(CHALKWALK_JUCE_ROOT "${CHALKWALK_JUCE_DIR}")
    message(STATUS "JUCE: shared checkout at ${CHALKWALK_JUCE_ROOT}")
else()
    set(CHALKWALK_JUCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
    # CHALKWALK_JUCE_OPTIONAL: set by a project that can do something useful
    # with no JUCE at all -- Anvil builds and tests its physical core that way,
    # and turning that into a hard error would destroy the boundary it exists
    # to prove. Such a project checks CHALKWALK_JUCE_ROOT itself.
    if(NOT EXISTS "${CHALKWALK_JUCE_ROOT}/CMakeLists.txt" AND NOT CHALKWALK_JUCE_OPTIONAL)
        message(FATAL_ERROR
            "No JUCE.\n"
            "  This repository's JUCE submodule is not checked out, and "
            "CHALKWALK_JUCE_DIR is not set. Either:\n"
            "    git submodule update --init --recursive\n"
            "  or point at a shared checkout:\n"
            "    cmake -B build -DCHALKWALK_JUCE_DIR=/path/to/JUCE\n"
            "  See ECOSYSTEM.md. Without this the failure is a bare "
            "add_subdirectory error that says nothing about either option.")
    endif()
endif()
