namespace eval "pkgs/fbld" {
  pkg fbld [list core] ""

  # Fbld/Tests interpreted
  testsuite $::b/pkgs/fbld/Fbld/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/fbld/Fbld/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio -I $::s/pkgs/core -I $::s/pkgs/fbld -m /Fbld/Tests% --prefix Interpreted."

  # Fbld/Tests compiled
  stdio $::b/pkgs/fbld/fbld-tests "/Fbld/Tests%" "fbld"
  testsuite $::b/pkgs/fbld/fbld-tests.tr $::b/pkgs/fbld/fbld-tests \
    "$::b/pkgs/fbld/fbld-tests --prefix Compiled."
}

