namespace eval "pkgs/graphics" {
  pkg graphics [list core app] ""
  if {$::arch == "aarch64"} {
    app $::out/pkgs/graphics/Graphics/fble-graphics "/Graphics/AppIO%" "graphics"
  }
}
