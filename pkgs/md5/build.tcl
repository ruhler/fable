namespace eval "pkgs/md5" {

  fbld_help_fble_usage $::b/pkgs/md5/Md5/Usage.fble $::s/pkgs/md5/fble-md5.fbld

  pkg md5 [list core] $::b/pkgs/md5/Md5/Usage.fble ""

  # Md5/Tests interpreted
  run_stdio_tests $::b/pkgs/md5/Md5/tests.tr \
    "-I $::s/pkgs/core -I $::s/pkgs/md5 -I $::b/pkgs/md5 -m /Md5/Tests%" \
    $::b/pkgs/md5/Md5/Usage.fble

  # Md5/Tests compiled
  stdio $::b/pkgs/md5/md5-tests "/Md5/Tests%" "md5" ""
  testsuite $::b/pkgs/md5/md5-tests.tr $::b/pkgs/md5/md5-tests \
    "$::b/pkgs/md5/md5-tests --prefix Compiled."

  # Md5/Bench compiled
  stdio $::b/pkgs/md5/md5-bench "/Md5/Bench/Main%" "md5" ""

  # fble-md5 program.
  fbld_man_usage $::b/pkgs/md5/fble-md5.1 $::s/pkgs/md5/fble-md5.fbld
  install $::b/pkgs/md5/fble-md5.1 $::config::mandir/man1/fble-md5.1
  stdio $::b/pkgs/md5/fble-md5 "/Md5/Main%" "md5" ""
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

