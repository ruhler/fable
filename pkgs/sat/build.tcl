namespace eval "pkgs/sat" {
  pkg sat [list core] ""

  # /Sat/Tests% interpreted
  testsuite $::b/pkgs/sat/Sat/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/sat/Sat/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/pkgs/sat -m /Sat/Tests% --prefix Interpreted."

  # /Sat/Tests% compiled
  stdio $::b/pkgs/sat/Sat/sat-tests "/Sat/Tests%" "sat"
  testsuite $::b/pkgs/sat/Sat/sat-tests.tr $::b/pkgs/sat/Sat/sat-tests \
    "$::b/pkgs/sat/Sat/sat-tests --prefix Compiled."

  # /Sat/Bench% compiled
  stdio $::b/pkgs/sat/Sat/sat-bench "/Sat/Bench%" "sat"

  # fble-sat binary
  stdio $::b/pkgs/sat/Sat/fble-sat "/Sat/Main%" "sat"
  install_bin $::b/pkgs/sat/Sat/fble-sat
}
