namespace eval "pkgs/pinball" {
  pkg pinball [list core app] ""
  app $::out/pkgs/pinball/Pinball/fble-pinball "/Pinball/App%" "pinball"

  # /Pinball/Tests% interpreted
  set cflags "-I pkgs/core -I pkgs/app -I pkgs/pinball"
  test $::out/pkgs/pinball/Pinball/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/pinball/Pinball/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio $cflags -m /Pinball/Tests%" "pool = console"

  # /Pinball/Tests% compiled
  stdio $::out/pkgs/pinball/Pinball/pinball-tests "/Pinball/Tests%" "pinball app"
  test $::out/pkgs/pinball/Pinball/pinball-tests.tr $::out/pkgs/pinball/Pinball/pinball-tests \
    "$::out/pkgs/pinball/Pinball/pinball-tests" "pool = console"
}
