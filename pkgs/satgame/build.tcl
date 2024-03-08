namespace eval "pkgs/satgame" {
  pkg satgame [list core app] ""

  # SatGame/Tests compiled
  stdio $::b/pkgs/satgame/satgame-tests "/SatGame/Tests%" "satgame app"
  testsuite $::b/pkgs/satgame/satgame-tests.tr $::b/pkgs/satgame/satgame-tests \
    "$::b/pkgs/satgame/satgame-tests"

  # fble-satgame binary
  stdio $::b/pkgs/satgame/fble-satgame "/SatGame/Main%" "satgame app"
  install $::b/pkgs/satgame/fble-satgame $::config::bindir/fble-satgame

  if $::config::enable_fble_app {
    app $::b/pkgs/satgame/fble-satgame-screenshot "/SatGame/Screenshot/AppIO%" "satgame"
    install $::b/pkgs/satgame/fble-satgame-screenshot $::config::bindir/fble-satgame-screenshot
  }
}
