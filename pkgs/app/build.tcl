namespace eval "pkgs/app" {
  set objs [list]

  # .c library files.
  foreach {x} { App/app.fble } {
    lappend objs $::out/pkgs/app/$x.o
    obj $::out/pkgs/app/$x.o pkgs/app/$x.c "-I /usr/include/SDL2 -I fble/include -I pkgs/core -I pkgs/app"
  }

  pkg app $objs

  # fble-app program.
  obj $::out/pkgs/app/App/fble-app.o pkgs/app/App/fble-app.c \
    [exec pkg-config --cflags fble fble-core fble-app]
  bin $::out/pkgs/app/App/fble-app "$::out/pkgs/app/App/fble-app.o" \
    [exec pkg-config --static --libs fble-app] \
    "$::out/fble/lib/libfble.a $::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a"

  # Build an fble-app compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as App@ main.
  #   libs - addition pkg-config named libraries to depend on.
  #   args - optional additional dependencies.
  proc ::app { target path libs args} {
    build $target.s $::out/fble/bin/fble-compile \
      "$::out/fble/bin/fble-compile --main FbleAppMain -m $path > $target.s"
    asm $target.o $target.s
    bin $target "$target.o" \
      [exec pkg-config --static --libs fble-app {*}$libs] \
      "$::out/fble/lib/libfble.a $::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $args"
  }
}
