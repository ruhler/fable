namespace eval "pkgs/graphics" {
  pkg graphics [list core app] ""

  if $::config::enable_fble_app {
    fbld_man_usage $::b/pkgs/graphics/fble-graphics.1 $::s/pkgs/graphics/graphics.fbld
    install $::b/pkgs/graphics/fble-graphics.1 $::config::mandir/man1/fble-graphics.1
    app $::b/pkgs/graphics/fble-graphics "/Graphics/AppIO%" "graphics"
    install $::b/pkgs/graphics/fble-graphics $::config::bindir/fble-graphics
  }
}
