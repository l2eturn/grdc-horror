cmake_minimum_required(VERSION 3.20)
# run on both Apple Silicon and Intel Macs, macOS 11 and newer (must come before project())
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
project(grdc_horror LANGUAGES C CXX OBJCXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_subdirectory(../terEngine ${CMAKE_BINARY_DIR}/terEngine)

# assets/ (including assets/data/*.json) is copied into the app; load files with ter::assetPath("data/map.json")
ter_add_game(horror
  SOURCES src/main.cpp src/World.cpp src/Props.cpp src/Environment.cpp src/Trees.cpp src/Phone.cpp src/PhoneScreen.cpp src/GameClock.cpp src/Inbox.cpp
          src/MeshKit.cpp src/Textures.cpp src/Sounds.cpp src/Animator.cpp src/Dialogue.cpp src/Bus.cpp src/GrandmaEvent.cpp src/Ending.cpp src/Breath.cpp src/Hiding.cpp src/Searcher.cpp src/RuleSystem.cpp src/Companion.cpp src/Playtest.cpp
  ASSETS  assets
  NAME    "grdc-horror"
  ID      com.terx13.grdchorror)
