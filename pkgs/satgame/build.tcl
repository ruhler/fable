namespace eval "pkgs/satgame" {
  pkg satgame [list core] ""

  # SatGame/Tests compiled
  stdio $::b/pkgs/satgame/satgame-tests "/SatGame/Tests%" "satgame"
  testsuite $::b/pkgs/satgame/satgame-tests.tr $::b/pkgs/satgame/satgame-tests \
    "$::b/pkgs/satgame/satgame-tests"

  # fble-satgame binary
  stdio $::b/pkgs/satgame/fble-satgame "/SatGame/Main%" "satgame"
  install $::b/pkgs/satgame/fble-satgame $::config::bindir/fble-satgame
}
