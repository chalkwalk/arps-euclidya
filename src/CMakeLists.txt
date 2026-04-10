set(plugin_formats Standalone VST3)
if(APPLE)
    list(APPEND plugin_formats AU)
endif()

juce_add_plugin(ArpsEuclidya
    COMPANY_NAME "ChalkWalkMusic"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    NAME_PREFIX "Arps"
    VERSION "0.1.0"
    JUCER_VERSION "7.0.5"
    PRODUCT_NAME "Arps Euclidya"
    BUNDLE_ID "com.ChalkWalkMusic.ArpsEuclidya"
    COPYRIGHT "Copyright (c) 2026 ChalkWalkMusic"
    MICROPHONE_PERMISSION_ENABLED FALSE
    FORMATS ${plugin_formats})

juce_generate_juce_header(ArpsEuclidya)

target_sources(ArpsEuclidya
    PRIVATE
        AllNotesNode/AllNotesNode.cpp
        AppSettings.cpp
        ArpsLookAndFeel.cpp
        ChordNNode/ChordNNode.cpp
        ChordSplitNode/ChordSplitNode.cpp
        ClockManager.cpp
        ConcatenateNode/ConcatenateNode.cpp
        ConvergeNode/ConvergeNode.cpp
        DiagnosticNode/DiagnosticNode.cpp
        DivergeNode/DivergeNode.cpp
        EuclideanMath.cpp
        FoldNode/FoldNode.cpp
        GraphCanvas.cpp
        GraphEngine.cpp
        GraphNode.cpp
        NoteExpressionManager.cpp
        MidiInNode/MidiInNode.cpp
        MidiOutNode/MidiOutNode.cpp
        MidiOutNode/MidiOutNodeEditor.cpp
        ModuleLibraryPanel.cpp
        MultiplyNode/MultiplyNode.cpp
        NodeBlock.cpp
        OctaveStackNode/OctaveStackNode.cpp
        OctaveTransposeNode/OctaveTransposeNode.cpp
        PatchLibrary.cpp
        PatchBrowserPanel.cpp
        PluginEditor.cpp
        PluginProcessor.cpp
        QuantizerNode/QuantizerNode.cpp
        ReverseNode/ReverseNode.cpp
        RouteNode/RouteNode.cpp
        SelectNode/SelectNode.cpp
        SequenceNode/SequenceNode.cpp
        SettingsPanel.cpp
        SortNode/SortNode.cpp
        SplitNode/SplitNode.cpp
        SwitchNode/SwitchNode.cpp
        TextNoteNode/TextNoteNode.cpp
        TransportBar.cpp
        TransposeNode/TransposeNode.cpp
        UnfoldNode/UnfoldNode.cpp
        UnzipNode/UnzipNode.cpp
        WalkNode/WalkNode.cpp
        ZipNode/ZipNode.cpp
    PUBLIC
        NodeEditorPanel.cpp)

juce_add_binary_data(NodeLayoutData SOURCES
    AllNotesNode/AllNotesNode.json
    ChordNNode/ChordNNode.json
    ChordSplitNode/ChordSplitNode.json
    DiagnosticNode/DiagnosticNode.json
    FoldNode/FoldNode.json
    MidiInNode/MidiInNode.json
    MidiOutNode/MidiOutNode.json
    MultiplyNode/MultiplyNode.json
    OctaveStackNode/OctaveStackNode.json
    OctaveTransposeNode/OctaveTransposeNode.json
    QuantizerNode/QuantizerNode.json
    RouteNode/RouteNode.json
    SelectNode/SelectNode.json
    SequenceNode/SequenceNode.json
    SplitNode/SplitNode.json
    SwitchNode/SwitchNode.json
    TransposeNode/TransposeNode.json
    UnfoldNode/UnfoldNode.json
    WalkNode/WalkNode.json)

juce_add_binary_data(FactoryPatchData NAMESPACE FactoryPatches SOURCES
    ../patches/Factory/Init.euclidya
    ../patches/Factory/Tutorial/Tutorial_0.euclidya
    ../patches/Factory/Tutorial/Tutorial_1.euclidya
    ../patches/Factory/Tutorial/Tutorial_2.euclidya
    ../patches/Factory/Tutorial/Tutorial_3.euclidya
    ../patches/Factory/Tutorial/Tutorial_4.euclidya
    ../patches/Factory/Tutorial/Tutorial_5.euclidya
    ../patches/Factory/Tutorial/Tutorial_6.euclidya)

clap_juce_extensions_plugin(TARGET ArpsEuclidya CLAP_ID "com.ChalkWalkMusic.ArpsEuclidya" CLAP_FEATURES "note-effect" "tool")

target_compile_definitions(ArpsEuclidya
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ArpsEuclidya
    PRIVATE
        NodeLayoutData
        FactoryPatchData
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(ArpsEuclidya PRIVATE
        -Wall -Wextra -Wpedantic
        -Wsign-conversion -Wfloat-conversion -Wshadow
        -Werror)
else()
    target_link_libraries(ArpsEuclidya PUBLIC juce::juce_recommended_warning_flags)
endif()
