namespace eval "pkgs/games" {
  pkg games ""

  # /Games/Tests% interpreted
  test $::out/pkgs/games/Games/tests.tr "$::out/pkgs/core/Core/fble-stdio $::out/pkgs/games/Games/Tests.fble.d" \
    "$::out/pkgs/core/Core/fble-stdio [exec pkg-config --cflags-only-I fble-games] -m /Games/Tests%" "pool = console"

  # /Games/Tests% compiled
  stdio $::out/pkgs/games/Games/games-tests "/Games/Tests%" "fble-games" \
    "$::out/pkgs/core/libfble-core.a $::out/pkgs/app/libfble-app.a $::out/pkgs/games/libfble-games.a"
  test $::out/pkgs/games/Games/games-tests.tr $::out/pkgs/games/Games/games-tests \
    "$::out/pkgs/games/Games/games-tests" "pool = console"
}
