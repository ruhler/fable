namespace eval "pkgs/graphics" {
  pkg graphics [list core app] ""
  app $::out/pkgs/graphics/Graphics/fble-graphics "/Graphics/App%" \
    "fble-graphics" $::out/pkgs/graphics/libfble-graphics.a
}
