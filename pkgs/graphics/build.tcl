namespace eval "pkgs/graphics" {
  pkg graphics [list core app] ""

  # /Graphics/Tests% interpreted
  set cflags "-I $::s/pkgs/graphics -I $::s/pkgs/app -I $::s/pkgs/core"
  testsuite $::b/pkgs/graphics/tests-interpreted.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/graphics/Graphics/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Graphics/Tests%"

  if $::config::enable_fble_app {
    fbld_man_usage $::b/pkgs/graphics/fble-graphics.1 $::s/pkgs/graphics/fble-graphics.fbld
    install $::b/pkgs/graphics/fble-graphics.1 $::config::mandir/man1/fble-graphics.1
    app $::b/pkgs/graphics/fble-graphics "/Graphics/AppIO%" "graphics"
    install $::b/pkgs/graphics/fble-graphics $::config::bindir/fble-graphics
  }
}
