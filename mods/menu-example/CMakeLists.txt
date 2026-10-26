# Standalone build for the menu-example mod's DLL. Independent of the main game
# build so the game never churns on it:
#
#   cmake -S mods/menu-example -B mods/menu-example/build
#   cmake --build mods/menu-example/build
#
# Produces menu_example.dll next to mod.json (referenced by roguesq_menu.json's
# @exports). To use the mod, copy this whole folder into the game's mods/ directory.
cmake_minimum_required(VERSION 3.20)
project(menu_example_mod LANGUAGES CXX)

add_library(menu_example SHARED menu_example.cpp)
# The trailing $<0:> keeps multi-config generators (Visual Studio) from appending a
# per-config subfolder, so the DLL lands directly beside mod.json.
set_target_properties(menu_example PROPERTIES
    PREFIX ""
    OUTPUT_NAME "menu_example"
    CXX_STANDARD 17
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/$<0:>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/$<0:>")

# Static CRT so the DLL is self-contained (LoadLibraryEx uses a restricted search
# that won't find the CRT DLLs otherwise).
if (MSVC)
    set_property(TARGET menu_example PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
