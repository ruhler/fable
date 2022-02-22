namespace eval "pkgs/games" {
  pkg games [list core app] ""

  # /Games/Tests% interpreted
  set cflags "-I pkgs/games -I pkgs/app -I pkgs/core"
  test $::out/pkgs/games/Games/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/games/Games/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio $cflags -m /Games/Tests%" "pool = console"

  # /Games/Tests% compiled
  stdio $::out/pkgs/games/Games/games-tests "/Games/Tests%" "fble-games" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/games/libfble-games.a"
  test $::out/pkgs/games/Games/games-tests.tr $::out/pkgs/games/Games/games-tests \
    "$::out/pkgs/games/Games/games-tests" "pool = console"
}
