# ter_add_game(): build a game as a self-contained macOS app you can hand to other people.
#
#   ter_add_game(mygame
#     SOURCES src/main.cpp                 # required
#     ASSETS  assets                       # folder copied to <App>.app/Contents/Resources/assets
#     NAME    "Find the Box"               # app / window-menu name (default: the target name)
#     ID      com.yourname.findthebox      # bundle identifier
#     VERSION 1.0
#     ICON    icon.icns)
#
# Creates:
#   mygame       -> build/<NAME>.app          (run it: open "build/<NAME>.app")
#   mygame_zip   -> build/<NAME>-macOS.zip    (ad-hoc signed, ready to upload)
#
# At runtime, load files with ter::assetPath("sfx/door.wav"); the engine's fonts are copied into the
# bundle too, so nothing points back at the machine that built the game.

function(ter_add_game target)
  cmake_parse_arguments(G "" "ASSETS;NAME;ID;VERSION;ICON" "SOURCES" ${ARGN})
  # this file's folder (the caller's scope does not see terEngine's variables)
  set(TER_GAME_CMAKE_DIR "${CMAKE_CURRENT_FUNCTION_LIST_DIR}")
  if(NOT G_SOURCES)
    message(FATAL_ERROR "ter_add_game(${target}): SOURCES is required")
  endif()
  if(NOT G_NAME)
    set(G_NAME ${target})
  endif()
  if(NOT G_ID)
    string(REGEX REPLACE "[^A-Za-z0-9.-]" "-" _id "${target}")
    set(G_ID "io.terengine.${_id}")
  endif()
  if(NOT G_VERSION)
    set(G_VERSION 1.0)
  endif()

  set(_extra)
  if(G_ICON)
    get_filename_component(_icon "${G_ICON}" ABSOLUTE)
    get_filename_component(_icon_name "${_icon}" NAME)
    set_source_files_properties("${_icon}" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    list(APPEND _extra "${_icon}")
  endif()

  add_executable(${target} MACOSX_BUNDLE ${G_SOURCES} ${_extra})
  target_link_libraries(${target} PRIVATE ter::engine)
  set_target_properties(${target} PROPERTIES
    OUTPUT_NAME "${G_NAME}"
    MACOSX_BUNDLE_INFO_PLIST "${TER_GAME_CMAKE_DIR}/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "${G_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "${G_ID}"
    MACOSX_BUNDLE_BUNDLE_VERSION "${G_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${G_VERSION}"
    MACOSX_BUNDLE_ICON_FILE "${_icon_name}")

  # Copy fonts and assets into the bundle on every build (cheap when nothing changed), so editing
  # a sound or texture only needs `cmake --build`, not a relink.
  if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
    set(_copy copy_directory_if_different)
  else()
    set(_copy copy_directory)
  endif()
  set(_res "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources")
  set(_cmds COMMAND ${CMAKE_COMMAND} -E ${_copy} "${TER_GAME_CMAKE_DIR}/../fonts" "${_res}/fonts")
  if(G_ASSETS)
    get_filename_component(_assets "${G_ASSETS}" ABSOLUTE)
    list(APPEND _cmds COMMAND ${CMAKE_COMMAND} -E ${_copy} "${_assets}" "${_res}/assets")
  endif()
  add_custom_target(${target}_resources ALL ${_cmds} VERBATIM)
  add_dependencies(${target}_resources ${target})

  # Upload-ready zip. Ad-hoc signing (`-s -`) is required for Apple Silicon to launch the app at all;
  # without a paid Developer ID players still have to approve it once (see docs/shipping.md).
  set(_zip "${CMAKE_BINARY_DIR}/${G_NAME}-macOS.zip")
  add_custom_target(${target}_zip
    COMMAND codesign --force --deep --sign - "$<TARGET_BUNDLE_DIR:${target}>"
    COMMAND ${CMAKE_COMMAND} -E rm -f "${_zip}"
    COMMAND ditto -c -k --keepParent "$<TARGET_BUNDLE_DIR:${target}>" "${_zip}"
    COMMAND ${CMAKE_COMMAND} -E echo "terEngine: packaged ${_zip}"
    VERBATIM)
  add_dependencies(${target}_zip ${target}_resources)
endfunction()
