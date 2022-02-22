namespace eval "pkgs/pinball" {
  pkg pinball [list core app] ""
  app $::out/pkgs/pinball/Pinball/fble-pinball "/Pinball/App%" \
    "fble-pinball" $::out/pkgs/pinball/libfble-pinball.a

  # /Pinball/Tests% interpreted
  test $::out/pkgs/pinball/Pinball/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/pinball/Pinball/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-pinball] -m /Pinball/Tests%" "pool = console"

  # /Pinball/Tests% compiled
  stdio $::out/pkgs/pinball/Pinball/pinball-tests "/Pinball/Tests%" "fble-pinball" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/pinball/libfble-pinball.a"
  test $::out/pkgs/pinball/Pinball/pinball-tests.tr $::out/pkgs/pinball/Pinball/pinball-tests \
    "$::out/pkgs/pinball/Pinball/pinball-tests" "pool = console"
}
