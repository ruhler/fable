namespace eval "pkgs/satgame" {
  pkg satgame [list core app] "" ""

  # SatGame/Tests compiled
  # --allow-shlib-undefined because we know this doesn't use the part of the
  # app package that depends on SDL.
  stdio $::b/pkgs/satgame/satgame-tests "/SatGame/Tests%" "app satgame" "-Wl,--allow-shlib-undefined"
  testsuite $::b/pkgs/satgame/satgame-tests.tr $::b/pkgs/satgame/satgame-tests \
    "$::b/pkgs/satgame/satgame-tests"

  if $::config::enable_fble_app {
    app $::b/pkgs/satgame/fble-satgame "/SatGame/AppIO%" "satgame"
    install $::b/pkgs/satgame/fble-satgame $::config::bindir/fble-satgame
  }
}
