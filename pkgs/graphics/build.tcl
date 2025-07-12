namespace eval "pkgs/graphics" {
  fbld_help_fble_usage $::b/pkgs/graphics/Graphics/Usage.fble $::s/pkgs/graphics/fble-graphics.fbld

  pkg graphics [list core app] $::b/pkgs/graphics/Graphics/Usage.fble ""

  # /Graphics/Tests% interpreted
  set cflags "-I $::s/pkgs/graphics -I $::b/pkgs/graphics -I $::s/pkgs/app -I $::s/pkgs/core"
  run_stdio_tests $::b/pkgs/graphics/tests-interpreted.tr "$cflags -m /Graphics/Tests%" $::b/pkgs/graphics/Graphics/Usage.fble

  if $::config::enable_fble_app {
    fbld_man_usage $::b/pkgs/graphics/fble-graphics.1 $::s/pkgs/graphics/fble-graphics.fbld
    install $::b/pkgs/graphics/fble-graphics.1 $::config::mandir/man1/fble-graphics.1
    app $::b/pkgs/graphics/fble-graphics "/Graphics/AppIO%" "graphics"
    install $::b/pkgs/graphics/fble-graphics $::config::bindir/fble-graphics
  }
}
