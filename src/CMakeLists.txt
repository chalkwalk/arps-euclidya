
juce_add_plugin(EuclideanArp
    COMPANY_NAME "ChalkWalkMusic"
    IS_SYNTH FALSE
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
        GraphNode.cpp)

target_compile_definitions(EuclideanArp
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(EuclideanArp
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
