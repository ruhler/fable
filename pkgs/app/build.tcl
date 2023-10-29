namespace eval "pkgs/app" {
  set objs [list]
  if $::config::enable_fble_app {
    fbld_header_usage $::b/pkgs/app/app.usage.h $::s/pkgs/app/app.fbld \
      fbldUsageHelpText $::s/pkgs/app/app.usage.txt
    obj $::b/pkgs/app/app.o $::s/pkgs/app/app.c \
      "$::config::sdl_cflags $::config::gl_cflags -I $::s/include -I $::s/pkgs/core -I $::s/pkgs/app -I $::b/pkgs/app" \
      $::b/pkgs/app/app.usage.h
    lappend objs $::b/pkgs/app/app.o
  }

  pkg app [list core] $objs

  # /App/Tests% interpreted
  set cflags "-I $::s/pkgs/app -I $::s/pkgs/core"
  test $::b/pkgs/app/App/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/app/App/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /App/Tests%"

  if $::config::enable_fble_app {
    # fble-app program.
    fbld_man_usage $::b/pkgs/app/fble-app.1 $::s/pkgs/app/app.fbld
    install $::b/pkgs/app/fble-app.1 $::config::mandir/man1/fble-app.1

    obj $::b/pkgs/app/fble-app.o $::s/pkgs/app/fble-app.c \
      "-I $::s/include -I $::s/pkgs/core -I $::s/pkgs/app $::config::sdl_cflags"
    bin $::b/pkgs/app/fble-app \
      "$::b/pkgs/app/fble-app.o $::b/pkgs/app/libfble-app.a $::b/pkgs/core/libfble-core.a $::b/lib/libfble.a" \
      "$::config::sdl_libs $::config::gl_libs"
    install $::b/pkgs/app/fble-app $::config::bindir/fble-app

    # Build an fble-app compiled binary.
    # This is only available when enable_fble_app configuration is enabled.
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
      append objs " $::b/lib/libfble.a"
      
      fbleobj $target.o $::b/bin/fble-compile \
        "--main FbleAppMain -m $path"
      bin $target $objs "$::config::sdl_libs $::config::gl_libs"
    }
  }
}
