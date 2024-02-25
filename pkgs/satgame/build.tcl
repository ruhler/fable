namespace eval "pkgs/satgame" {
  pkg satgame [list core] ""

  # fble-satgame binary
  stdio $::b/pkgs/satgame/fble-satgame "/SatGame/Main%" "satgame"
  install $::b/pkgs/satgame/fble-satgame $::config::bindir/fble-satgame
}
