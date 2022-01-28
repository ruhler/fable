
add_library(fble
  alloc.c
  code.c
  compile.c
  execute.c
  expr.c
  generate_aarch64.c
  heap.c
  interpret.c
  kind.c
  link.c
  load.c
  loc.c
  main.c
  module-path.c
  name.c
  parse.y
  profile.c
  string.c
  tc.c
  type.c
  typecheck.c
  value.c
  vector.c
)
target_include_directories(fble PUBLIC "${PROJECT_SOURCE_DIR}/include")
