namespace eval "pkgs/sat" {
  fbld_help_fble_usage $::b/pkgs/sat/Sat/Usage.fble $::s/pkgs/sat/fble-sat.fbld

  pkg sat [list core] "$::b/pkgs/sat/Sat/Usage.fble" ""

  # /Sat/Tests% interpreted
  testsuite $::b/pkgs/sat/tests-interpreted.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/sat/Sat/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/pkgs/sat -I $::b/pkgs/sat -m /Sat/Tests% --prefix Interpreted."

  # /Sat/Tests% compiled
  stdio $::b/pkgs/sat/sat-tests "/Sat/Tests%" "sat" ""
  testsuite $::b/pkgs/sat/tests-compiled.tr \
    $::b/pkgs/sat/sat-tests \
    "$::b/pkgs/sat/sat-tests --prefix Compiled."

  # fble-sat binary
  fbld_man_usage $::b/pkgs/sat/fble-sat.1 $::s/pkgs/sat/fble-sat.fbld
  install $::b/pkgs/sat/fble-sat.1 $::config::mandir/man1/fble-sat.1
  stdio $::b/pkgs/sat/fble-sat "/Sat/Main%" "sat" ""
  install $::b/pkgs/sat/fble-sat $::config::bindir/fble-sat
}
