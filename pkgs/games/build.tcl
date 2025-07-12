namespace eval "pkgs/games" {
  pkg games [list core app] "" ""

  # /Games/Tests% interpreted
  set cflags "-I $::s/pkgs/games -I $::s/pkgs/app -I $::s/pkgs/core"
  run_stdio_tests $::b/pkgs/games/Games/tests.tr "$cflags -m /Games/Tests%" ""

  # /Games/Tests% compiled
  # --allow-shlib-undefined because we know this doesn't use the part of the
  # app package that depends on SDL.
  stdio $::b/pkgs/games/Games/games-tests "/Games/Tests%" "app games" "-Wl,--allow-shlib-undefined"
  testsuite $::b/pkgs/games/Games/games-tests.tr $::b/pkgs/games/Games/games-tests \
    "$::b/pkgs/games/Games/games-tests --prefix Compiled."

  # --allow-shlib-undefined because we know this doesn't use the part of the
  # app package that depends on SDL.
  stdio $::b/pkgs/games/reverse-complement \
    "/Games/BenchmarkGame/ReverseComplement/Stdio%" "games app" "-Wl,--allow-shlib-undefined"
}
