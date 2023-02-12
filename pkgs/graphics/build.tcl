namespace eval "pkgs/graphics" {
  pkg graphics [list core app] ""

  if $::config::enable_fble_app {
    app $::b/pkgs/graphics/fble-graphics "/Graphics/AppIO%" "graphics"
    install_bin $::b/pkgs/graphics/fble-graphics
  }
}
