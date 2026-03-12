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
    FORMATS Standalone VST3)

juce_generate_juce_header(ArpsEuclidya)

target_sources(ArpsEuclidya
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        MidiInNode.cpp
        MidiOutNode.cpp
        ClockManager.cpp
        GraphNode.cpp
        GraphEngine.cpp
        SortNode.cpp
        ReverseNode.cpp
        MidiHandler.cpp
        EuclideanMath.cpp
        TransposeNode.cpp
        FoldNode.cpp
        UnfoldNode.cpp
        QuantizerNode.cpp
        ChordNNode.cpp
        OctaveStackNode.cpp
        MultiplyNode.cpp
        ConvergeNode.cpp
        DivergeNode.cpp
        WalkNode.cpp
        ZipNode.cpp
        UnzipNode.cpp
        RouteNode.cpp
        SelectNode.cpp
        SwitchNode.cpp
        ChordSplitNode.cpp
        SequenceNode.cpp
        OctaveTransposeNode.cpp
        MultiplyNode.cpp
        AllNotesNode.cpp
        SplitNode.cpp
        ConcatenateNode.cpp
        MidiOutNodeEditor.cpp
        ModuleLibraryPanel.cpp
        NodeBlock.cpp
        GraphCanvas.cpp
        ArpsLookAndFeel.cpp
    PUBLIC    NodeEditorPanel.cpp)

clap_juce_extensions_plugin(TARGET ArpsEuclidya CLAP_ID "com.ChalkWalkMusic.ArpsEuclidya" CLAP_FEATURES "note-effect" "tool")

target_compile_definitions(ArpsEuclidya
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ArpsEuclidya
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
