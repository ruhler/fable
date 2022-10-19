namespace eval "pkgs/sat" {
  pkg sat [list core] ""

  # /Sat/Tests% interpreted
  testsuite $::out/pkgs/sat/Sat/tests.tr "$::out/pkgs/core/fble-stdio $::out/pkgs/sat/Sat/Tests.fble.d" \
    "$::out/pkgs/core/fble-stdio -I pkgs/core -I pkgs/sat -m /Sat/Tests% --prefix Interpreted."

  # /Sat/Tests% compiled
  stdio $::out/pkgs/sat/Sat/sat-tests "/Sat/Tests%" "sat"
  testsuite $::out/pkgs/sat/Sat/sat-tests.tr $::out/pkgs/sat/Sat/sat-tests \
    "$::out/pkgs/sat/Sat/sat-tests --prefix Compiled."

  # /Sat/Bench% compiled
  stdio $::out/pkgs/sat/Sat/sat-bench "/Sat/Bench%" "sat"

  # fble-sat binary
  stdio $::out/pkgs/sat/Sat/fble-sat "/Sat/Main%" "sat"
}
