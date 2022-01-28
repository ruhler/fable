
# fble-stdio
add_executable(fble-stdio
  fble-stdio.c
  Core/char.fble.c
  Core/int.fble.c
  Core/string.fble.c
)
target_include_directories(fble-stdio PUBLIC "${PROJECT_SOURCE_DIR}/fble/include")
target_include_directories(fble-stdio PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(fble-stdio PUBLIC fble)

# fble-md5
add_executable(fble-md5 fble-md5.c)
target_include_directories(fble-md5 PUBLIC "${PROJECT_SOURCE_DIR}/fble/include")
target_link_libraries(fble-md5 PUBLIC fble)

# fble-app
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
add_executable(fble-app
  fble-app.c
  Core/char.fble.c
  Core/int.fble.c
  Core/string.fble.c
)
target_include_directories(fble-app PUBLIC "${PROJECT_SOURCE_DIR}/fble/include")
target_include_directories(fble-app PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(fble-app PUBLIC ${SDL2_INCLUDE_DIRS})
target_include_directories(fble-app PUBLIC ${OPENGL_INCLUDE_DIR})
target_link_libraries(fble-app PUBLIC ${OPENGL_LIBRARIES} ${SDL2_LIBRARIES} fble)
