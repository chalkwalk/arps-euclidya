# ---------------------------------------------------------------------------
# THE PROMOTION FLOOR: does what is leaving still compile as C++17?
#
# chalkwalk-music has a C++17 floor, and this tree is C++20. JUCE drags in most
# of the standard library, so a header on its way out can take a C++20 feature
# -- or, more insidiously, merely stop including what it uses and reach it
# transitively through a C++20 standard header -- and still compile here and
# still pass here. It fails at the moment somebody
# tries to move it, attached to whoever is doing the extraction rather than to
# whoever introduced it.
#
# THIS IS NOT HYPOTHETICAL. It was found for real in a sibling project:
# `TapeNoise.h` used `std::clamp` with no `<algorithm>`, reached it through a
# C++20 header, built clean for months, and surfaced only when the promotion
# compiled it at 17.
#
# Silent, late, and invisible to the build we actually ship -- which is what
# makes it worth a test rather than a convention. The set it guards is declared
# in `PromotionSet.cmake`; read that first, because the interesting question
# here is which files are leaving, not how they are compiled.
#
# Each header is compiled ALONE, which also asserts that each is
# self-contained: a header that only works because something else was included
# first is the other half of the same bug, and it costs nothing to catch here.
# ---------------------------------------------------------------------------

if(NOT DEFINED SRC_DIR)
    message(FATAL_ERROR "SRC_DIR must be set")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/PromotionSet.cmake")

set(work "${CMAKE_CURRENT_BINARY_DIR}/promotion-readiness")
file(MAKE_DIRECTORY "${work}")

separate_arguments(include_list UNIX_COMMAND "${EXTRA_INCLUDES}")
set(include_flags "")
foreach(dir IN LISTS include_list)
    list(APPEND include_flags -I "${dir}")
endforeach()

set(failures "")
set(checked 0)

foreach(name IN LISTS ARPS_PROMOTION_SET)
    set(header "${SRC_DIR}/${name}")
    if(NOT EXISTS "${header}")
        message(FATAL_ERROR "guarded file is missing: ${header}")
    endif()

    file(WRITE "${work}/tu.cpp" "#include \"${header}\"\nint main() { return 0; }\n")

    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -std=c++17 -fsyntax-only
                -I "${SRC_DIR}" ${include_flags} "${work}/tu.cpp"
        RESULT_VARIABLE rc
        OUTPUT_VARIABLE out
        ERROR_VARIABLE err)

    math(EXPR checked "${checked} + 1")

    if(NOT rc EQUAL 0)
        # The first error only; the cascade after it names the wrong file.
        string(REGEX MATCH "[^\n]*error:[^\n]*" first "${err}")
        list(APPEND failures "  ${name}\n      ${first}")
    endif()
endforeach()

if(failures)
    string(REPLACE ";" "\n" report "${failures}")
    message(FATAL_ERROR
        "\nThese no longer compile as C++17, so they cannot be promoted into a\n"
        "chalkwalk-* library (floor: C++17):\n\n"
        "${report}\n\n"
        "Fix the header, not this test. If a C++20 feature is genuinely needed,\n"
        "the file is not going anywhere and belongs off the list in\n"
        "cmake/PromotionSet.cmake -- which is a decision, not a build fix.\n")
endif()

message(STATUS "promotion-readiness: ${checked} guarded headers compile as C++17")
