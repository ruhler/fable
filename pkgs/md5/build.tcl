namespace eval "pkgs/md5" {
  pkg md5 [list core] ""

  # Md5/Tests interpreted
  testsuite $::b/pkgs/md5/Md5/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/md5/Md5/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/pkgs/md5 -m /Md5/Tests% --prefix Interpreted."

  # Md5/Tests compiled
  stdio $::b/pkgs/md5/md5-tests "/Md5/Tests%" "md5"
  testsuite $::b/pkgs/md5/md5-tests.tr $::b/pkgs/md5/md5-tests \
    "$::b/pkgs/md5/md5-tests --prefix Compiled."

  # Md5/Bench compiled
  stdio $::b/pkgs/md5/md5-bench "/Md5/Bench%" "md5"

  # fble-md5 program.
  man_usage $::b/pkgs/md5/fble-md5.1 $::s/pkgs/md5/md5.fbld
  install $::b/pkgs/md5/fble-md5.1 $::config::mandir/man1/fble-md5.1
  stdio $::b/pkgs/md5/fble-md5 "/Md5/Main%" "md5"
  install $::b/pkgs/md5/fble-md5 $::config::bindir/fble-md5

  # fble-md5 test
  build $::b/pkgs/md5/fble-md5.in "" "echo hello > $::b/pkgs/md5/fble-md5.in"
  build $::b/pkgs/md5/fble-md5.out \
    "$::b/pkgs/md5/fble-md5 $::b/pkgs/md5/fble-md5.in" \
    "$::b/pkgs/md5/fble-md5 < $::b/pkgs/md5/fble-md5.in > $::b/pkgs/md5/fble-md5.out"
  test $::b/pkgs/md5/fble-md5.tr \
    "$::b/pkgs/md5/fble-md5.out" \
    "grep b1946ac92492d2347c6235b4d2611184 $::b/pkgs/md5/fble-md5.out"
}

