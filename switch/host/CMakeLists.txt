# Linux test harness for the Switch build (see host_main.cpp). Not shipped.
# cmake -S switch/host -B switch/host/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake_minimum_required(VERSION 3.24)
project(rs64host LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)

set(NX ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(ROOT ${NX}/..)

# Host stand-ins first (switch.h), then the Switch shims recompui/ and recompinput/ (not its dlfcn.h or sys/).
file(COPY ${NX}/shim/recompui ${NX}/shim/recompinput DESTINATION ${CMAKE_BINARY_DIR}/nxshim)
include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/shim ${CMAKE_BINARY_DIR}/nxshim)
set(FMT_OS OFF CACHE BOOL "" FORCE)
# As the Switch link does: unused sections (PC-only paths that reference RT64 and the touch UI) are dropped.
add_compile_options(-ffunction-sections -fdata-sections)

add_subdirectory(${ROOT}/lib/N64ModernRuntime ${CMAKE_BINARY_DIR}/N64ModernRuntime)
find_package(SDL2 REQUIRED)

set(RECOMP_INCLUDES
    ${ROOT}/include
    ${ROOT}/lib/N64ModernRuntime/ultramodern/include
    ${ROOT}/lib/N64ModernRuntime/librecomp/include
    ${ROOT}/lib/N64ModernRuntime/N64Recomp/include
)
set(RECOMP_C_FLAGS
    -fno-strict-aliasing
    -falign-functions=16
    -Wno-unused-variable
    $<$<COMPILE_LANGUAGE:C>:-Wno-implicit-function-declaration>
    $<$<COMPILE_LANGUAGE:C>:-Wno-int-conversion>
    $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-pointer-types>
)

file(GLOB FUNC_SOURCES ${ROOT}/RecompiledFuncs/*.c ${ROOT}/RecompiledFuncs/*.cpp)
add_library(RecompiledFuncs STATIC ${FUNC_SOURCES} ${ROOT}/src/game/toml_hooks.c)
target_include_directories(RecompiledFuncs PRIVATE ${RECOMP_INCLUDES} ${ROOT}/src)
target_compile_options(RecompiledFuncs PRIVATE ${RECOMP_C_FLAGS} -include ${ROOT}/include/mem_wu.h)
target_compile_definitions(RecompiledFuncs PRIVATE RECOMP_FUNC_WEAK main=rs_main)

add_library(PatchesLib OBJECT ${ROOT}/RecompiledPatches/patches.c ${ROOT}/RecompiledPatches/patches_bin.c)
target_include_directories(PatchesLib PRIVATE ${RECOMP_INCLUDES})
target_compile_options(PatchesLib PRIVATE ${RECOMP_C_FLAGS} -include ${ROOT}/include/mem_wu.h)

set(UCODE_SOURCES ${ROOT}/build/factor5_ucode/factor5_boot_recompiled.c ${ROOT}/build/factor5_ucode/musyx_audio_recompiled.c)
set_source_files_properties(${UCODE_SOURCES} PROPERTIES LANGUAGE CXX COMPILE_OPTIONS "-Wno-unused-variable;-fpermissive;-msse4.1")

# GLideN64 as on Switch (EGL GL loading), with the portable C paths instead of the NEON ones.
set(GLIDEN64_DIR ${ROOT}/lib/GLideN64/src)
include(${NX}/gliden64_sources.cmake)
add_library(GLideN64 STATIC ${GLIDEN64_SOURCES} ${NX}/gliden64_rt64ex.cpp)
target_include_directories(GLideN64 PRIVATE ${GLIDEN64_DIR} ${GLIDEN64_DIR}/inc ${GLIDEN64_DIR}/osal ${NX}/gliden64)
target_compile_definitions(GLideN64 PRIVATE MUPENPLUSAPI TXFILTER_LIB OS_LINUX EGL NDEBUG)
set_target_properties(GLideN64 PROPERTIES CXX_STANDARD 17)
target_compile_options(GLideN64 PRIVATE -Wno-narrowing $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>)

file(GLOB RS64_GAME_SOURCES ${ROOT}/src/game/*.cpp ${ROOT}/src/host_api/*.cpp ${ROOT}/src/lockstep/*.cpp)
add_executable(rs64host
    ${RS64_GAME_SOURCES}
    ${ROOT}/src/audio/audio.cpp
    ${ROOT}/src/video/video_config.cpp
    ${ROOT}/src/input/touch_config.cpp
    ${UCODE_SOURCES}
    $<TARGET_OBJECTS:PatchesLib>
    ${NX}/rs64_stubs.cpp
    ${NX}/core_switch.cpp
    ${NX}/render_gliden64.cpp
    ${NX}/nx_settings.cpp
    ${NX}/nx_cheats.cpp
    host_main.cpp
    host_support.cpp
)
target_include_directories(rs64host PRIVATE
    ${RECOMP_INCLUDES}
    ${ROOT}/lib/N64ModernRuntime/thirdparty/sse2neon
    ${ROOT}/lib/GLideN64/src/inc
    ${ROOT}/src
    ${ROOT}/RecompiledFuncs
    ${ROOT}/RecompiledPatches
    ${ROOT}/lib/rt64/src
    ${ROOT}/lib/N64ModernRuntime/thirdparty
    ${NX}
)
target_compile_definitions(rs64host PRIVATE CVNX_HOST _GNU_SOURCE RS64_VERSION_STRING="host" NX_SETTINGS_PATH="host_data/nx_options.txt" NX_CHEATS_DIR="host_data")
target_compile_options(rs64host PRIVATE -fno-strict-aliasing -fms-extensions)
target_link_options(rs64host PRIVATE -Wl,--allow-multiple-definition -Wl,--gc-sections)
target_link_libraries(rs64host PRIVATE
    -Wl,--start-group RecompiledFuncs librecomp ultramodern -Wl,--end-group
    GLideN64 SDL2::SDL2 EGL GL pthread dl
)
