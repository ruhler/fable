
add_executable(fble-test fble-test.c)
target_include_directories(fble-test PUBLIC "${PROJECT_SOURCE_DIR}/fble/include")
target_link_libraries(fble-test PUBLIC fble)
