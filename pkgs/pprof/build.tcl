namespace eval "pkgs/pprof" {
  pkg pprof [list core network] "" ""

  # Pprof/Tests compiled
  stdio $::b/pkgs/pprof/pprof-tests "/Pprof/Tests%" "network pprof" ""
  testsuite $::b/pkgs/pprof/pprof-tests.tr $::b/pkgs/pprof/pprof-tests \
    "$::b/pkgs/pprof/pprof-tests"

  set ldflags ""
  if {[string first "_NT" [exec uname -s]] != -1} {
    # Windows needs to link to Ws2_32.lib for the sockets library.
    set ldflags "-lWs2_32"
  }

  # fble-pprof program.
  stdio $::b/pkgs/pprof/fble-pprof "/Pprof/Server/Main%" "network pprof" "$ldflags"
  install $::b/pkgs/pprof/fble-pprof $::config::bindir/fble-pprof

  # fble-pprof-report program.
  stdio $::b/pkgs/pprof/fble-pprof-report "/Pprof/Report%" "network pprof" ""
  install $::b/pkgs/pprof/fble-pprof-report $::config::bindir/fble-pprof-report
}
