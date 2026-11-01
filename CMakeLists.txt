cmake_minimum_required(VERSION 3.22)
project(UnofficialAudrey VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
  # CI passes -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" for the universal build.
  set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
endif()

# Turn OFF to build only the headless engine test (no JUCE download).
option(AUDREY_BUILD_PLUGIN "Build the JUCE plugin" ON)

# --- DSP core: the Audrey engine + vendored DaisySP subset, no JUCE ---
add_library(audrey_core STATIC
  src/engine/FeedbackSynthEngine.cpp
  src/engine/KarplusString.cpp
  src/engine/BiquadFilters.cpp
  src/engine/DCBlock.cpp
  src/control/AudreyController.cpp
  third_party/daisysp/limiter.cpp
  third_party/daisysp/crossfade.cpp
  third_party/daisysp/overdrive.cpp
  third_party/daisysp/oscillator.cpp
  third_party/daisysp-lgpl/reverbsc.cpp
  third_party/daisysp-lgpl/tone.cpp)
target_include_directories(audrey_core PUBLIC src/engine src/control)
# tone.cpp (LGPL folder) includes "dsp.h" from the MIT folder
target_include_directories(audrey_core PRIVATE third_party/daisysp)
set_target_properties(audrey_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- Headless test:  cmake --build build --target engine_test && build/engine_test ---
add_executable(engine_test EXCLUDE_FROM_ALL test/engine_test.cpp)
target_link_libraries(engine_test PRIVATE audrey_core)

# Offline pitch measurement tool (renders one note to a raw float file)
add_executable(pitch_check EXCLUDE_FROM_ALL tools/pitch_check.cpp)
target_link_libraries(pitch_check PRIVATE audrey_core)

if(AUDREY_BUILD_PLUGIN)
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)

  # Plugin formats: AU + VST3 (+ Standalone for quick testing).
  set(FORMATS VST3 Standalone)
  if(APPLE)
    list(APPEND FORMATS AU)
  endif()

  juce_add_plugin(UnofficialAudrey
    COMPANY_NAME "particledots"
    PLUGIN_MANUFACTURER_CODE Ptdt
    PLUGIN_CODE Audr
    FORMATS ${FORMATS}
    PRODUCT_NAME "Unofficial Audrey"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    AU_MAIN_TYPE kAudioUnitType_MusicEffect   # Logic offers it as a "MIDI-controlled effect"
    COPY_PLUGIN_AFTER_BUILD TRUE)

  target_sources(UnofficialAudrey PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
  target_compile_definitions(UnofficialAudrey PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
  target_link_libraries(UnofficialAudrey PRIVATE
    audrey_core
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
endif()
