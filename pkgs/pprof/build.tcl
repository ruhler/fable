namespace eval "pkgs/pprof" {

  fbld_help_fble_usage $::b/pkgs/pprof/Pprof/Usage.fble $::s/pkgs/pprof/fble-pprof.fbld

  pkg pprof [list core network] $::b/pkgs/pprof/Pprof/Usage.fble ""

  set ldflags ""
  if {[string first "_NT" [exec uname -s]] != -1} {
    # Windows needs to link to Ws2_32.lib for the sockets library.
    set ldflags "-lWs2_32"
  }

  # fble-pprof program.
  fbld_man_usage $::b/pkgs/pprof/fble-pprof.1 $::s/pkgs/pprof/fble-pprof.fbld
  install $::b/pkgs/pprof/fble-pprof.1 $::config::mandir/man1/fble-pprof.1
  stdio $::b/pkgs/pprof/fble-pprof "/Pprof/Server/Main%" "network pprof" "$ldflags"
  install $::b/pkgs/pprof/fble-pprof $::config::bindir/fble-pprof

  # Pprof/Tests compiled
  stdio $::b/pkgs/pprof/pprof-tests "/Pprof/Tests%" "pprof" ""
  testsuite $::b/pkgs/pprof/pprof-tests.tr $::b/pkgs/pprof/pprof-tests \
    "$::b/pkgs/pprof/pprof-tests"
}
