namespace eval "pkgs/sat" {
  dist_s $::s/pkgs/sat/bench/README.txt
  dist_s $::s/pkgs/sat/bench/unif-c1000-v500-s1356655268.cnf
  dist_s $::s/pkgs/sat/build.tcl
  dist_s $::s/pkgs/sat/sat.fbld
  dist_s $::s/pkgs/sat/thoughts.txt
  dist_s $::s/pkgs/sat/TODO.txt

  pkg sat [list core] ""

  # /Sat/Tests% interpreted
  testsuite $::b/pkgs/sat/tests-interpreted.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/sat/Sat/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/pkgs/sat -m /Sat/Tests% --prefix Interpreted."

  # /Sat/Tests% compiled
  stdio $::b/pkgs/sat/sat-tests "/Sat/Tests%" "sat"
  testsuite $::b/pkgs/sat/tests-compiled.tr \
    $::b/pkgs/sat/sat-tests \
    "$::b/pkgs/sat/sat-tests --prefix Compiled."

  # /Sat/Bench% compiled
  stdio $::b/pkgs/sat/sat-bench "/Sat/Bench%" "sat"

  # fble-sat binary
  man_usage $::b/pkgs/sat/fble-sat.1 $::s/pkgs/sat/sat.fbld
  install $::b/pkgs/sat/fble-sat.1 $::config::mandir/man1/fble-sat.1
  stdio $::b/pkgs/sat/fble-sat "/Sat/Main%" "sat"
  install $::b/pkgs/sat/fble-sat $::config::bindir/fble-sat
}
