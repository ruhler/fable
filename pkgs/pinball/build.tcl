namespace eval "pkgs/pinball" {
  pkg pinball [list core app] ""

  # /Pinball/Tests% interpreted
  set cflags "-I $::s/pkgs/core -I $::s/pkgs/app -I $::s/pkgs/pinball"
  testsuite $::b/pkgs/pinball/Pinball/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/pinball/Pinball/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Pinball/Tests% --prefix Interpreted."

  app $::b/pkgs/pinball/Pinball/fble-pinball "/Pinball/AppIO%" "pinball"
  install_bin $::b/pkgs/pinball/Pinball/fble-pinball

  # /Pinball/Tests% compiled
  stdio $::b/pkgs/pinball/Pinball/pinball-tests "/Pinball/Tests%" "pinball app"
  testsuite $::b/pkgs/pinball/Pinball/pinball-tests.tr $::b/pkgs/pinball/Pinball/pinball-tests \
    "$::b/pkgs/pinball/Pinball/pinball-tests --prefix Compiled"
}
