namespace eval "pkgs/std-tests" {
  pkg std-tests [list std] "" ""

  # /Std/Tests interpreted
  run_cli_tests $::b/pkgs/std-tests/std-tests-interpreted.tr \
    "-I $::s/pkgs/std -I $::s/pkgs/std-tests -m /Std/Tests%" ""

  # /Std/Tests compiled
  cli $::b/pkgs/std-tests/std-tests "/Std/Tests%" "std-tests" ""
  testsuite $::b/pkgs/std-tests/std-tests-compiled.tr \
    $::b/pkgs/std-tests/std-tests \
    "$::b/pkgs/std-tests/std-tests --prefix Compiled."
}
