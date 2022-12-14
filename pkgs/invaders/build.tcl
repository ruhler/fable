namespace eval "pkgs/invaders" {
  pkg invaders [list core app] ""

  # /Invaders/Tests% interpreted
  set cflags "-I $::s/pkgs/invaders -I $::s/pkgs/app -I $::s/pkgs/core"
  test $::b/pkgs/invaders/tests-interpreted.tr "$::b/pkgs/core/fble-stdio $::b/pkgs/invaders/Invaders/Tests.fble.d" \
    "$::b/pkgs/core/fble-stdio $cflags -m /Invaders/Tests%"

  # /Invaders/Tests% compiled
  stdio $::b/pkgs/invaders/invaders-tests "/Invaders/Tests%" "invaders app"
  test $::b/pkgs/invaders/tests-compiled.tr $::b/pkgs/invaders/invaders-tests \
    "$::b/pkgs/invaders/invaders-tests"

  app $::b/pkgs/invaders/fble-invaders "/Invaders/AppIO%" "invaders"
  install_bin $::b/pkgs/invaders/fble-invaders
}
