
set(TOOLS
  fble-compile
  fble-disassemble
  fble-mem-test
  fble-perf-profile
  fble-profiles-test
  fble-profile-test
  fble-test
)

foreach(TOOL ${TOOLS})
  add_executable(${TOOL} ${TOOL}.c)
  target_include_directories(${TOOL} PUBLIC "${PROJECT_SOURCE_DIR}/fble/include")
  target_link_libraries(${TOOL} PUBLIC fble)
endforeach()

