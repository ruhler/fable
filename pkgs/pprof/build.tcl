namespace eval "pkgs/pprof" {
  pkg pprof [list core network] "" ""

  # fble-pprof program.
  stdio $::b/pkgs/pprof/fble-pprof "/Pprof/Server/Main%" "network pprof" ""
  install $::b/pkgs/pprof/fble-pprof $::config::bindir/fble-pprof
}
