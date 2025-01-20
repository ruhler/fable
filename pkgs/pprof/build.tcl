namespace eval "pkgs/pprof" {
  pkg pprof [list core network] "" ""

  # Pprof/Tests compiled
  stdio $::b/pkgs/pprof/pprof-tests "/Pprof/Tests%" "network pprof" ""
  testsuite $::b/pkgs/pprof/pprof-tests.tr $::b/pkgs/pprof/pprof-tests \
    "$::b/pkgs/pprof/pprof-tests"

  # fble-pprof program.
  stdio $::b/pkgs/pprof/fble-pprof "/Pprof/Server/Main%" "network pprof" ""
  install $::b/pkgs/pprof/fble-pprof $::config::bindir/fble-pprof
}
