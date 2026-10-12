# terEngine — a small C++20 / OpenGL 4.1 game library for macOS (three.js-style scene graph).
#
# Use it from a game:
#   add_subdirectory(terEngine)            # or FetchContent
#   target_link_libraries(mygame PRIVATE ter::engine)
cmake_minimum_required(VERSION 3.20)
project(terEngine VERSION 0.1.0 LANGUAGES C CXX OBJCXX)

if(NOT APPLE)
  message(FATAL_ERROR "terEngine currently builds on macOS only (ImageIO / CoreText / OpenGL 4.1)")
endif()

option(TER_BUILD_EXAMPLES "Build the terEngine examples" ${PROJECT_IS_TOP_LEVEL})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(PROJECT_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(FetchContent)
# cgltf / meshoptimizer / miniaudio are only unpacked (we compile the parts we need), not added as projects
if(POLICY CMP0169)
  cmake_policy(SET CMP0169 OLD)
endif()

# math (MIT)
FetchContent_Declare(glm
  URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
# glTF 2.0 parser, single header (MIT)
FetchContent_Declare(cgltf
  URL https://github.com/jkuhlmann/cgltf/archive/refs/tags/v1.14.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
# EXT_meshopt_compression decoder for .glb files (MIT)
FetchContent_Declare(meshoptimizer
  URL https://github.com/zeux/meshoptimizer/archive/refs/tags/v0.21.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
# JSON (MIT) — glTF extensions, and handy for game data
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
# audio output + wav / mp3 / flac decoding (public domain / MIT-0)
FetchContent_Declare(miniaudio
  URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.21.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

FetchContent_MakeAvailable(glm json)
FetchContent_Populate(cgltf)
FetchContent_Populate(meshoptimizer)
FetchContent_Populate(miniaudio)

# window + input: use Homebrew's GLFW when installed, otherwise build it from source
find_package(glfw3 3.4 QUIET)
if(NOT glfw3_FOUND)
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(glfw)
endif()
find_package(OpenGL REQUIRED)

include(cmake/EmbedShaders.cmake)
ter_embed_shaders(${CMAKE_CURRENT_BINARY_DIR}/ter_builtin_shaders.cpp)

file(GLOB_RECURSE TER_SOURCES CONFIGURE_DEPENDS ter/*.cpp ter/*.mm)
add_library(terEngine STATIC
  ${TER_SOURCES}
  ${CMAKE_CURRENT_BINARY_DIR}/ter_builtin_shaders.cpp
  ${meshoptimizer_SOURCE_DIR}/src/vertexcodec.cpp
  ${meshoptimizer_SOURCE_DIR}/src/indexcodec.cpp
  ${meshoptimizer_SOURCE_DIR}/src/vertexfilter.cpp)
add_library(ter::engine ALIAS terEngine)

target_include_directories(terEngine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(terEngine SYSTEM PRIVATE
  ${cgltf_SOURCE_DIR} ${meshoptimizer_SOURCE_DIR}/src ${miniaudio_SOURCE_DIR})
target_link_libraries(terEngine
  PUBLIC glfw glm::glm OpenGL::GL nlohmann_json::nlohmann_json
  PRIVATE "-framework CoreFoundation" "-framework CoreGraphics" "-framework CoreText" "-framework ImageIO"
          "-framework CoreAudio" "-framework AudioToolbox" "-framework Foundation")
target_compile_definitions(terEngine
  PUBLIC GLFW_INCLUDE_NONE GL_SILENCE_DEPRECATION
  PRIVATE TER_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fonts")
target_compile_options(terEngine PRIVATE $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wall -Wextra -Wno-missing-field-initializers>)
set_source_files_properties(
  ${meshoptimizer_SOURCE_DIR}/src/vertexcodec.cpp
  ${meshoptimizer_SOURCE_DIR}/src/indexcodec.cpp
  ${meshoptimizer_SOURCE_DIR}/src/vertexfilter.cpp
  ter/audio/audio.cpp
  PROPERTIES COMPILE_OPTIONS "-w")
set_source_files_properties(ter/gfx/image.mm ter/ui/canvas2d.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")

if(TER_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
