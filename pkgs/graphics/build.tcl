namespace eval "pkgs/graphics" {
  pkg graphics [list core app] ""

  if $::config::enable_fble_app {
    app $::b/pkgs/graphics/fble-graphics "/Graphics/AppIO%" "graphics"
    install $::b/pkgs/graphics/fble-graphics $::config::bindir/fble-graphics
  }
}
