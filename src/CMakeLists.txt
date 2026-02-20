
juce_add_plugin(EuclideanArp
    COMPANY_NAME "ChalkWalkMusic"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    PLUGIN_MANUFACTURER_CODE Mnty
    PLUGIN_CODE EARP
    FORMATS VST3 Standalone
    PRODUCT_NAME "Euclidean Arp")

juce_generate_juce_header(EuclideanArp)

target_sources(EuclideanArp
    PRIVATE
        PluginProcessor.cpp
        PluginEditor.cpp
        GraphNode.cpp
        GraphEngine.cpp
        SortNode.cpp
        ReverseNode.cpp
        MidiHandler.cpp
        EuclideanMath.cpp
        ClockManager.cpp
        MidiInNode.cpp
        MidiOutNode.cpp
        MidiOutNodeEditor.cpp
        ConvergeNode.cpp
        DivergeNode.cpp
        ChordNNode.cpp
        WalkNode.cpp
        OctaveStackNode.cpp
        TransposeNode.cpp
        OctaveTransposeNode.cpp
        FoldNode.cpp
        UnfoldNode.cpp
        QuantizerNode.cpp
        AllNotesNode.cpp
        SplitNode.cpp
        ConcatenateNode.cpp
        ChordSplitNode.cpp
        MultiplyNode.cpp
        NodeBlock.cpp
        GraphCanvas.cpp
        EuclideanLookAndFeel.cpp
    PUBLIC    NodeEditorPanel.cpp)

clap_juce_extensions_plugin(TARGET EuclideanArp CLAP_ID "com.ChalkWalkMusic.EuclideanArp" CLAP_FEATURES "note-effect" "tool")

target_compile_definitions(EuclideanArp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(EuclideanArp
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
