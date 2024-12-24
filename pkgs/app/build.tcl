namespace eval "pkgs/app" {
  set objs [list]
  if $::config::enable_fble_app {
    fbld_header_usage $::b/pkgs/app/fble-app.usage.h $::s/pkgs/app/fble-app.fbld \
      fbldUsageHelpText
    obj $::b/pkgs/app/app.o $::s/pkgs/app/app.c \
      "$::config::sdl_cflags $::config::gl_cflags -I $::s/include -I $::s/pkgs/core -I $::s/pkgs/app -I $::b/pkgs/app" \
      $::b/pkgs/app/fble-app.usage.h
    lappend objs $::b/pkgs/app/app.o
  }

  pkg app [list core] "" $objs

  # Check doc comments
  foreach {x} [build_glob $::s/pkgs/app -tails "*.h" "*.c"] {
    fbld_check_dc $::b/pkgs/app/$x.dc $::s/pkgs/app/$x
  }

  # /App/Tests% interpreted
  set cflags "-I $::s/pkgs/app -I $::s/pkgs/core"
  test $::b/pkgs/app/App/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/app/libfble-app$::lext" \
    "$::b/pkgs/core/fble-stdio $cflags -m /App/Tests%"

  if $::config::enable_fble_app {
    # fble-app program.
    fbld_man_usage $::b/pkgs/app/fble-app.1 $::s/pkgs/app/fble-app.fbld
    install $::b/pkgs/app/fble-app.1 $::config::mandir/man1/fble-app.1

    obj $::b/pkgs/app/fble-app.o $::s/pkgs/app/fble-app.c \
      "-I $::s/include -I $::s/pkgs/core -I $::s/pkgs/app $::config::sdl_cflags"
    bin $::b/pkgs/app/fble-app \
      "$::b/pkgs/app/fble-app.o" \
      "$::b/pkgs/app/libfble-app$::lext $::b/pkgs/core/libfble-core$::lext $::b/lib/libfble$::lext" \
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
      set nlibs ""
      foreach lib [lreverse $libs] {
        append nlibs " $::b/pkgs/$lib/libfble-$lib$::lext"
      }
      append nlibs " $::b/pkgs/app/libfble-app$::lext $::b/pkgs/core/libfble-core$::lext $::b/lib/libfble$::lext"
      
      fblemain $target.o $::b/bin/fble-compile "--main FbleAppMain -m $path"
      bin $target $objs $nlibs "$::config::sdl_libs $::config::gl_libs"
    }
  }
}
