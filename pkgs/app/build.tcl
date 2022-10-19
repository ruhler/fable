namespace eval "pkgs/app" {
  set objs [list]

  # .c library files.
  foreach {x} { app.fble } {
    lappend objs $::out/pkgs/app/$x.o
    obj $::out/pkgs/app/$x.o pkgs/app/$x.c "-I /usr/include/SDL2 -I fble/include -I pkgs/core -I pkgs/app"
  }

  pkg app [list core] $objs

  # fble-app program.
  obj $::out/pkgs/app/fble-app.o pkgs/app/fble-app.c \
    "-I fble/include -I pkgs/core -I pkgs/app -I/usr/include/SDL2"
  bin $::out/pkgs/app/fble-app \
    "$::out/pkgs/app/fble-app.o $::out/pkgs/app/libfble-app.a $::out/pkgs/core/libfble-core.a $::out/fble/lib/libfble.a" \
    "-lSDL2 -lGL"

  # Build an fble-app compiled binary.
  #
  # Inputs:
  #   target - the file to build.
  #   path - the module path to use as App@ main.
  #   libs - addition fble packages to depend on besides core and app
  #          (without fble- prefix).
  proc ::app { target path libs} {
    set objs $target.o
    foreach lib $libs {
      append objs " $::out/pkgs/$lib/libfble-$lib.a"
    }
    append objs " $::out/pkgs/app/libfble-app.a"
    append objs " $::out/pkgs/core/libfble-core.a"
    append objs " $::out/fble/lib/libfble.a"
    
    fbleobj $target.o $::out/fble/bin/fble-compile \
      "--main FbleAppMain -m $path"
    bin $target $objs "-lSDL2 -lGL"
  }

  # /App/Tests% interpreted
  set cflags "-I pkgs/app -I pkgs/core"
  test $::out/pkgs/app/App/tests.tr "$::out/pkgs/core/fble-stdio $::out/pkgs/app/App/Tests.fble.d" \
    "$::out/pkgs/core/fble-stdio $cflags -m /App/Tests%" "pool = console"
}
