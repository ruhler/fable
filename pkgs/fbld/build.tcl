namespace eval "pkgs/fbld" {
  pkg fbld [list core] "" ""

  # Fbld/Tests compiled
  stdio $::b/pkgs/fbld/fbld-tests "/Fbld/Tests%" "fbld"
  testsuite $::b/pkgs/fbld/fbld-tests.tr $::b/pkgs/fbld/fbld-tests \
    "$::b/pkgs/fbld/fbld-tests"

  # fbld
  # Create this under a bin directory to avoid name conflict with Fbld/
  # directory on Windows.
  stdio $::b/pkgs/fbld/bin/fbld "/Fbld/Main%" "fbld"
  install $::b/pkgs/fbld/bin/fbld $::config::bindir/fbld
  fbld_man_usage $::b/pkgs/fbld/fbld.1 $::s/pkgs/fbld/fbld.fbld
  install $::b/pkgs/fbld/fbld.1 $::config::mandir/man1/fbld.1

  # Fbld spec tests.
  foreach {x} [build_glob $::s/fbld/SpecTests -tails -nocomplain -type f *.fbld] {
    # fble-based fbld.
    test $::b/fbld/SpecTests/$x.fble.tr \
      "$::b/pkgs/fbld/bin/fbld $::s/fbld/spec-test.run.tcl $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x" \
      "tclsh8.6 $::s/fbld/spec-test.run.tcl $::b/pkgs/fbld/bin/fbld $::s/fbld/SpecTests.fbld $::s/fbld/SpecTests/$x"
  }
}
