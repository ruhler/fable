namespace eval "pkgs/graphics" {
  dist_s $::s/pkgs/graphics/build.tcl
  dist_s $::s/pkgs/graphics/TODO.txt

  pkg graphics [list core app] ""

  if $::config::enable_fble_app {
    app $::b/pkgs/graphics/fble-graphics "/Graphics/AppIO%" "graphics"
    install $::b/pkgs/graphics/fble-graphics $::config::bindir/fble-graphics
  }
}
