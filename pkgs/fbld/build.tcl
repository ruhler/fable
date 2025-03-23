namespace eval "pkgs/fbld" {
  pkg fbld [list core] "" ""

  # Fbld/Tests compiled
  stdio $::b/pkgs/fbld/fbld-tests "/Fbld/Tests%" "fbld" ""
  testsuite $::b/pkgs/fbld/fbld-tests.tr $::b/pkgs/fbld/fbld-tests \
    "$::b/pkgs/fbld/fbld-tests"

  # fbld
  stdio $::b/pkgs/fbld/fble-fbld "/Fbld/Main/IO%" "fbld" ""
  install $::b/pkgs/fbld/fble-fbld $::config::bindir/fble-fbld
  fbld_man_usage $::b/pkgs/fbld/fble-fbld.1 $::s/pkgs/fbld/fble-fbld.fbld
  install $::b/pkgs/fbld/fble-fbld.1 $::config::mandir/man1/fble-fbld.1

  # Fbld spec tests.
  foreach {x} [build_glob $::s/fbld/SpecTests -tails -nocomplain -type f *.fbld] {
    # fble-based fbld.
    test $::b/fbld/SpecTests/$x.fble.tr \
      "$::b/pkgs/fbld/fble-fbld $::s/fbld/spec-test.run.tcl $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x" \
      "tclsh8.6 $::s/fbld/spec-test.run.tcl $::b/pkgs/fbld/fble-fbld $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x"
  }
}
