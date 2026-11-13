cmake_minimum_required(VERSION 3.24)
project(MdpTreeTest VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(JUCE_DIR "" CACHE PATH "JUCE checkout")
option(SANITIZE "Build with ASan (and UBSan where supported)" ON)
option(COVERAGE "Build with Clang source-based coverage" OFF)

if (NOT JUCE_DIR)
    message(FATAL_ERROR "Set JUCE_DIR")
endif ()

if (SANITIZE)
    if (MSVC)
        string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
        string(REGEX REPLACE "/RTC[1csu]+" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
        add_compile_options(/fsanitize=address /Zi)
        add_link_options(/INCREMENTAL:NO /DEBUG)
    else ()
        add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif ()
endif ()

if (COVERAGE)
    add_compile_options(-fprofile-instr-generate -fcoverage-mapping)
    add_link_options(-fprofile-instr-generate)
endif ()

add_subdirectory("${JUCE_DIR}" JUCE)
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/.." melatonin_inspector)

juce_add_gui_app(MdpTreeTest PRODUCT_NAME "MdpTreeTest")
target_sources(MdpTreeTest PRIVATE Main.cpp)
target_compile_definitions(MdpTreeTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
target_link_libraries(MdpTreeTest PRIVATE
    juce::juce_gui_basics
    Melatonin::Inspector
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
