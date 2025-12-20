namespace eval "pkgs/std-tests" {
  pkg std-tests [list std core] "" ""

  # /Std/Tests interpreted
  run_stdio_tests $::b/pkgs/std-tests/std-tests-interpreted.tr \
    "-I $::s/pkgs/std -I $::s/pkgs/std-tests -I $::s/pkgs/core -m /Std/Tests%" ""

  # /Std/Tests compiled
  stdio $::b/pkgs/std-tests/std-tests "/Std/Tests%" "std-tests" ""
  testsuite $::b/pkgs/std-tests/std-tests-compiled.tr \
    $::b/pkgs/std-tests/std-tests \
    "$::b/pkgs/std-tests/std-tests --prefix Compiled."
}
