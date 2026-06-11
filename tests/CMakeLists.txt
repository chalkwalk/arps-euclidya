include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1)
FetchContent_MakeAvailable(Catch2)

# Prevent Catch2 headers from tripping -Werror on our strict flags.
get_target_property(_c2inc Catch2 INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(Catch2 PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_c2inc}")
get_target_property(_c2wm_inc Catch2WithMain INTERFACE_INCLUDE_DIRECTORIES)
set_target_properties(Catch2WithMain PROPERTIES
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_c2wm_inc}")

juce_add_console_app(ArpsTests PRODUCT_NAME "ArpsTests")

target_sources(ArpsTests PRIVATE
    ${ARPS_CORE_SOURCES}
    TestMain.cpp
    TestMpeCondition.cpp
    TestEuclideanMath.cpp
    TestMacroResolution.cpp
    TestNodeProcess.cpp
    TestGraphEngineTopology.cpp
    TestPortTypes.cpp)

target_include_directories(ArpsTests PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_compile_definitions(ArpsTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    ARPS_BUILD_TESTS=1
    ARPS_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/patches")

target_link_libraries(ArpsTests PRIVATE
    NodeLayoutData
    FactoryScaleData
    juce::juce_audio_utils
    Catch2::Catch2WithMain
    juce::juce_recommended_config_flags)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(ArpsTests PRIVATE
        -Wall -Wextra -Wpedantic
        -Wsign-conversion -Wfloat-conversion -Wshadow -Werror)
endif()

add_test(NAME ArpsUnitTests COMMAND ArpsTests)
