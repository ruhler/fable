namespace eval "pkgs/pinball" {
  pkg pinball [list core app] ""

  # /Pinball/Tests% interpreted
  set cflags "-I $::s/pkgs/core -I $::s/pkgs/app -I $::s/pkgs/pinball"
  testsuite $::b/pkgs/pinball/tests-interpreted.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/pinball/Pinball/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Pinball/Tests% --prefix Interpreted."

  # /Pinball/Tests% compiled
  stdio $::b/pkgs/pinball/pinball-tests "/Pinball/Tests%" "pinball app"
  testsuite $::b/pkgs/pinball/tests-compiled.tr \
    $::b/pkgs/pinball/pinball-tests \
    "$::b/pkgs/pinball/pinball-tests --prefix Compiled"

  if $::config::enable_fble_app {
    man_usage $::b/pkgs/pinball/fble-pinball.1 $::s/pkgs/pinball/pinball.fbld
    install $::b/pkgs/pinball/fble-pinball.1 $::config::mandir/man1/fble-pinball.1
    app $::b/pkgs/pinball/fble-pinball "/Pinball/AppIO%" "pinball"
    install $::b/pkgs/pinball/fble-pinball $::config::bindir/fble-pinball
  }
}
