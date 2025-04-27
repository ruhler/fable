namespace eval "pkgs/pprof2" {
  pkg pprof2 [list core] "" ""

  # Pprof/Tests compiled
  stdio $::b/pkgs/pprof2/pprof-tests "/Pprof/Tests%" "pprof2" ""
  testsuite $::b/pkgs/pprof2/pprof-tests.tr $::b/pkgs/pprof2/pprof-tests \
    "$::b/pkgs/pprof2/pprof-tests"

  # fble-perf-pprof program.
  stdio $::b/pkgs/pprof2/fble-perf-pprof "/Pprof/Main%" "pprof2" ""
  install $::b/pkgs/pprof2/fble-perf-pprof $::config::bindir/fble-perf-pprof
}
