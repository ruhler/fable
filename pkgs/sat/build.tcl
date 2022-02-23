namespace eval "pkgs/sat" {
  pkg sat [list core] ""

  # /Sat/Tests% interpreted
  test $::out/pkgs/sat/Sat/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/sat/Sat/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio -I pkgs/core -I pkgs/sat -m /Sat/Tests%" "pool = console"

  # /Sat/Tests% compiled
  stdio $::out/pkgs/sat/Sat/sat-tests "/Sat/Tests%" "sat"
  test $::out/pkgs/sat/Sat/sat-tests.tr $::out/pkgs/sat/Sat/sat-tests \
    "$::out/pkgs/sat/Sat/sat-tests" "pool = console"
}
