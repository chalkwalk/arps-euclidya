# ---------------------------------------------------------------------------
# Where the shared libraries come from.
#
# Unset, nothing changes: this repository's submodules are used and a fresh
# clone builds with no extra steps. The submodule stays the source of truth for
# WHICH commit this project wants.
#
# CHALKWALK_TAPE_DIR, CHALKWALK_DSP_DIR -- cache variables or environment
# variables -- point at working checkouts instead, which is what makes a change
# to a library testable HERE without a commit and without a push:
#
#     cmake -B build -DCHALKWALK_TAPE_DIR=$HOME/Programming/chalkwalk-tape
#
# Remanence then compiles that working tree directly. Edit there, rebuild here,
# run the suite; no round trip through GitHub. That matters most for
# chalkwalk-tape, which this project is the second consumer of and which will
# receive most of what `src/tape/` grows (DESIGN.md section 13): a change to the
# medium or the resampler wants a real machine's tests run against it before it
# is committed anywhere.
#
# THE SUBMODULE SHA NO LONGER DESCRIBES WHAT YOU BUILT while one of these is
# set, which is the whole cost of it. CI must not use them, and neither should
# anything whose result is meant to be attributable -- the solver and
# oversampling bench measurements above all (DESIGN.md section 4.2), since a
# number that cannot name the commit that produced it is not a measurement. Use
# an override to iterate; bump the submodule and re-verify before calling
# anything done.
#
# Same shape as the sibling projects', deliberately: one pattern across the
# ecosystem is worth more than a better one used in one place.
# ---------------------------------------------------------------------------
function(chalkwalk_add_library name submodule_path)
    # Already added by a parent, so use theirs.
    #
    # These libraries nest and they overlap: chalkwalk-tape and chalkwalk-dsp
    # may each be pulled in by something else in the tree. Adding a second copy
    # is not a version conflict -- it is a duplicate CMake target name, which
    # fails the configure outright.
    #
    # Whichever project adds it first wins and the rest reuse it, which is the
    # same rule chalkwalk-tape applies to its vendored signalsmith-dsp. It also
    # means the OUTER project's submodule SHA is the one that describes the
    # build, and the inner one is not consulted at all.
    if(TARGET chalkwalk_${name})
        message(STATUS "chalkwalk-${name}: already provided by a parent project")
        return()
    endif()

    string(TOUPPER "${name}" upper)
    set(var "CHALKWALK_${upper}_DIR")

    if(NOT ${var} AND DEFINED ENV{${var}})
        set(${var} "$ENV{${var}}")
    endif()
    set(${var} "${${var}}" CACHE PATH
        "Working checkout of chalkwalk-${name}; empty means use this repository's own submodule")

    if(${var})
        if(NOT EXISTS "${${var}}/CMakeLists.txt")
            message(FATAL_ERROR
                "${var} is set to '${${var}}' but there is no chalkwalk-${name} "
                "there. Point it at a checkout, or unset it to use the submodule.")
        endif()
        set(root "${${var}}")
        message(STATUS
            "chalkwalk-${name}: OVERRIDE at ${root} "
            "(the submodule SHA does not describe this build)")
    else()
        set(root "${CMAKE_CURRENT_SOURCE_DIR}/${submodule_path}")
        if(NOT EXISTS "${root}/CMakeLists.txt")
            message(FATAL_ERROR
                "No chalkwalk-${name}.\n"
                "  This repository's ${submodule_path} submodule is not checked "
                "out, and ${var} is not set. Either:\n"
                "    git submodule update --init --recursive\n"
                "  or point at a working checkout:\n"
                "    cmake -B build -D${var}=/path/to/chalkwalk-${name}")
        endif()
    endif()

    # Where the library actually came from, for anything that needs a PATH
    # rather than a target -- the promotion-readiness check compiles headers
    # with a bare compiler invocation and cannot ask a target for its includes.
    # Exported here so it survives an override, which is exactly when a
    # hard-coded libs/<name> would be pointing at the wrong tree.
    set(CHALKWALK_${upper}_ROOT "${root}" PARENT_SCOPE)

    # Each library's own suite runs inside this project's ctest, so Remanence
    # verifies its dependencies rather than assuming them.
    set(CHALKWALK_${upper}_TESTS ON CACHE BOOL "" FORCE)
    add_subdirectory("${root}" "${CMAKE_BINARY_DIR}/libs/${name}")
endfunction()
