namespace eval "pkgs/sat" {
  fbld_help_fble_usage $::b/pkgs/sat/Sat/Usage.fble $::s/pkgs/sat/fble-sat.fbld

  pkg sat [list core] "$::b/pkgs/sat/Sat/Usage.fble" ""

  # /Sat/Tests% interpreted
  run_stdio_tests $::b/pkgs/sat/tests-interpreted.tr \
    "-I $::s/pkgs/core -I $::s/pkgs/sat -I $::b/pkgs/sat -m /Sat/Tests%" \
    $::b/pkgs/sat/Sat/Usage.fble

  # /Sat/Tests% compiled
  stdio $::b/pkgs/sat/sat-tests "/Sat/Tests%" "sat" ""
  testsuite $::b/pkgs/sat/tests-compiled.tr \
    $::b/pkgs/sat/sat-tests \
    "$::b/pkgs/sat/sat-tests --prefix Compiled."

  # fble-sat binary
  fbld_man_usage $::b/pkgs/sat/fble-sat.1 $::s/pkgs/sat/fble-sat.fbld
  install $::b/pkgs/sat/fble-sat.1 $::config::mandir/man1/fble-sat.1
  stdio $::b/pkgs/sat/fble-sat "/Sat/Main/IO%" "sat" ""
  install $::b/pkgs/sat/fble-sat $::config::bindir/fble-sat
}
