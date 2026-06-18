namespace eval "pkgs/md5" {

  fbld_help_fble_usage $::b/pkgs/md5/Md5/Usage.fble $::s/pkgs/md5/fble-md5.fbld

  pkg md5 [list std] $::b/pkgs/md5/Md5/Usage.fble ""

  # Md5/Tests interpreted
  run_cli_tests $::b/pkgs/md5/Md5/tests.tr \
    "-I $::s/pkgs/std -I $::s/pkgs/md5 -I $::b/pkgs/md5 -m /Md5/Tests%" \
    $::b/pkgs/md5/Md5/Usage.fble

  # Md5/Tests compiled
  cli $::b/pkgs/md5/md5-tests "/Md5/Tests%" "md5" ""
  testsuite $::b/pkgs/md5/md5-tests.tr $::b/pkgs/md5/md5-tests \
    "$::b/pkgs/md5/md5-tests --prefix Compiled."

  # Md5/Bench compiled
  cli $::b/pkgs/md5/md5-bench "/Md5/Bench/Main%" "md5" ""

  # fble-md5 program.
  fbld_man_usage $::b/pkgs/md5/fble-md5.1 $::s/pkgs/md5/fble-md5.fbld
  install $::b/pkgs/md5/fble-md5.1 $::config::mandir/man1/fble-md5.1
  cli $::b/pkgs/md5/fble-md5 "/Md5/Main%" "md5" ""
  install $::b/pkgs/md5/fble-md5 $::config::bindir/fble-md5

  # fble-md5 test
  build $::b/pkgs/md5/fble-md5.out \
    "$::b/pkgs/md5/fble-md5 $::s/pkgs/md5/fble-md5.in" \
    "$::b/pkgs/md5/fble-md5 < $::s/pkgs/md5/fble-md5.in > $::b/pkgs/md5/fble-md5.out"
  test $::b/pkgs/md5/fble-md5.tr \
    "$::b/pkgs/md5/fble-md5.out" \
    "grep 96a1a5fea24987d6a38f206939ac148c $::b/pkgs/md5/fble-md5.out"
}

