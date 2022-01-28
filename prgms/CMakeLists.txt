
add_executable(fble-stdio
  fble-stdio.c
  Core/char.fble.c
  Core/int.fble.c
  Core/string.fble.c
)
target_include_directories(fble-stdio PUBLIC "${PROJECT_SOURCE_DIR}/fble/include")
target_include_directories(fble-stdio PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fble-stdio PUBLIC fble)
