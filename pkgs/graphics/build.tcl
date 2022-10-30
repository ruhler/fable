namespace eval "pkgs/graphics" {
  pkg graphics [list core app] ""
  app $::b/pkgs/graphics/fble-graphics "/Graphics/AppIO%" "graphics"
  install_bin $::b/pkgs/graphics/fble-graphics
}
