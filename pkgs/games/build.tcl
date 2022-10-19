namespace eval "pkgs/games" {
  pkg games [list core app] ""

  # /Games/Tests% interpreted
  set cflags "-I pkgs/games -I pkgs/app -I pkgs/core"
  testsuite $::out/pkgs/games/Games/tests.tr "$::out/pkgs/core/fble-stdio $::out/pkgs/games/Games/Tests.fble.d" \
    "$::out/pkgs/core/fble-stdio $cflags -m /Games/Tests% --prefix Interpreted."

  # /Games/Tests% compiled
  stdio $::out/pkgs/games/Games/games-tests "/Games/Tests%" "games app"
  testsuite $::out/pkgs/games/Games/games-tests.tr $::out/pkgs/games/Games/games-tests \
    "$::out/pkgs/games/Games/games-tests --prefix Compiled."
}
