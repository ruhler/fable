namespace eval "pkgs/graphics" {
  pkg graphics [list core app] ""
  app $::b/pkgs/graphics/Graphics/fble-graphics "/Graphics/AppIO%" "graphics"
  install_bin $::b/pkgs/graphics/Graphics/fble-graphics
}
