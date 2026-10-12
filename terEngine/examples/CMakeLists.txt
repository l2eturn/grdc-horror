foreach(example 01_hello_cube 02_dark_room 03_model_viewer)
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} PRIVATE ter::engine)
endforeach()
