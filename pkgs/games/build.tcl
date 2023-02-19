namespace eval "pkgs/games" {
  dist_s $::s/pkgs/games/build.tcl

  pkg games [list core app] ""

  # /Games/Tests% interpreted
  set cflags "-I $::s/pkgs/games -I $::s/pkgs/app -I $::s/pkgs/core"
  testsuite $::b/pkgs/games/Games/tests.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/games/Games/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Games/Tests% --prefix Interpreted."

  # /Games/Tests% compiled
  stdio $::b/pkgs/games/Games/games-tests "/Games/Tests%" "games app"
  testsuite $::b/pkgs/games/Games/games-tests.tr $::b/pkgs/games/Games/games-tests \
    "$::b/pkgs/games/Games/games-tests --prefix Compiled."

  stdio $::b/pkgs/games/reverse-complement \
    "/Games/BenchmarkGame/ReverseComplement/Stdio%" "games app"
}
