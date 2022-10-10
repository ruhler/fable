namespace eval "pkgs/pinball" {
  pkg pinball [list core app] ""

  # /Pinball/Tests% interpreted
  set cflags "-I pkgs/core -I pkgs/app -I pkgs/pinball"
  testsuite $::out/pkgs/pinball/Pinball/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/pinball/Pinball/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio $cflags -m /Pinball/Tests% --prefix Interpreted."

  if {$::arch == "aarch64"} {
    app $::out/pkgs/pinball/Pinball/fble-pinball "/Pinball/AppIO%" "pinball"

    # /Pinball/Tests% compiled
    stdio $::out/pkgs/pinball/Pinball/pinball-tests "/Pinball/Tests%" "pinball app"
    testsuite $::out/pkgs/pinball/Pinball/pinball-tests.tr $::out/pkgs/pinball/Pinball/pinball-tests \
      "$::out/pkgs/pinball/Pinball/pinball-tests --prefix Compiled"
  }
}
