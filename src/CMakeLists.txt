set(plugin_formats Standalone VST3)
if(APPLE)
    list(APPEND plugin_formats AU)
endif()

# Sources shared between the plugin and the test binary.
# GUI-free: every node, the graph engine, and core utilities.
set(ARPS_CORE_SOURCES
    AllNotesNode/AllNotesNode.cpp
    AlgorithmicModulatorNode/AlgorithmicModulatorNode.cpp
    AndNode/AndNode.cpp
    OrNode/OrNode.cpp
    XorNode/XorNode.cpp
    ChordNNode/ChordNNode.cpp
    ChordSplitNode/ChordSplitNode.cpp
    ClockManager.cpp
    ConcatenateNode/ConcatenateNode.cpp
    ConvergeNode/ConvergeNode.cpp
    DiagnosticNode/DiagnosticNode.cpp
    DivergeNode/DivergeNode.cpp
    EuclideanMath.cpp
    FoldNode/FoldNode.cpp
    GraphEngine.cpp
    GraphNode.cpp
    NoteExpressionManager.cpp
    MidiInNode/MidiInNode.cpp
    MidiOutNode/MidiOutNode.cpp
    MidiOutNode/MidiOutNodeEditor.cpp
    MultiplyNode/MultiplyNode.cpp
    OctaveStackNode/OctaveStackNode.cpp
    OctaveTransposeNode/OctaveTransposeNode.cpp
    InterleaveNode/InterleaveNode.cpp
    MpeFilterNode/MpeFilterNode.cpp
    VelocityFilterNode/VelocityFilterNode.cpp
    QuantizerNode/QuantizerNode.cpp
    ReverseNode/ReverseNode.cpp
    RouteNode/RouteNode.cpp
    SelectNode/SelectNode.cpp
    SequenceNode/SequenceNode.cpp
    SortNode/SortNode.cpp
    SplitNode/SplitNode.cpp
    SwitchNode/SwitchNode.cpp
    TextNoteNode/TextNoteNode.cpp
    TransposeNode/TransposeNode.cpp
    Tuning/ScalaParser.cpp
    Scales/ScaleLibrary.cpp
    Scales/HarmonicAnalysis.cpp
    Scales/ScaleEditorComponent.cpp
    UnfoldNode/UnfoldNode.cpp
    UnzipNode/UnzipNode.cpp
    WalkNode/WalkNode.cpp
    ZipNode/ZipNode.cpp)
list(TRANSFORM ARPS_CORE_SOURCES PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/")
set(ARPS_CORE_SOURCES ${ARPS_CORE_SOURCES} PARENT_SCOPE)

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
        ${ARPS_CORE_SOURCES}
        AlgorithmicModulatorNode/AlgorithmicModulatorNodeEditor.cpp
        AppSettings.cpp
        ArpsLookAndFeel.cpp
        GraphCanvas.cpp
        ModuleLibraryPanel.cpp
        NodeBlock.cpp
        PatchLibrary.cpp
        PatchBrowserPanel.cpp
        PluginEditor.cpp
        PluginProcessor.cpp
        SequenceNode/SequenceNodeEditor.cpp
        SettingsPanel.cpp
        TransportBar.cpp
        Tuning/TuningLibrary.cpp
        Tuning/TuningPanel.cpp
    PUBLIC
        NodeEditorPanel.cpp)

juce_add_binary_data(NodeLayoutData SOURCES
    AlgorithmicModulatorNode/AlgorithmicModulatorNode.json
    AndNode/AndNode.json
    OrNode/OrNode.json
    XorNode/XorNode.json
    AllNotesNode/AllNotesNode.json
    ChordNNode/ChordNNode.json
    ChordSplitNode/ChordSplitNode.json
    DiagnosticNode/DiagnosticNode.json
    FoldNode/FoldNode.json
    MidiInNode/MidiInNode.json
    MidiOutNode/MidiOutNode.json
    InterleaveNode/InterleaveNode.json
    MpeFilterNode/MpeFilterNode.json
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
    VelocityFilterNode/VelocityFilterNode.json
    WalkNode/WalkNode.json)

