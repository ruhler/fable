namespace eval "pkgs/games" {
  pkg games [list std core app] "" ""

  # /Games/Tests% interpreted
  set cflags "-I $::s/pkgs/games -I $::s/pkgs/app -I $::s/pkgs/std -I $::s/pkgs/core"
  run_cli_tests $::b/pkgs/games/Games/tests.tr "$cflags -m /Games/Tests%" ""

  # /Games/Tests% compiled
  # --allow-shlib-undefined because we know this doesn't use the part of the
  # app package that depends on SDL.
  cli $::b/pkgs/games/Games/games-tests "/Games/Tests%" "app games" "-Wl,--allow-shlib-undefined"
  testsuite $::b/pkgs/games/Games/games-tests.tr $::b/pkgs/games/Games/games-tests \
    "$::b/pkgs/games/Games/games-tests --prefix Compiled."

  # --allow-shlib-undefined because we know this doesn't use the part of the
  # app package that depends on SDL.
  cli $::b/pkgs/games/reverse-complement \
    "/Games/BenchmarkGame/ReverseComplement/Cli%" "games app" "-Wl,--allow-shlib-undefined"
}
