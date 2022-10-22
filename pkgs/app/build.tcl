namespace eval "pkgs/app" {
  set objs [list]

  # .c library files.
  foreach {x} { app.fble } {
    lappend objs $::b/pkgs/app/$x.o
    obj $::b/pkgs/app/$x.o $::s/pkgs/app/$x.c \
      "-I /usr/include/SDL2 -I $::s/fble/include -I $::s/pkgs/core -I $::s/pkgs/app"
  }

  pkg app [list core] $objs

  # fble-app program.
  obj $::b/pkgs/app/fble-app.o $::s/pkgs/app/fble-app.c \
    "-I $::s/fble/include -I $::s/pkgs/core -I $::s/pkgs/app -I/usr/include/SDL2"
  bin $::b/pkgs/app/fble-app \
    "$::b/pkgs/app/fble-app.o $::b/pkgs/app/libfble-app.a $::b/pkgs/core/libfble-core.a $::b/fble/lib/libfble.a" \
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
      append objs " $::b/pkgs/$lib/libfble-$lib.a"
    }
    append objs " $::b/pkgs/app/libfble-app.a"
    append objs " $::b/pkgs/core/libfble-core.a"
    append objs " $::b/fble/lib/libfble.a"
    
    fbleobj $target.o $::b/fble/bin/fble-compile \
      "--main FbleAppMain -m $path"
    bin $target $objs "-lSDL2 -lGL"
  }

  # /App/Tests% interpreted
  set cflags "-I $::s/pkgs/app -I $::s/pkgs/core"
  test $::b/pkgs/app/App/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/app/App/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /App/Tests%"
}