juce_add_binary_data(FactoryPatchData NAMESPACE FactoryPatches SOURCES
    ../patches/Factory/Init.euclidya
    ../patches/Factory/Tutorial/Tutorial_0.euclidya
    ../patches/Factory/Tutorial/Tutorial_1.euclidya
    ../patches/Factory/Tutorial/Tutorial_2.euclidya
    ../patches/Factory/Tutorial/Tutorial_3.euclidya
    ../patches/Factory/Tutorial/Tutorial_4.euclidya
    ../patches/Factory/Tutorial/Tutorial_5.euclidya
    ../patches/Factory/Tutorial/Tutorial_6.euclidya
    ../patches/Factory/Expressive/Gesture_Velocity_Ratchet.euclidya
    ../patches/Factory/Expressive/MPE_Filter_Dialogue.euclidya
    ../patches/Factory/Expressive/Pad_Pressure_Gate.euclidya
    ../patches/Factory/Expressive/Zip_MPE_Recombine.euclidya
    ../patches/Factory/Harmonic/And_Or_Logic.euclidya
    ../patches/Factory/Harmonic/Chord_Stack_Quantized.euclidya
    ../patches/Factory/Harmonic/Diverge_Spread.euclidya
    ../patches/Factory/Harmonic/Interleave_Counterpoint.euclidya
    ../patches/Factory/Melodic/Sequence_Fold_Return.euclidya
    ../patches/Factory/Melodic/Split_Dialogue.euclidya
    ../patches/Factory/Melodic/UpDown_vs_Interleave.euclidya
    ../patches/Factory/Melodic/Walk_Evolve.euclidya
    ../patches/Factory/Microtonal/19TET_Neutral_Triads.euclidya
    ../patches/Factory/Microtonal/31TET_Meantone_Sweep.euclidya
    ../patches/Factory/Microtonal/JI_5Limit_Pad.euclidya
    ../patches/Factory/Microtonal/Pythagorean_UpDown.euclidya
    ../patches/Factory/Rhythmic/Euclid_Basic.euclidya
    ../patches/Factory/Rhythmic/Polymeter_Split.euclidya
    ../patches/Factory/Rhythmic/Ratchet_Cascade.euclidya
    ../patches/Factory/Rhythmic/Shuffle_Humanize.euclidya)

juce_add_binary_data(FactoryScaleData NAMESPACE FactoryScales SOURCES
    ../resources/scales/12-TET/Lydian.json
    ../resources/scales/12-TET/Ionian.json
    ../resources/scales/12-TET/Mixolydian.json
    ../resources/scales/12-TET/Dorian.json
    ../resources/scales/12-TET/Aeolian.json
    ../resources/scales/12-TET/Phrygian.json
    ../resources/scales/12-TET/Locrian.json
    ../resources/scales/12-TET/Major.json
    ../resources/scales/12-TET/Minor.json
    ../resources/scales/12-TET/HarmonicMinor.json
    ../resources/scales/12-TET/MelodicMinor.json
    ../resources/scales/12-TET/MajorPentatonic.json
    ../resources/scales/12-TET/MinorPentatonic.json
    ../resources/scales/12-TET/WholeTone.json
    ../resources/scales/12-TET/Diminished.json
    ../resources/scales/12-TET/Blues.json
    ../resources/scales/12-TET/Chromatic.json)

juce_add_binary_data(FactoryTuningData NAMESPACE FactoryTunings SOURCES
    ../patches/FactoryTunings/19-tet.scl
    ../patches/FactoryTunings/22-tet.scl
    ../patches/FactoryTunings/31-tet.scl
    ../patches/FactoryTunings/53-tet.scl
    ../patches/FactoryTunings/pythagorean.scl
    ../patches/FactoryTunings/meantone_qc.scl
    ../patches/FactoryTunings/ji_5limit.scl)

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
        FactoryTuningData
        FactoryScaleData
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
